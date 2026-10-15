# Configure and build the Server; do not reuse old binaries after a failed build.
#
# 本地构建刻意**不注入 uiAccess 清单**。原因见 server/AGENTS.md 与 docs/fork-release.md：
#   uiAccess="true" 只在二进制拥有受信任的 Authenticode 签名、且装在 Windows 认可的位置时才生效。
#   本地/测试构建只有自签名证书，带上这个清单会让 Server 直接启动失败
#   （watchdog 用 ShellExecuteEx 拉起它会拿不到进程），表现为"中文输入整个坏掉"。
#
# 发布流程不经过本脚本：release.yml 用原始 cmake 编译，测试通过后再由
# scripts/ci/embed-server-manifest.ps1 独立注入清单，然后才上传与签名。
# 那条路径的时序是有意的——uiAccess 二进制无法在非交互会话里启动，必须先测普通版本。
#
# 所以：本脚本只负责"能跑起来"的普通构建；需要 uiAccess 时请显式调用上面那个注入脚本。
[CmdletBinding()]
param(
    # ALL_BUILD also builds MetasequoiaImeServerTests and test_webview_contract, which the packaging
    # step deletes again. The server target already depends on the watchdog, the three panels,
    # Settings, DictionaryReplay and the OpenCC dictionaries, so it produces exactly the packaged
    # set. The test jobs still build everything by passing -Target ALL_BUILD.
    [string]$Target = 'MetasequoiaImeServer',
    # The Visual Studio generator wires ZERO_CHECK into every target, so a build regenerates itself
    # whenever CMakeLists.txt changes. Re-running configure up front only costs ~10s without
    # changing the result, so do it once and let ZERO_CHECK handle the rest.
    [switch]$Reconfigure
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build-release'
Push-Location $projectRoot
try {
    if ($Reconfigure -or -not (Test-Path -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt'))) {
        cmake --preset=vcpkg-release
        if ($LASTEXITCODE -ne 0) { throw "Server configure failed ($LASTEXITCODE)" }
    }
    cmake --build $buildDirectory --config Release --target $Target --parallel
    if ($LASTEXITCODE -ne 0) { throw "Server build failed ($LASTEXITCODE)" }

    # 构建成功即可用。这里**曾经**调用 mt.exe 注入 uiAccess 清单，已移除：
    # 它会让本地构建的 Server 起不来，而发布路径并不依赖它（见文件头）。
} finally { Pop-Location }
