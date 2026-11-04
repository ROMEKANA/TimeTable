param([Parameter(Mandatory)][string]$Destination)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$config = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../release-config.json') | ConvertFrom-Json
$destinationRoot = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $destinationRoot) { throw 'Dependency destination must not exist.' }
New-Item -ItemType Directory -Path $destinationRoot | Out-Null
$sevenZip = (Get-Command 7z.exe -ErrorAction Stop).Source
$downloads = Join-Path $destinationRoot 'downloads'
New-Item -ItemType Directory -Path $downloads | Out-Null

# Qt 6.11の配布階層を直接参照し、公式チェックサムの一致後だけ展開する。
function Install-QtPackage([string]$repository, [string]$name, [string]$version, [string]$archivePattern) {
    [xml]$metadata = (Invoke-WebRequest "$repository/Updates.xml" -TimeoutSec 120).Content
    $packages = @($metadata.Updates.PackageUpdate | Where-Object { $_.Name -eq $name })
    if ($packages.Count -ne 1 -or $packages[0].Version -ne $version) {
        throw "Unexpected Qt package/version: $name ($version)"
    }
    $archives = @($packages[0].DownloadableArchives.Split(',').Trim() | Where-Object { $_ -match $archivePattern })
    if ($archives.Count -eq 0) { throw "No archives selected: $name" }
    foreach ($archive in $archives) {
        $url = "$repository/$name/$version$archive"
        $local = Join-Path $downloads "$version$archive"
        $checksumContent = (Invoke-WebRequest "$url.sha1" -TimeoutSec 120).Content
        $expected = if ($checksumContent -is [byte[]]) { [Text.Encoding]::ASCII.GetString($checksumContent).Trim() } else { $checksumContent.Trim() }
        # Qtの.sha1はバイナリ形式のHTTP応答になるため、文字列に直して比較する。
        if ($expected -notmatch '^[0-9a-fA-F]{40}$') { throw "Invalid checksum: $url" }
        Invoke-WebRequest $url -OutFile $local -TimeoutSec 600
        if ((Get-FileHash -LiteralPath $local -Algorithm SHA1).Hash -ne $expected) {
            throw "Checksum mismatch: $url"
        }
        & $sevenZip x $local "-o$destinationRoot" -y -bsp0 -bso0
        if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $archive" }
    }
}

$qtVersionKey = $config.qtVersion.Replace('.', '')
Install-QtPackage $config.qtRepository "qt.qt6.$qtVersionKey.win64_mingw" $config.qtPackageVersion '^(qtbase|qtsvg|qtdeclarative|qttools|qttranslations|MinGW-|d3dcompiler|opengl32sw)'
Install-QtPackage $config.qtRepository "qt.qt6.$qtVersionKey.addons.qtimageformats.win64_mingw" $config.qtPackageVersion '^qtimageformats-'
Install-QtPackage $config.pdfRepository "extensions.qtpdf.$qtVersionKey.win64_mingw" $config.pdfPackageVersion '^qtpdf-'
Install-QtPackage $config.mingwRepository 'qt.tools.win64_mingw1310' $config.mingwPackageVersion '^mingw1310\.7z$'

$qtRoot = Join-Path $destinationRoot "$($config.qtVersion)/mingw_64"
$mingwBin = Join-Path $destinationRoot 'Tools/mingw1310_64/bin'
foreach ($file in @("$qtRoot/bin/qmake.exe", "$qtRoot/bin/windeployqt.exe", "$qtRoot/lib/cmake/Qt6Pdf/Qt6PdfConfig.cmake", "$mingwBin/g++.exe")) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing dependency: $file" }
}
[IO.File]::WriteAllText((Join-Path $qtRoot 'bin/qt.conf'), "[Paths]`nPrefix=..`n")
# 展開先に合わせてQtのツールが自分自身のライブラリを見つけられるようにする。

$qpdfZip = Join-Path $downloads 'qpdf.zip'
Invoke-WebRequest $config.qpdfUrl -OutFile $qpdfZip -TimeoutSec 300
if ((Get-FileHash -LiteralPath $qpdfZip -Algorithm SHA256).Hash -ne $config.qpdfSha256) {
    throw 'qpdf SHA256 mismatch.'
}
Expand-Archive -LiteralPath $qpdfZip -DestinationPath (Join-Path $destinationRoot 'qpdf')
$qpdfRoot = Join-Path $destinationRoot "qpdf/qpdf-$($config.qpdfVersion)-mingw64"
if (!(Test-Path -LiteralPath "$qpdfRoot/bin/qpdf.exe")) { throw 'qpdf executable missing.' }

"QT_ROOT_DIR=$qtRoot" >> $env:GITHUB_ENV
"QPDF_ROOT=$qpdfRoot" >> $env:GITHUB_ENV
$mingwBin >> $env:GITHUB_PATH
"$qtRoot/bin" >> $env:GITHUB_PATH
