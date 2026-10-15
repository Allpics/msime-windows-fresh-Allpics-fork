// 模拟输入：按全局热键把预置文本逐字符注入键盘事件。设计说明见 auto_typer.h。
#include "auto_typer.h"

#include "config/ime_config.h"
#include "config/ime_config_internal.h"
#include "utils/common_utils.h"
#include "window/window_hook.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
// 一次键入最多处理的字符数。预置文本是配置里的手写内容，不是数据文件，给一个宽松但有限的上限，
// 避免误粘一大段文本后把注入通道占住几分钟还只能靠中断退出。
constexpr size_t kMaxTextLength = 20000;

struct HotkeySpec
{
    bool valid = false;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool win = false;
    unsigned int vk = 0;
};

struct AutoTypeConfig
{
    bool enabled = false;
    HotkeySpec hotkey;
    std::wstring text;
    bool send_enter = true;
    int char_delay_ms = 30;
};

std::mutex g_config_mutex;
AutoTypeConfig g_config;

std::atomic<bool> g_typing{false};
std::atomic<bool> g_abort{false};

std::mutex g_worker_mutex;
std::thread g_worker;

// 设置进程（MetasequoiaImeSettings.exe）同样编译并链接本模块——它复用 config/ 下的全部
// 配置读写——但那里没有键盘钩子，也不该起注入线程。只有 Server 会安装 g_hHook
// （window/ime_windows.cpp），而该变量在每个进程里各自独立，因此用它区分两者：
// 钩子还没装上时本模块完全是空操作，设置进程里的 Reload() 与热键调用都会直接返回。
bool ActiveInThisProcess()
{
    return g_hHook != nullptr;
}

std::string TrimAscii(const std::string &text)
{
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    auto begin = std::find_if(text.begin(), text.end(), not_space);
    auto end = std::find_if(text.rbegin(), text.rend(), not_space).base();
    return begin < end ? std::string(begin, end) : std::string();
}

std::string ToLowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

// 主键名 → 虚拟键。词表刻意写得窄而明确：写错的名字解析失败等于热键未配置，
// 比悄悄绑到一个用户没想要的键要安全。
bool ParseMainKey(const std::string &name, unsigned int &vk)
{
    if (name.size() == 1)
    {
        const char ch = name[0];
        if (ch >= 'a' && ch <= 'z')
        {
            vk = static_cast<unsigned int>(std::toupper(static_cast<unsigned char>(ch)));
            return true;
        }
        if (ch >= '0' && ch <= '9')
        {
            vk = static_cast<unsigned int>(ch);
            return true;
        }
        return false;
    }

    if (name.size() >= 2 && name[0] == 'f')
    {
        const std::string digits = name.substr(1);
        if (std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
        {
            const int index = std::atoi(digits.c_str());
            if (index >= 1 && index <= 24)
            {
                vk = static_cast<unsigned int>(VK_F1) + static_cast<unsigned int>(index - 1);
                return true;
            }
        }
        return false;
    }

    struct NamedKey
    {
        const char *name;
        unsigned int vk;
    };
    static const NamedKey kNamedKeys[] = {
        {"space", VK_SPACE},         {"enter", VK_RETURN},          {"return", VK_RETURN},
        {"tab", VK_TAB},             {"escape", VK_ESCAPE},          {"esc", VK_ESCAPE},
        {"backspace", VK_BACK},      {"delete", VK_DELETE},          {"insert", VK_INSERT},
        {"home", VK_HOME},           {"end", VK_END},                {"pageup", VK_PRIOR},
        {"pagedown", VK_NEXT},       {"up", VK_UP},                  {"down", VK_DOWN},
        {"left", VK_LEFT},           {"right", VK_RIGHT},            {"minus", VK_OEM_MINUS},
        {"equal", VK_OEM_PLUS},      {"comma", VK_OEM_COMMA},        {"period", VK_OEM_PERIOD},
        {"slash", VK_OEM_2},         {"backslash", VK_OEM_5},        {"semicolon", VK_OEM_1},
        {"quote", VK_OEM_7},         {"backquote", VK_OEM_3},        {"bracketleft", VK_OEM_4},
        {"bracketright", VK_OEM_6},
    };
    for (const auto &key : kNamedKeys)
    {
        if (name == key.name)
        {
            vk = key.vk;
            return true;
        }
    }
    return false;
}

// "ctrl+shift+alt+v" → 修饰键集合 + 主键。要求至少一个修饰键加一个主键：
// 不带修饰键的热键会把普通输入整个吃掉，不能让它从配置里被写出来。
HotkeySpec ParseHotkey(const std::string &raw)
{
    HotkeySpec spec;
    const std::string text = ToLowerAscii(TrimAscii(raw));
    if (text.empty())
    {
        return spec;
    }

    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size())
    {
        const size_t pos = text.find('+', start);
        if (pos == std::string::npos)
        {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }

    spec.valid = true;
    for (size_t index = 0; index < parts.size(); ++index)
    {
        const std::string part = TrimAscii(parts[index]);
        if (part.empty())
        {
            spec.valid = false;
            return spec;
        }
        const bool is_last = index + 1 == parts.size();
        const bool is_modifier =
            part == "ctrl" || part == "control" || part == "shift" || part == "alt" || part == "win";
        if (is_modifier)
        {
            if (is_last)
            {
                spec.valid = false;
                return spec;
            }
            if (part == "ctrl" || part == "control")
                spec.ctrl = true;
            else if (part == "shift")
                spec.shift = true;
            else if (part == "alt")
                spec.alt = true;
            else
                spec.win = true;
            continue;
        }
        if (!is_last)
        {
            // 主键只能出现在最后一段，中间出现非修饰键说明写法不对。
            spec.valid = false;
            return spec;
        }
        if (!ParseMainKey(part, spec.vk))
        {
            spec.valid = false;
            return spec;
        }
    }

    if (spec.vk == 0 || (!spec.ctrl && !spec.shift && !spec.alt && !spec.win))
    {
        spec.valid = false;
    }
    return spec;
}

// 恰好按住了配置要求的修饰键组合。多按了别的修饰键就不算命中，避免和系统或别的软件的热键抢。
// 这里直接用 GetAsyncKeyState 而不是 window_hook.cpp 的 IsKeyPressed：后者只在 Server 的
// 目标里编译，而本文件同时属于设置进程。只有 Server 会走到这里（见 ActiveInThisProcess）。
bool ModifiersMatch(const HotkeySpec &spec)
{
    const auto pressed = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };

    if (pressed(VK_CONTROL) != spec.ctrl)
        return false;
    if (pressed(VK_SHIFT) != spec.shift)
        return false;
    if (pressed(VK_MENU) != spec.alt)
        return false;
    const bool win = pressed(VK_LWIN) || pressed(VK_RWIN);
    return win == spec.win;
}

bool SendKey(unsigned int vk)
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = static_cast<WORD>(vk);
    inputs[0].ki.dwExtraInfo = GetMessageExtraInfo();
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool SendUnicodeChar(wchar_t ch)
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = static_cast<WORD>(ch);
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
    inputs[0].ki.dwExtraInfo = GetMessageExtraInfo();
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags |= KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

// 可被中断的等待：分片睡眠，避免 0 间隔时把 CPU 占满，也避免长间隔时中断要等很久。
void SleepInterruptible(int milliseconds)
{
    int remaining = milliseconds;
    while (remaining > 0 && !g_abort.load(std::memory_order_relaxed))
    {
        const int step = std::min(10, remaining);
        ::Sleep(static_cast<DWORD>(step));
        remaining -= step;
    }
}

bool SendText(const std::wstring &text, bool send_enter, int char_delay_ms)
{
    for (size_t index = 0; index < text.size(); ++index)
    {
        if (g_abort.load(std::memory_order_relaxed))
        {
            return false;
        }

        const wchar_t ch = text[index];
        if (ch == L'\r')
        {
            // CRLF 已经由加载侧规范成 LF，这里只处理落单的 CR。
            if (send_enter)
                SendKey(VK_RETURN);
            else
                SendUnicodeChar(L'\r');
        }
        else if (ch == L'\n')
        {
            if (send_enter)
                SendKey(VK_RETURN);
            else
                SendUnicodeChar(L'\n');
        }
        else
        {
            SendUnicodeChar(ch);
        }

        if (char_delay_ms > 0 && index + 1 < text.size())
        {
            SleepInterruptible(char_delay_ms);
        }
    }
    return !g_abort.load(std::memory_order_relaxed);
}

void JoinWorkerIfFinished()
{
    if (g_worker.joinable() && !g_typing.load(std::memory_order_relaxed))
    {
        g_worker.join();
    }
}

void StartTyping(const AutoTypeConfig &config)
{
    std::lock_guard<std::mutex> lock(g_worker_mutex);
    // 上一轮的线程可能还没被回收，先收掉；此时 g_typing 已经是 false。
    if (g_worker.joinable())
    {
        g_worker.join();
    }

    g_abort.store(false, std::memory_order_relaxed);
    g_typing.store(true, std::memory_order_relaxed);

    const std::wstring text = config.text;
    const bool send_enter = config.send_enter;
    const int char_delay_ms = config.char_delay_ms;
    g_worker = std::thread([text, send_enter, char_delay_ms]() {
        (void)SendText(text, send_enter, char_delay_ms);
        g_typing.store(false, std::memory_order_relaxed);
    });
}
} // namespace

void AutoType::Reload()
{
    // 注意：本函数可能在键盘钩子安装之前就被配置加载调用（main.cpp 里 InitImeConfig 早于
    // CreateCandidateWindow）。因此这里不能用 ActiveInThisProcess() 提前返回，否则启动时读到的
    // 那份配置永远进不了快照，热键会静默失效；钩子装好后 ime_windows.cpp 会再调一次本函数。
    AutoTypeConfig next;
    next.enabled = GetConfiguredAutoTypeEnabled();
    next.hotkey = ParseHotkey(ime_config_detail::g_autotype_hotkey);
    next.send_enter = GetConfiguredAutoTypeSendEnter();
    next.char_delay_ms = std::clamp(GetConfiguredAutoTypeCharDelayMs(), 0, 5000);
    next.text = string_to_wstring(ime_config_detail::g_autotype_text);
    if (next.text.size() > kMaxTextLength)
    {
        next.text.resize(kMaxTextLength);
    }

    std::lock_guard<std::mutex> lock(g_config_mutex);
    g_config = std::move(next);
}

bool AutoType::IsTyping()
{
    return g_typing.load(std::memory_order_relaxed);
}

bool AutoType::HandleHotkey(unsigned int vk_code)
{
    if (!ActiveInThisProcess())
    {
        return false;
    }

    AutoTypeConfig config;
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        config = g_config;
    }

    if (!config.enabled || !config.hotkey.valid || config.hotkey.vk != vk_code)
    {
        return false;
    }

    // 修饰键必须完全吻合，否则一律放行。
    //
    // 这一步必须放在任何"吃掉按键"的分支之前：主键相同但修饰键不同是很常见的组合
    // （按一下 v、Ctrl+V、Shift+V…），一旦在这之前吞掉按键，正常输入就会被拦下来。
    if (!ModifiersMatch(config.hotkey))
    {
        return false;
    }

    // 已经在键入：同一个热键表示中断。耗时的注入在工作线程里，钩子线程只做置位，不会被拖住。
    if (g_typing.load(std::memory_order_relaxed))
    {
        g_abort.store(true, std::memory_order_relaxed);
        return true;
    }

    JoinWorkerIfFinished();

    if (config.text.empty())
    {
        // 热键配好了但还没写文本：不吞按键，让它照常送到前台程序。
        // 静默吃掉一个组合键会让用户以为输入法坏了，而这里本来就没有可做的事。
        return false;
    }

    StartTyping(config);
    return true;
}

void AutoType::Shutdown()
{
    g_abort.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_worker_mutex);
    if (g_worker.joinable())
    {
        g_worker.join();
    }
    g_typing.store(false, std::memory_order_relaxed);
}
