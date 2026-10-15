// 模拟输入（[autotype] 段）的配置读写。
//
// 这里的写入路径与其它配置段一致：先落盘、成功后才更新内存值，然后让 AutoType 模块重新取快照。
// 读 config 的是 window_hook.cpp 里的低级键盘钩子，但钩子读的不是这里的全局，而是
// AutoType::Reload() 取出的快照（见 autotype/auto_typer.h），所以本文件不需要再加锁。
#include "config/ime_config_internal.h"

#include <algorithm>
#include <string>

#include "autotype/auto_typer.h"

using namespace ime_config_detail;

namespace ime_config_detail
{
// ime_config.cpp 的 LoadImeConfig 读完配置后调用一次。段不存在时落到"关闭 + 热键未配置"，
// 这正是老配置文件（没有 [autotype] 段）应当得到的状态。
void LoadAutoTypeConfig(const toml::table &tbl)
{
    g_autotype_enabled = tbl["autotype"]["enabled"].value_or(false);

    std::string hotkey = tbl["autotype"]["hotkey"].value_or(std::string());
    g_autotype_hotkey = hotkey.size() > 128 ? std::string() : hotkey;

    // 统一成 LF：注入侧只需处理一种换行写法。
    const std::string text = tbl["autotype"]["text"].value_or(std::string());
    g_autotype_text.clear();
    g_autotype_text.reserve(text.size());
    for (const char ch : text)
    {
        if (ch != '\r')
        {
            g_autotype_text.push_back(ch);
        }
    }

    g_autotype_send_enter = tbl["autotype"]["send_enter"].value_or(true);
    g_autotype_char_delay_ms = std::clamp(tbl["autotype"]["char_delay_ms"].value_or(30), 0, 5000);

    AutoType::Reload();
}
} // namespace ime_config_detail

bool GetConfiguredAutoTypeEnabled()
{
    return g_autotype_enabled;
}

bool SetConfiguredAutoTypeEnabled(bool enabled)
{
    if (!WriteConfiguredValue("autotype", "enabled", enabled ? "true" : "false"))
    {
        return false;
    }
    g_autotype_enabled = enabled;
    AutoType::Reload();
    return true;
}

const std::string &GetConfiguredAutoTypeHotkey()
{
    return g_autotype_hotkey;
}

const std::string &GetConfiguredAutoTypeText()
{
    return g_autotype_text;
}

bool SetConfiguredAutoTypeHotkey(const std::string &hotkey)
{    if (!WriteConfiguredValue("autotype", "hotkey", EscapeTomlBasicString(hotkey)))
    {
        return false;
    }
    g_autotype_hotkey = hotkey;
    AutoType::Reload();
    return true;
}

bool SetConfiguredAutoTypeText(const std::string &text)
{
    if (!WriteConfiguredValue("autotype", "text", EscapeTomlBasicString(text)))
    {
        return false;
    }
    g_autotype_text = text;
    AutoType::Reload();
    return true;
}

bool GetConfiguredAutoTypeSendEnter()
{
    return g_autotype_send_enter;
}

bool SetConfiguredAutoTypeSendEnter(bool enabled)
{
    if (!WriteConfiguredValue("autotype", "send_enter", enabled ? "true" : "false"))
    {
        return false;
    }
    g_autotype_send_enter = enabled;
    AutoType::Reload();
    return true;
}

int GetConfiguredAutoTypeCharDelayMs()
{
    return g_autotype_char_delay_ms;
}

bool SetConfiguredAutoTypeCharDelayMs(int milliseconds)
{
    // 先校验后落盘：越界值不写文件也不动内存值，设置页的请求会被静默拒绝。
    if (milliseconds < 0 || milliseconds > 5000)
    {
        return false;
    }
    if (!WriteConfiguredValue("autotype", "char_delay_ms", std::to_string(milliseconds)))
    {
        return false;
    }
    g_autotype_char_delay_ms = milliseconds;
    AutoType::Reload();
    return true;
}
