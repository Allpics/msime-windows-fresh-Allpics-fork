# Fork 发行说明（自动输入分支）

本文件记录本 fork 的**版本号方案**、以及**签名/发行的决策与依据**。
面向使用者的编译与安装步骤在仓库根的 **[INSTALL.md](../INSTALL.md)**。

上游基线：`metasequoiaime/msime-windows`，本地分支 `feat/autotype-hotkey`，HEAD 基于 `242f8b2b`。

> 本文件的内容**没有提交、没有推送**。是否推送、推到哪个远端，由你决定。

## 当前决策：不签名，改为「用户本地编译」

**已定方向**：本分支**不做代码签名，也不发布成品安装包**。需要的用户按
[INSTALL.md](../INSTALL.md) 在本机自行编译。

由此产生、并已向用户写明的两个边界：

1. **不注入 `uiAccess` 清单** —— 已落进代码：`server/scripts/lcompile-release.ps1`
   原先无条件注入该清单，会让本地构建的 Server 启动失败，现已移除这一步。
   发布路径不受影响 —— `release.yml` 本来就用原始 `cmake` 编译，测试通过后才由
   `scripts/ci/embed-server-manifest.ps1` 单独注入（`release.yml:413-415`）。
2. **模拟输入无法注入管理员权限窗口** —— 普通窗口完全正常

同时剔除的其它本地构建障碍见 [INSTALL.md](../INSTALL.md) 第六节。

下面第 2、3 节保留为**背景依据**，也是将来若重启签名路线的现成参考。

---

## 1. 版本号方案

你定的格式是 **`0.7.1.f1.a1`**，含义：

| 段 | 含义 | 示例 |
|---|---|---|
| `0.7.1` | 上游主线版本基底 | 跟随上游发布 |
| `f1` | 分支代号 + 功能序号（`f` = 自动输入 fork） | 下一个独立功能可以是 `f2` |
| `a1` | 该功能第几次发版，字母序递增 | `a2`、`a3` … `a9`、`b1`… |

### 各处的落值与约束来源

版本号不能只写在一个地方——四个位置有各自的硬约束，实测结论如下：

| 位置 | 写什么 | 约束来源（已实测） |
|---|---|---|
| `version.txt` | `0.7.1` | **必须 `MAJOR.MINOR.PATCH` 三段**。`scripts/apply_version.py:48` 用 `SEMVER` 正则校验，写 `0.7.1.1` 或带字母**会直接报错退出** |
| PE 版本资源 `FILEVERSION` / `FileVersion` | `0.7.1.0` | **Windows 要求 4 段纯数字**，不能有字母 |
| **安装包文件名** | `MetasequoiaIME_Setup_v0.7.1.f1.a1.exe` | 无限制 ✅ |
| **`AppVersion`（显示名）** | `0.7.1.f1.a1` | 无限制 ✅ |
| **`ProductVersion`（字符串字段）** | `0.7.1.f1.a1` | 无限制（是字符串，不进数字比对）✅ |
| 版本目录 `msime_v*` | `msime_v0.7.1` | 保持纯数字：升级逻辑按 `.1`/`.2` 轮转后缀，掺字母会让既有升级路径变复杂 |

### 已落地的改动

`installer/msime_setup.iss` 头部新增两个宏，把"数字基底"和"显示版本"分开：

```innosetup
#define MyAppVersion     "0.0.1"          ; 数字基底，打包脚本用 /DMyAppVersion=0.7.1 覆盖
#define MyForkTag        ".f1.a1"         ; ← 发新版只改这一行
#define MyDisplayVersion MyAppVersion + MyForkTag

AppVersion={#MyDisplayVersion}                              ; 显示 0.7.1.f1.a1
OutputBaseFilename=MetasequoiaIME_Setup_v{#MyDisplayVersion} ; 文件名带分支标记
VersionInfoVersion={#MyAppVersion}                          ; 保持纯数字
#define MyVersionDirBase "msime_v" + MyAppVersion           ; 版本目录保持纯数字
```

**发下一个版本时**：只改 `MyForkTag`（`a1` → `a2`）。`MyAppVersion` 由打包脚本按上游版本注入，不用手改。

### 实测验证结果

用探针脚本验证过展开结果与 PE 资源：

```
文件名          MetasequoiaIME_Setup_v0.7.1.f1.a1.exe   ← 你的方案 ✅
FileVersion     0.7.1                                    ← 纯数字，Windows 可比对 ✅
ProductVersion  0.7.1.f1.a1                              ← 分支标记保留 ✅
```

### 还没做（需要你决定）

`MyForkTag` 目前是**硬编码**在 `.iss` 里的。注意 `Prepare-PackageFiles.ps1` 会**原地改写** `.iss` 的 `MyAppVersion`（这是它的既有设计，我用完都会 `git checkout` 还原）。

如果你想让发版更省事，我可以再做一步：新增 `fork-version.json`（唯一真源），由 `Compile-Installer.ps1` 读取并展开成 `/DMyForkTag=...`。要的话说一声。

---

## 2. 发行的两个前提问题

### 2.1 签名：这是 uiAccess 能否生效的唯一路径

现状（实测）：

| 项 | 值 |
|---|---|
| `.iss` | `PrivilegesRequired=admin` ✅ |
| 安装包清单 | `asInvoker`（**Inno Setup 6 的正常行为**：运行时用 `runas` 提权，不写 `requireAdministrator`） |
| 安装包签名 | **NotSigned** |
| server 清单 | `uiAccess="true"` + `asInvoker` |

**关键因果**：`uiAccess="true"` 要求 **Authenticode 受信任签名**才算数（见 `server/AGENTS.md` 与
`scripts/ci/detect-release-signing.ps1:3` 的注释）。当前用的是本机测试证书（未入信任存储），
所以：

- **安装器**：能装（Inno 运行时提权会弹 UAC），但会触发 SmartScreen「未知发布者」
- **server**：`uiAccess` 不生效 → 永远是中完整性 → **注入不进管理员权限的窗口**
- 更糟的组合：`uiAccess=true` + 不受信任签名会让 watchdog 的 `ShellExecuteEx` 启动失败
  （详见 `server/src/watchdog/main.cpp:126-141` 的注释）

**结论：没有受信任签名，"自己申请管理员权限"这件事无法真正解决。**

### 2.2 ⚠️ 一个必须先告诉你的申请风险

SignPath Foundation 的条款里有一条 **"No hacking tools"**（[terms.html](https://signpath.org/terms.html)）：

> Software must not include features designed to identify or exploit security vulnerabilities or
> **circumvent security measures of their execution environment**.

本 fork 的核心功能是"**按键注入，绕过网页的禁止粘贴限制**"。这**可能**被审核方视为
"绕过执行环境的安全措施"，从而**拒绝签发**。

我无法替你判断他们怎么认定，但你应该在申请时**主动说明**，而不是等他们发现：

- 它不利用漏洞、不提权、不注入其它进程内存
- 走的是与真实输入法完全相同的 `SendInput` 通道，行为等价于用户手动打字
- 用途是让用户把**自己**的内容输入进**自己**打开的页面

**这一条是整个计划的最大不确定性**，建议先按下面的清单提交申请，拿到明确答复再投入更多工作。

---

## 3. SignPath 接入清单

### 好消息：代码侧已经全部就位

仓库里 SignPath 的接线**本来就是完整的**（上游已支持），fork 后主要是**账号配置**，不需要改代码：

| 组件 | 位置 | 状态 |
|---|---|---|
| 申报/上传/回写三步 | `.github/workflows/release.yml`（`signpath-payload` / `signpath-installer` 两组） | ✅ 已有 |
| 文件暂存与回填 | `scripts/ci/signpath-files.ps1`（`-Mode Stage/Restore`、`-Set Payload/Installer`） | ✅ 已有 |
| 签名模式判定 | `scripts/ci/detect-release-signing.ps1` | ✅ 已有，且**半配置会直接失败**而不是静默退回未签名 |
| artifact 配置副本 | `installer/signpath/payload.xml`、`installer/signpath/installer.xml` | ⚠️ 缺元数据限制，见下 |

### 需要你在 SignPath 后台做的事

1. 用 GitHub 账号在 [signpath.org/apply](https://signpath.org/apply) 提交项目申请（选 Foundation 免费证书路径）
2. 项目 slug 建议 `msime-windows-fork`（或你定的名字）
3. 建两个 **artifact configuration**，**内容照抄** `installer/signpath/payload.xml` 与 `installer/signpath/installer.xml`
4. 建一个 **signing policy**（例如 `release`），勾上"每次签名请求需要人工审批"

### 需要在 GitHub 仓库配置的变量与密钥

| 名称 | 类型 | 值 |
|---|---|---|
| `WINDOWS_SIGNING_PROVIDER` | 变量 | `signpath` |
| `SIGNPATH_ORGANIZATION_ID` | 变量 | SignPath 后台的 Organization ID |
| `SIGNPATH_PROJECT_SLUG` | 变量 | 上面建的项目 slug |
| `SIGNPATH_SIGNING_POLICY_SLUG` | 变量 | 上面建的 policy slug |
| `SIGNPATH_TOKEN` | **密钥** | SignPath API token（需具备 submitter 权限） |

> `detect-release-signing.ps1:10-15` 会校验这四项是否齐全，缺任何一项**直接失败**（不退回未签名）。

另外注意 `release.yml:157`：设为 `signpath` 后，三个 Windows job 会从自托管 runner
**切到 GitHub 托管 runner**（`windows-2025`）—— 这是 SignPath 开源计划的硬要求：
"每个签名请求之前的 job 都必须跑在 GitHub 托管 runner 上"。

### ⚠️ 必须先修的代码问题：7 个 EXE 没有版本资源

SignPath 新政策要求（[metadata restrictions](https://about.signpath.io/documentation/artifact-configuration/reference#metadata-restrictions)）：

> All signed binaries must have **metadata attributes set and enforced**
> — 所有 *product name* 设为项目名，所有 *product version* 在同一构建中取同一值。

实测结果——**10 个 `.rc` 里只有 1 个有 `VERSIONINFO`**：

| 被签名的二进制 | 版本资源 | 供资源的 `.rc` |
|---|---|---|
| `MetasequoiaImeTsf.dll`（32/64） | ✅ `ProductName=MetasequoiaIME` `FileVersion=0.7.1.0` | `windows/src/IME/MetasequoiaIME.rc` |
| `MetasequoiaImeServer.exe` | ❌ **无** | `server/src/resource/resource.rc`（只有图标） |
| `MetasequoiaImeSettings.exe` | ❌ 无 | 同上 |
| `MetasequoiaImeWatchdog.exe` | ❌ 无 | 同上 |
| `MetasequoiaImeDictionaryReplay.exe` | ❌ 无 | 同上 |
| `MetasequoiaImeEmojiPanel.exe` | ❌ 无 | `server/src/emoji-panel/emoji_panel.rc`（只有图标） |
| `MetasequoiaImeKeyboardPanel.exe` | ❌ 无 | `server/src/keyboard-panel/keyboard_panel.rc` |
| `MetasequoiaImeHandwritingPanel.exe` | ❌ 无 | `server/src/handwriting-panel/handwriting_panel.rc` |

**这会让签名请求被拒。** 需要给这 7 个二进制补 `VERSIONINFO` 块，并把值统一：

```
ProductName    "Metasequoia IME 水杉输入法"   ; 全部一致
ProductVersion "0.7.1.0"                    ; 全部一致（= MyAppVersion），同一构建内唯一
FileVersion    "0.7.1.0"                    ; 同上，4 段数字
CompanyName    "Metasequoia"
OriginalFilename "<对应的 exe 名>"
```

建议做法（工作量可控）：

1. 新建 `server/src/resource/version.rc` 或在现有 `resource.rc` 里补一个 `VERSIONINFO` 块
2. 把 `ProductVersion` / `FileVersion` 交给构建注入（与 `apply_version.py` 同样的思路），避免手工改多处
3. 把该 `.rc` 加进 `server/CMakeLists.txt` 里那 5 处 `add_executable`（Server / Settings / Watchdog / DictionaryReplay / 三个面板）
4. 用 `dumpbin` 或 `（Get-Item x.exe).VersionInfo` 复验每个二进制

**这块我还没做** —— 它属于"为了签名才需要"的改动，等你确认要走 SignPath 再动手，避免白改。

### 申请时的其它必做项（来自条款）

| 条款 | 要做的事 |
|---|---|
| **Code signing policy** | 在项目主页/下载页加一节，标题必须用 **"Code signing policy"**，内容含 "Free code signing provided by SignPath.io, certificate by SignPath Foundation"、团队角色与成员、隐私政策链接 |
| **多因素认证** | 所有成员在 SignPath 与 GitHub 上都必须开 MFA |
| **角色划分** | 明确 Authors / Reviewers / Approvers；每个签名请求都要 Approver 审批 |
| **可见 fork** | 条款要求"your project visibly uses a fork of the upstream project, e.g. using GitHub's fork feature"——**用 GitHub 的 Fork 功能**，别推一个看不出 fork 关系的新仓库 |
| **活跃维护** | 项目必须处于活跃维护状态，且**已发布过**想签名的那个形态 |
| **声誉门槛** | 条款明确写了：对可执行的程序，"we require a certain verifiable reputation"（源码没人认识的项目不会签）。**这是软条件，但对全新 fork 是最现实的风险**，建议申请时补充说明上游项目的背景与用户量 |

---

## 4. 建议的执行顺序

1. **先申请，拿到明确答复**（尤其是 2.2 那条"绕过粘贴"是否触犯 No hacking tools）
2. 得到肯定答复后，再补 7 个 EXE 的 `VERSIONINFO`（第 3 节末尾）
3. 在 GitHub 上**用 Fork 功能**创建仓库，把 `feat/autotype-hotkey` 推上去
4. 配好 4 个变量 + 1 个密钥，把 `WINDOWS_SIGNING_PROVIDER=signpath`
5. 加 "Code signing policy" 页面
6. 走一次 release 流程，验证签名与 `uiAccess` 是否真正生效（用 `signtool verify /pa` 与
   把候选窗放到一个管理员权限窗口上做实测）

## 5. 在拿到签名之前怎么办

`uiAccess` 拿不到，但功能本身是好的（模拟输入不依赖提权，只是进不了管理员窗口）。折中：

- 发布时用 `-unsigned` 后缀命名（现有流程在未配置证书时会自动加，见 `detect-release-signing.ps1:47`）
- 在 Release 说明里写明：**未签名，SmartScreen 会提示；且无法注入管理员权限窗口**
- 安装包可继续用本机自签证书（`installer/Sign-PackageBinaries-Local.ps1`），仅对**已导入该证书的机器**有效
