param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$QpdfRoot,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../releases')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$config = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'release-config.json') | ConvertFrom-Json
$cmake = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt')
if ($cmake -notmatch 'project\(TimeTable VERSION (\d+\.\d+\.\d+) LANGUAGES CXX\)') { throw 'Project version not found.' }
$version = $Matches[1]
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$zipPath = Join-Path $outputRoot "TimeTable-v$version-win64.zip"
if (Test-Path -LiteralPath $zipPath) { throw "ZIP already exists; move it elsewhere before retrying: $zipPath" }
$stage = Join-Path $projectRoot ('build/release-stage-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
# 過去のDLLや利用者データを引き継がないよう、毎回別の空フォルダーを使う。

foreach ($name in @('TimeTable.exe', 'TimeTableUpdater.exe', 'TimeTable_ja_JP.qm')) {
    Copy-Item -LiteralPath (Join-Path $buildRoot $name) -Destination $stage
}
New-Item -ItemType Directory -Path (Join-Path $stage 'defaults') | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'defaults/manual.md') -Destination (Join-Path $stage 'defaults/manual.md')

$deploy = Join-Path $QtRoot 'bin/windeployqt.exe'
foreach ($name in @('TimeTable.exe', 'TimeTableUpdater.exe')) {
    & $deploy --release --compiler-runtime (Join-Path $stage $name)
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed: $name" }
}

$qpdfStage = Join-Path $stage "qpdf$($config.qpdfVersion)"
New-Item -ItemType Directory -Path (Join-Path $qpdfStage 'bin') | Out-Null
$qpdfFiles = @('qpdf.exe', 'qpdf30.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')
foreach ($name in $qpdfFiles) {
    Copy-Item -LiteralPath (Join-Path $QpdfRoot "bin/$name") -Destination (Join-Path $qpdfStage 'bin')
}
# qpdfの配布条件・著作権表示も、新規インストール用ZIPへ同梱する。
foreach ($name in @('Artistic-2.0', 'LICENSE.txt', 'NOTICE', 'README.md')) {
    $path = Join-Path $QpdfRoot $name
    if (Test-Path -LiteralPath $path -PathType Leaf) { Copy-Item -LiteralPath $path -Destination $qpdfStage }
}
$qpdfLicense = Join-Path $QpdfRoot 'share/doc/qpdf/manual-html/_sources/license.rst.txt'
Copy-Item -LiteralPath $qpdfLicense -Destination (Join-Path $qpdfStage 'license.txt')

$required = @('TimeTable.exe', 'TimeTableUpdater.exe', 'TimeTable_ja_JP.qm', 'defaults/manual.md',
    'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'Qt6PrintSupport.dll', 'Qt6Pdf.dll', 'Qt6PdfWidgets.dll',
    'platforms/qwindows.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll',
    "qpdf$($config.qpdfVersion)/bin/qpdf.exe", "qpdf$($config.qpdfVersion)/bin/qpdf30.dll", "qpdf$($config.qpdfVersion)/license.txt")
$required += @($qpdfFiles | ForEach-Object { "qpdf$($config.qpdfVersion)/bin/$_" })
foreach ($name in $required) {
    $path = Join-Path $stage $name
    if (!(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Required release file missing: $name"
    }
}

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $entries = @($zip.Entries | Where-Object { $_.Name } | ForEach-Object { $_.FullName.Replace('\', '/') })
    foreach ($name in $required) {
        if ($entries -notcontains $name) { throw "Required ZIP entry missing: $name" }
    }
    # データ・開発用ファイルが混入したら、Release作成前に停止する。
    foreach ($name in $entries) {
        if ($name -match '(^|/)(data|schedules|backups|schedulePDF|guidanceReportPDF|salaryPDF|CMakeFiles|\.git)(/|$)' -or
            $name -match '\.(pdb|obj|o|a|cpp|h|schedule|lock)$' -or $name -match '(^|/)(CMakeCache\.txt|build\.ninja|AGENTS\.md)$') {
            throw "Forbidden ZIP entry: $name"
        }
    }
} finally {
    $zip.Dispose()
}
$hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$zipPath.sha256", "$hash  $([IO.Path]::GetFileName($zipPath))`n")
Write-Host "Created and verified: $zipPath"
