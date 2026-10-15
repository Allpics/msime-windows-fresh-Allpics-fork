#pragma once

// 模拟输入：按全局热键把配置里的预置文本逐字符注入键盘事件。
//
// 通道是 SendInput + KEYEVENTF_UNICODE，与输入法自身的 TSF 通道无关：这里不碰
// composition、不碰 IPC、也不碰剪贴板，前台窗口收到的是普通按键消息。因此本模块
// 只需要在 Server 进程里工作，windows/ 的 TSF 前端一行都不用改。
//
// 调用方是 window_hook.cpp 里的低级键盘钩子（设置改动则由配置写入路径触发重载）。
// 钩子运行在它自己的线程上，所以这里对外的状态一律是原子的。

namespace AutoType
{
// 若当前按键恰好是本功能的热键，返回 true，调用方应当吃掉这次按键（不再传给前台应用）。
// 已经在键入时，同样的热键表示中断：置位中止标志并返回 true。
// 关闭或未配置热键时恒返回 false，不会影响任何按键。
bool HandleHotkey(unsigned int vk_code);

// 键入是否正在进行。供别处判断本功能是否占着注入通道。
bool IsTyping();

// 配置重载后调用：把热键与文本的快照刷新一遍。正在进行的键入不打断。
void Reload();

// 进程退出前收尾：中止进行中的键入并等待工作线程结束。
void Shutdown();
} // namespace AutoType
