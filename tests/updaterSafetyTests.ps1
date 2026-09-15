$ErrorActionPreference = 'Stop'
$sourcePath = Join-Path $PSScriptRoot '../updaterMain.cpp'
$source = [IO.File]::ReadAllText($sourcePath)
$script = [regex]::Match($source, '(?s)R"PS\((.*?)\)PS"').Groups[1].Value
if (!$script) { throw 'Updater script not found' }
$script = $script.Replace('%1', "'test.zip'").Replace('%2', "'test-install'").Replace('%3', '12345')
$parseErrors = $null
$tokens = $null
$ast = [Management.Automation.Language.Parser]::ParseInput($script, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
if ($script.Contains('Stop-Process')) { throw 'Forced termination must not be used' }
$functions = $ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] }, $true)
foreach ($definition in $functions) {
    # 関数定義だけを取り込む。プロセス終了・更新・アプリ起動の本体は実行しない。
    . ([scriptblock]::Create($definition.Extent.Text))
}

$buildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build'))
$testRoot = Join-Path $buildRoot ('updater-safety-' + [Guid]::NewGuid().ToString('N'))
if (!$testRoot.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Test path must stay under build'
}
New-Item -ItemType Directory -Path $testRoot | Out-Null
$checks = 0

# 条件を検証し、違いがあれば試験用ファイルを残して停止する。
function Assert-Condition([bool]$condition, [string]$message) {
    if (!$condition) { throw $message }
    $script:checks++
}

# 試験用の配布物とインストール先を独立したフォルダーに作る。
function Initialize-Fixture([string]$name) {
    $script:work = Join-Path $testRoot $name
    $script:extract = Join-Path $work 'extract'
    $script:install = Join-Path $work 'install'
    foreach ($file in @('TimeTable.exe', 'TimeTableUpdater.exe', 'Qt6Core.dll', 'Qt6Widgets.dll', 'platforms/qwindows.dll', 'data/master.json', 'data/custom.exe', 'schedules/week.schedule', 'backups/original.json', 'custom.pdf')) {
        foreach ($directory in @($extract, $install)) {
            $path = Join-Path $directory $file
            New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
            [IO.File]::WriteAllText($path, $(if ($directory -eq $extract) { 'new' } else { 'old' }))
        }
    }
}

Initialize-Fixture 'success'
Install-VerifiedFiles
Assert-Condition ((Get-Content -LiteralPath (Join-Path $install 'TimeTable.exe') -Raw) -eq 'new') 'Application was not updated'
Assert-Condition ((Get-Content -LiteralPath (Join-Path $work 'original-app/TimeTable.exe') -Raw) -eq 'old') 'Original application backup missing'
foreach ($name in @('data/master.json', 'data/custom.exe', 'schedules/week.schedule', 'backups/original.json', 'custom.pdf')) {
    Assert-Condition ((Get-Content -LiteralPath (Join-Path $install $name) -Raw) -eq 'old') "User file overwritten: $name"
}

Initialize-Fixture 'missing-required'
Remove-Item -LiteralPath (Join-Path $extract 'TimeTable.exe')
$failed = $false
try { Install-VerifiedFiles } catch { $failed = $true }
Assert-Condition $failed 'Missing executable was accepted'
Assert-Condition ((Get-Content -LiteralPath (Join-Path $install 'Qt6Core.dll') -Raw) -eq 'old') 'Files changed before package validation'

Initialize-Fixture 'rollback'
$script:injectFailure = $true
# 更新の途中だけ書込失敗を注入し、通常のファイル操作で復旧を確認する。
function Copy-Item {
    [CmdletBinding()]
    param([string]$LiteralPath, [string]$Destination, [switch]$Force)
    if ($script:injectFailure -and $LiteralPath -eq (Join-Path $extract 'TimeTableUpdater.exe')) {
        $script:injectFailure = $false
        throw 'Injected write failure'
    }
    Microsoft.PowerShell.Management\Copy-Item -LiteralPath $LiteralPath -Destination $Destination -Force:$Force -ErrorAction Stop
}
$failed = $false
try { Install-VerifiedFiles } catch { $failed = $true }
Assert-Condition $failed 'Injected failure was not detected'
foreach ($name in @('TimeTable.exe', 'TimeTableUpdater.exe', 'Qt6Core.dll', 'Qt6Widgets.dll')) {
    Assert-Condition ((Get-Content -LiteralPath (Join-Path $install $name) -Raw) -eq 'old') "Rollback failed: $name"
}

# 実プロセスには触れず、終了待ちタイムアウトが中止になることを確認する。
function Get-TimeTableProcess { return [pscustomobject]@{ Name = 'Fixture' } }
$failed = $false
try { Wait-TimeTableProcesses -Seconds 0 } catch { $failed = $true }
Assert-Condition $failed 'Running application did not cancel update'

Write-Output "$checks updater checks passed. Fixtures: $testRoot"
