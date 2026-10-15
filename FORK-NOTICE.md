# 来源、修改声明与许可

## 原作

本仓库是 **水杉输入法 Windows 端**的一个修改版本。原始作品与全部上游代码来自：

| 项目 | 信息 |
|---|---|
| 上游仓库 | [metasequoiaime/msime-windows](https://github.com/metasequoiaime/msime-windows) |
| 上游官网 | <https://msime.app> |
| 上游默认分支 | `develop` |
| 上游开源协议 | **GPL-3.0**（见仓库根 [LICENSE](LICENSE)，上游 `README.md` 的「开源协议」一节亦声明 GPL-3.0） |

**原作的著作权归上游作者与贡献者所有。** 本仓库的维护者不是原作者，与上游项目方没有隶属关系；
本仓库不代表上游项目，也不应被当作上游发布的版本。

## 许可证

本仓库继续以 **GPL-3.0** 分发，`LICENSE` 保持为上游原件、未作修改。任何再分发本仓库或其衍生作品，
都必须继续遵守 GPL-3.0（包括保留版权声明与许可声明、提供对应源码）。

GPL-3.0 不提供任何担保（见 `LICENSE` 第 15、16 条）。本仓库的修改由本仓库维护者自行提供，
**同样不提供任何担保**；因使用本仓库产物造成的损失，上游作者不承担任何责任。

## 修改声明（GPL-3.0 § 5(a)）

依据 GPL-3.0 第 5(a) 条，此处**显著声明本仓库改动了原作的源文件，并注明修改日期**。

| 项 | 值 |
|---|---|
| 基线提交 | `242f8b2bc881cabf9dfb53905c0370fc9e99248c`（`242f8b2b`，2026-10-12，上游 `develop`） |
| 基线标题 | `fix(ui-html): 词库、删除确认框与凭据测试的异步问题跟进修复 (#904)` |
| 修改日期 | **2026-10-13 至 2026-10-15** |
| 修改者 | 本仓库维护者（Allpics） |
| 分发许可 | GPL-3.0（与上游一致） |

本仓库**不是 GitHub 的 fork 关系仓库**，而是基于上述提交独立创建的仓库；
两者共享同一段上游历史，但本仓库的改动未提交给上游，上游也未包含这些改动。

### 改动范围

**修改的上游文件（20 个）**

```
installer/default_config/config.default.toml
installer/msime_setup.iss
server/AGENTS.md
server/CMakeLists.txt
server/assets/config/config.toml
server/scripts/lcompile-release.ps1
server/src/config/ime_config.cpp
server/src/config/ime_config.h
server/src/config/ime_config_internal.h
server/src/main.cpp
server/src/settings/settings_app.cpp
server/src/window/ime_windows.cpp
server/src/window/window_hook.cpp
server/tests/CMakeLists.txt
ui-html/webview2/settings/ime-settings/package.json
ui-html/webview2/settings/ime-settings/src/modules/config-sync.ts
ui-html/webview2/settings/ime-settings/src/modules/shortcut.ts
ui-html/webview2/settings/ime-settings/src/modules/sidebar.ts
ui-html/webview2/settings/ime-settings/src/partials/shortcut.html
ui-html/webview2/settings/ime-settings/src/styles/modules/shortcut.css
```

**新增的文件**

```
server/src/autotype/auto_typer.h
server/src/autotype/auto_typer.cpp
server/src/config/ime_config_autotype.cpp
ui-html/webview2/settings/ime-settings/scripts/sync-contracts.mjs
INSTALL.md
docs/fork-release.md
autotype-test.html
docs/autotype-test-preview.png
```

### 改动的性质

1. **新增功能：模拟输入（按键注入）**
   在 Server 进程内新增模块：按用户配置的全局热键，用 `SendInput` + `KEYEVENTF_UNICODE`
   把预置文本逐字符键入当前窗口；可在设置页配置热键与文本，配置存在 `config.toml` 的
   `[autotype]` 段。**不修改 TSF DLL，不修改 IPC 契约，不使用剪贴板。**

2. **修复本地构建障碍**（不为新增功能服务，但影响本仓库可用性）
   - `server/scripts/lcompile-release.ps1` 原先无条件注入 `uiAccess="true"` 清单。
     该清单只在二进制拥有受信任签名时才生效，未签名时会导致 Server 无法启动。
     已移除该注入（上游发布路径本就不经过此脚本）。
   - 设置页 `prebuild` 原先直接调用 `python3`，在 Windows 上会命中 Microsoft Store 存根并失败。
     改为跨平台包装 `scripts/sync-contracts.mjs`。
   - 设置页预置文本框原先只在失焦时保存，改为输入防抖 + 失焦补存。

3. **新增文档**：`INSTALL.md`（本地编译与安装说明）、`docs/fork-release.md`（版本号与发行说明）、
   `autotype-test.html`（功能验证页）。

### 未改动的部分

- `LICENSE`、`THIRD_PARTY_NOTICES.txt` 及各子目录的 `NOTICE.md` / `COPYING*` **保持原样**。
- `engine/`、`ui/`、`windows/` 的既有源码**未作修改**。
- 第三方组件（`vendor/opencc`、`vendor/cpp-pinyin`、引擎内的 kenlm/octagram/utfcpp 等）
  仍各自遵循其原有许可，详见 `THIRD_PARTY_NOTICES.txt` 与各 `NOTICE.md`。

## 关于本仓库的产物

本仓库**不发布成品安装包**，也不对产物做代码签名。需要的用户按 [INSTALL.md](INSTALL.md)
在本机自行编译。由此产生的两点限制已在 `INSTALL.md` 顶部写明：

- 未签名 → 安装时会看到"未知发布者"，且 Server 的 `uiAccess` 不生效
- 因此**模拟输入无法注入到"以管理员身份运行"的窗口**（普通窗口正常）

如果你需要签名版本、或希望功能进入上游，请使用[上游项目](https://github.com/metasequoiaime/msime-windows)的渠道。
