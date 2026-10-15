#!/usr/bin/env node
// 运行设置页的契约同步脚本。
//
// 为什么不让 package.json 直接调 python3：
//   Windows 上 `python3` 通常解析到 Microsoft Store 的存根程序
//   （%LOCALAPPDATA%\Microsoft\WindowsApps\python3.exe），它不执行任何东西就退出，
//   于是 `pnpm build` 的 prebuild 会以 9009 失败，本地构建第一步就断。
//   Linux/macOS 上 `python3` 才是正确的名字，所以不能反过来写死 `python`。
//
// 依次尝试 python / python3，取第一个能真正执行解释器的。可用 MSIME_PYTHON 显式指定。
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

// 必须用 fileURLToPath：直接读 import.meta.url 的 pathname 在 Windows 上会得到
// "/C:/..." 这种带前导斜杠的形式，Python 会把它当成 C:\C:\... 而报 Errno 22。
// 目标脚本在 ui-html/scripts/：本文件位于 ui-html/webview2/settings/ime-settings/scripts/，
// 所以要上溯四层到 ui-html/，再进 scripts/。
const script = fileURLToPath(new URL('../../../../scripts/sync-contracts.py', import.meta.url));
const args = [script, ...process.argv.slice(2)];

const candidates = process.env.MSIME_PYTHON
  ? [process.env.MSIME_PYTHON]
  : ['python', 'python3'];

let lastError = '';
for (const command of candidates) {
  // 先确认这个命令真的是一支解释器：Store 存根会以 9009 退出且什么都不做。
  const probe = spawnSync(command, ['-c', 'import sys; print(sys.version_info[0])'], {
    encoding: 'utf8',
    shell: false
  });
  if (probe.error || probe.status !== 0 || (probe.stdout || '').trim() !== '3') {
    lastError = probe.error ? probe.error.message : `exit ${probe.status}`;
    continue;
  }
  const result = spawnSync(command, args, { stdio: 'inherit', shell: false });
  if (result.error) {
    lastError = result.error.message;
    continue;
  }
  process.exit(result.status ?? 1);
}

console.error(
  '[sync-contracts] 找不到可用的 Python 3，已尝试：' + candidates.join(', ') + '\n' +
  '  最后一次失败：' + lastError + '\n' +
  '  请安装 Python 3.10+ 并确保 `python` 在 PATH 中，或用 MSIME_PYTHON 指定解释器路径。'
);
process.exit(1);
