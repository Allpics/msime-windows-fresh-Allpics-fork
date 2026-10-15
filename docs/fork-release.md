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

> 关于签名与 `uiAccess`：本仓库不做代码签名，因此不注入 `uiAccess` 清单，
> 随之而来的两点限制（安装时的未知发布者提示、无法注入管理员权限窗口）已在
> [INSTALL.md](../INSTALL.md) 顶部向使用者写明。若日后需要签名版本，请走上游渠道。

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
