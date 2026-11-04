# 実際のReleaseビルド・GitHub通信を行わず、配布物の混入防止と公開処理の停止条件を検証する。
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testRoot = Join-Path $projectRoot ('build/release-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$checks = @{ Count = 0 }

function Assert-ReleaseCheck([bool]$condition, [string]$message) {
    if (!$condition) { throw $message }
    $checks.Count++
}

function Assert-ReleaseFailure([scriptblock]$operation, [string]$message) {
    $failed = $false
    try { & $operation } catch { $failed = $_.Exception.Message -like "*$message*" }
    Assert-ReleaseCheck $failed "Expected failure: $message"
}

# Qt公式と同じバイト配列のチェックサム応答を模擬し、不一致時は展開しない。
$parseErrors = $null
$tokens = $null
$installAst = [Management.Automation.Language.Parser]::ParseFile((Join-Path $projectRoot 'scripts/install-release-dependencies.ps1'), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw 'Dependency installer parse error.' }
$installFunction = $installAst.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Install-QtPackage' }, $true)
. ([scriptblock]::Create($installFunction.Extent.Text))
$destinationRoot = Join-Path $testRoot 'dependencies'
$downloads = Join-Path $destinationRoot 'downloads'
New-Item -ItemType Directory -Path $downloads -Force | Out-Null
$downloadState = @{ Extracts = 0; BadHash = $false }
$sevenZip = 'Invoke-TestSevenZip'
function Invoke-TestSevenZip {
    $downloadState.Extracts++
    $global:LASTEXITCODE = 0
}
function Invoke-WebRequest {
    param($Uri, $TimeoutSec, $OutFile)
    if ($Uri -like '*/Updates.xml') {
        return @{ Content = '<Updates><PackageUpdate><Name>test.qt</Name><Version>6.11.0-fixed</Version><DownloadableArchives>qtbase.7z, unused.7z</DownloadableArchives></PackageUpdate></Updates>' }
    }
    if ($Uri -like '*.sha1') {
        $bytes = [Text.Encoding]::UTF8.GetBytes('test archive')
        $sha1 = [Security.Cryptography.SHA1]::Create()
        try { $hash = [BitConverter]::ToString($sha1.ComputeHash($bytes)).Replace('-', '') } finally { $sha1.Dispose() }
        if ($downloadState.BadHash) { $hash = '0' * 40 }
        return @{ Content = [Text.Encoding]::ASCII.GetBytes($hash) }
    }
    Assert-ReleaseCheck ($Uri -eq 'https://qt.invalid/test.qt/6.11.0-fixedqtbase.7z') 'Unexpected Qt archive URL.'
    [IO.File]::WriteAllText($OutFile, 'test archive', [Text.UTF8Encoding]::new($false))
}
Install-QtPackage 'https://qt.invalid' 'test.qt' '6.11.0-fixed' '^qtbase'
Assert-ReleaseCheck ($downloadState.Extracts -eq 1) 'Verified Qt archive must be extracted.'
Assert-ReleaseFailure { Install-QtPackage 'https://qt.invalid' 'test.qt' 'wrong-version' '^qtbase' } 'Unexpected Qt package/version'
$downloadState.BadHash = $true
Assert-ReleaseFailure { Install-QtPackage 'https://qt.invalid' 'test.qt' '6.11.0-fixed' '^qtbase' } 'Checksum mismatch'
Assert-ReleaseCheck ($downloadState.Extracts -eq 1) 'Unverified Qt archive must never be extracted.'

# 別のプロジェクトと擬似windeployqtを作り、過去ビルドの残骸を意図的に混ぜる。
$fixture = Join-Path $testRoot 'fixture'
foreach ($directory in @('scripts', 'defaults', 'old-build/data', 'old-build/platforms', 'qt/bin', 'qpdf/bin', 'qpdf/share/doc/qpdf/manual-html/_sources')) {
    New-Item -ItemType Directory -Path (Join-Path $fixture $directory) -Force | Out-Null
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'scripts/new-release-package.ps1') -Destination (Join-Path $fixture 'scripts')
Copy-Item -LiteralPath (Join-Path $projectRoot 'release-config.json') -Destination $fixture
Copy-Item -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Destination $fixture
foreach ($file in @('defaults/manual.md', 'old-build/TimeTable.exe', 'old-build/TimeTableUpdater.exe', 'old-build/TimeTable_ja_JP.qm',
    'old-build/obsolete.dll', 'old-build/platforms/obsolete.dll', 'old-build/data/master.json', 'old-build/unlisted.txt',
    'qpdf/bin/qpdf.exe', 'qpdf/bin/qpdf30.dll', 'qpdf/bin/libgcc_s_seh-1.dll', 'qpdf/bin/libstdc++-6.dll', 'qpdf/bin/libwinpthread-1.dll', 'qpdf/bin/obsolete.dll',
    'qpdf/share/doc/qpdf/manual-html/_sources/license.rst.txt')) {
    [IO.File]::WriteAllText((Join-Path $fixture $file), 'fixture')
}
$shimSource = Join-Path $testRoot 'deploy.cs'
[IO.File]::WriteAllText($shimSource, @'
using System;
using System.IO;
class Deploy {
    static int Main(string[] args) {
        if (Environment.GetEnvironmentVariable("TIMETABLE_TEST_DEPLOY_FAIL") == "1") return 1;
        string root = Path.GetDirectoryName(args[args.Length - 1]);
        foreach (string name in new [] { "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll", "Qt6PrintSupport.dll", "Qt6Pdf.dll", "Qt6PdfWidgets.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll", "platforms/qwindows.dll" }) {
            string path = Path.Combine(root, name);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, "deployed");
        }
        if (Environment.GetEnvironmentVariable("TIMETABLE_TEST_INJECT_DATA") == "1") {
            Directory.CreateDirectory(Path.Combine(root, "data"));
            File.WriteAllText(Path.Combine(root, "data/master.json"), "must reject");
        }
        return 0;
    }
}
'@)
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $compiler /nologo /target:exe "/out:$fixture/qt/bin/windeployqt.exe" $shimSource
if ($LASTEXITCODE -ne 0) { throw 'Could not compile deployment test stub.' }
$packageArguments = @{ BuildDirectory = "$fixture/old-build"; QtRoot = "$fixture/qt"; QpdfRoot = "$fixture/qpdf" }
$packageScript = Join-Path $fixture 'scripts/new-release-package.ps1'
& $packageScript @packageArguments
$zipFile = @(Get-ChildItem -LiteralPath "$fixture/releases" -Filter '*.zip')[0].FullName
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($zipFile)
try {
    $names = @($zip.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
    foreach ($unwanted in @('obsolete.dll', 'platforms/obsolete.dll', 'data/master.json', 'unlisted.txt', 'qpdf12.3.2/bin/obsolete.dll')) {
        Assert-ReleaseCheck ($names -notcontains $unwanted) "Old build file leaked: $unwanted"
    }
    Assert-ReleaseCheck ($names -contains 'defaults/manual.md') 'Default manual missing.'
    Assert-ReleaseCheck ($names -contains 'qpdf12.3.2/bin/qpdf30.dll') 'qpdf dependency missing.'
} finally { $zip.Dispose() }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $packageScript -BuildDirectory $packageArguments.BuildDirectory -QtRoot $packageArguments.QtRoot -QpdfRoot $packageArguments.QpdfRoot -OutputDirectory "$fixture/windows-powershell-output"
Assert-ReleaseCheck ($LASTEXITCODE -eq 0) 'Manual packaging must work in Windows PowerShell 5.1.'
Assert-ReleaseFailure { & $packageScript @packageArguments } 'ZIP already exists'
try {
    $env:TIMETABLE_TEST_DEPLOY_FAIL = '1'
    Assert-ReleaseFailure { & $packageScript @packageArguments -OutputDirectory "$fixture/failed-deploy" } 'windeployqt failed'
    $env:TIMETABLE_TEST_DEPLOY_FAIL = $null
    $env:TIMETABLE_TEST_INJECT_DATA = '1'
    Assert-ReleaseFailure { & $packageScript @packageArguments -OutputDirectory "$fixture/contaminated" } 'Forbidden ZIP entry'
} finally {
    $env:TIMETABLE_TEST_DEPLOY_FAIL = $null
    $env:TIMETABLE_TEST_INJECT_DATA = $null
}

# GitHub APIを模擬し、タグや公開済みReleaseを上書きしないことを確認する。
function Reset-ReleaseApi {
    $script:apiState = @{ Tag = $null; Release = $null; Assets = @(); Writes = [Collections.Generic.List[string]]::new(); Denied = $false }
}

function Invoke-WebRequest {
    param($Uri, $Method, $Headers, [switch]$SkipHttpErrorCheck, $TimeoutSec, $Body, $ContentType, $InFile)
    $path = ([uri]$Uri).AbsolutePath -replace '^/repos/ROMEKANA/TimeTable/', ''
    if ($apiState.Denied) { return @{ StatusCode = 403; Content = '{}' } }
    if ($Method -ne 'GET') { $apiState.Writes.Add("$Method $path") }
    $result = $null
    $status = 200
    if ($Method -eq 'GET' -and $path -eq 'git/ref/tags/v0.3.34') {
        if ($apiState.Tag) { $result = @{ object = $apiState.Tag } } else { $status = 404 }
    } elseif ($Method -eq 'GET' -and $path -eq 'git/tags/annotated') {
        $result = @{ object = @{ type = 'commit'; sha = $releaseCommit } }
    } elseif ($Method -eq 'GET' -and $path -eq 'releases/tags/v0.3.34') {
        if ($apiState.Release) { $result = $apiState.Release } else { $status = 404 }
    } elseif ($Method -eq 'POST' -and $path -eq 'git/refs') {
        $request = $Body | ConvertFrom-Json
        $apiState.Tag = @{ type = 'commit'; sha = $request.sha }
        $result = @{ object = $apiState.Tag }
    } elseif ($Method -eq 'POST' -and $path -eq 'releases/generate-notes') {
        $result = @{ body = 'Generated notes' }
    } elseif ($Method -eq 'POST' -and $path -eq 'releases') {
        $request = $Body | ConvertFrom-Json
        Assert-ReleaseCheck ($request.draft -and !$request.prerelease) 'Release must remain a draft.'
        $apiState.Release = @{ id = 1; body = $request.body; draft = $true; prerelease = $false; upload_url = 'https://uploads.github.com/releases/1/assets{?name,label}'; html_url = 'https://github.com/ROMEKANA/TimeTable/releases/tag/v0.3.34' }
        $result = $apiState.Release
    } elseif ($Method -eq 'GET' -and $path -eq 'releases/1/assets') {
        $result = @($apiState.Assets)
    } elseif ($Method -eq 'GET' -and $path -eq 'releases/1') {
        $result = $apiState.Release
    } elseif ($Method -eq 'Post' -and ([uri]$Uri).Host -eq 'uploads.github.com') {
        $name = [uri]::UnescapeDataString(([uri]$Uri).Query.Substring(6))
        $apiState.Assets += @{ name = $name; state = 'uploaded'; digest = 'sha256:' + (Get-FileHash -LiteralPath $InFile -Algorithm SHA256).Hash.ToLowerInvariant() }
        $result = @{}
    } else { throw "Unexpected API call in test: $Method $Uri" }
    return @{ StatusCode = $status; Content = (ConvertTo-Json -InputObject $result -Depth 10 -Compress) }
}

$releaseCommit = 'a' * 40
$publishArguments = @{ Repository = 'ROMEKANA/TimeTable'; Commit = $releaseCommit; Tag = 'v0.3.34'; ZipPath = (Join-Path $testRoot 'TimeTable-v0.3.34-win64.zip') }
Copy-Item -LiteralPath $zipFile -Destination $publishArguments.ZipPath
$hash = (Get-FileHash -LiteralPath $publishArguments.ZipPath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($publishArguments.ZipPath + '.sha256'), "$hash  TimeTable-v0.3.34-win64.zip`n")
$publishScript = Join-Path $projectRoot 'scripts/publish-draft-release.ps1'
$oldToken = $env:GH_TOKEN
$oldSummary = $env:GITHUB_STEP_SUMMARY
try {
    $env:GH_TOKEN = 'test-token-never-sent'
    $env:GITHUB_STEP_SUMMARY = Join-Path $testRoot 'summary.md'
    Reset-ReleaseApi
    & $publishScript @publishArguments
    Assert-ReleaseCheck ($apiState.Tag.sha -eq $releaseCommit) 'Tag commit differs from build commit.'
    Assert-ReleaseCheck ($apiState.Assets.Count -eq 2) 'Expected ZIP and checksum assets.'
    $writeCount = $apiState.Writes.Count
    & $publishScript @publishArguments
    Assert-ReleaseCheck ($apiState.Writes.Count -eq $writeCount) 'Identical retry must not overwrite assets.'
    $apiState.Tag = @{ type = 'tag'; sha = 'annotated' }
    & $publishScript @publishArguments
    Assert-ReleaseCheck ($apiState.Writes.Count -eq $writeCount) 'Annotated tag retry must not write.'
    $apiState.Assets[0].digest = 'sha256:wrong'
    Assert-ReleaseFailure { & $publishScript @publishArguments } 'Existing asset differs'
    Assert-ReleaseCheck ($apiState.Writes.Count -eq $writeCount) 'Different asset must not be overwritten.'
    Reset-ReleaseApi
    $apiState.Tag = @{ type = 'commit'; sha = ('b' * 40) }
    Assert-ReleaseFailure { & $publishScript @publishArguments } 'different commit'
    Assert-ReleaseCheck ($apiState.Writes.Count -eq 0) 'Conflicting tag must not trigger writes.'
    Reset-ReleaseApi
    $apiState.Release = @{ draft = $false; prerelease = $false }
    Assert-ReleaseFailure { & $publishScript @publishArguments } 'existing published'
    Assert-ReleaseCheck ($apiState.Writes.Count -eq 0) 'Published release must not trigger writes.'
    Reset-ReleaseApi
    $apiState.Release = @{ draft = $true; prerelease = $false; body = 'Unrelated draft' }
    Assert-ReleaseFailure { & $publishScript @publishArguments } 'Existing draft'
    Assert-ReleaseCheck ($apiState.Writes.Count -eq 0) 'Unrelated draft must not trigger tag creation.'
    Reset-ReleaseApi
    $apiState.Denied = $true
    Assert-ReleaseFailure { & $publishScript @publishArguments } '403'
    Assert-ReleaseCheck ($apiState.Writes.Count -eq 0) 'Permission error must not be treated as missing tag.'
} finally {
    $env:GH_TOKEN = $oldToken
    $env:GITHUB_STEP_SUMMARY = $oldSummary
}
Write-Host "Release workflow tests passed: $($checks.Count) checks. Fixtures: $testRoot"
