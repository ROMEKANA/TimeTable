param(
    [Parameter(Mandatory)][string]$Repository,
    [Parameter(Mandatory)][string]$Commit,
    [Parameter(Mandatory)][string]$Tag,
    [Parameter(Mandatory)][string]$ZipPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Repository -ne 'ROMEKANA/TimeTable' -or $Commit -notmatch '^[0-9a-f]{40}$' -or $Tag -notmatch '^v\d+\.\d+\.\d+$') {
    throw 'Unexpected repository, commit, or tag.'
}
$headers = @{ Authorization = "Bearer $env:GH_TOKEN"; Accept = 'application/vnd.github+json'; 'X-GitHub-Api-Version' = '2022-11-28' }
$api = "https://api.github.com/repos/$Repository"

# 404だけを未作成と扱い、認証失敗や通信障害はそのまま停止する。
function Invoke-ReleaseApi([string]$method, [string]$path, [object]$body = $null) {
    $arguments = @{ Uri = "$api/$path"; Method = $method; Headers = $headers; SkipHttpErrorCheck = $true; TimeoutSec = 120 }
    if ($null -ne $body) {
        $arguments.Body = ConvertTo-Json -InputObject $body -Depth 10 -Compress
        $arguments.ContentType = 'application/json'
    }
    $response = Invoke-WebRequest @arguments
    if ($response.StatusCode -eq 404 -and $method -eq 'GET') { return $null }
    if ($response.StatusCode -lt 200 -or $response.StatusCode -ge 300) {
        throw "GitHub API failed: $method $path ($($response.StatusCode)): $($response.Content)"
    }
    return ($response.Content | ConvertFrom-Json)
}

# 軽量タグと注釈付きタグの両方について、最終的なコミットを照合する。
function Assert-ReleaseTag {
    $reference = Invoke-ReleaseApi 'GET' "git/ref/tags/$Tag"
    if ($null -eq $reference) { return $false }
    $target = $reference.object
    for ($depth = 0; $target.type -eq 'tag' -and $depth -lt 10; $depth++) {
        $target = (Invoke-ReleaseApi 'GET' "git/tags/$($target.sha)").object
    }
    if ($target.type -ne 'commit' -or $target.sha -ne $Commit) { throw "Tag $Tag points to a different commit. It will not be overwritten." }
    return $true
}

foreach ($path in @($ZipPath, "$ZipPath.sha256")) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing release asset: $path" }
}
$expectedName = "TimeTable-$Tag-win64.zip"
if ([IO.Path]::GetFileName($ZipPath) -ne $expectedName) { throw 'ZIP version differs from release tag.' }
$expectedHash = (Get-Content -Raw -LiteralPath "$ZipPath.sha256").Split(' ')[0]
if ($expectedHash -notmatch '^[0-9a-f]{64}$' -or (Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'Release ZIP hash mismatch.'
}
$tagExists = Assert-ReleaseTag
$release = Invoke-ReleaseApi 'GET' "releases/tags/$Tag"
if ($null -ne $release -and (!$release.draft -or $release.prerelease)) {
    throw 'An existing published/prerelease release will not be changed.'
}
if ($null -ne $release -and (!$release.body -or !$release.body.Contains("<!-- timetable-commit:$Commit -->"))) {
    throw 'Existing draft was not created for this commit by this workflow.'
}
if (!$tagExists) {
    Invoke-ReleaseApi 'POST' 'git/refs' @{ ref = "refs/tags/$Tag"; sha = $Commit } | Out-Null
    if (!(Assert-ReleaseTag)) { throw 'Created tag could not be verified.' }
}

if ($null -eq $release) {
    $notes = Invoke-ReleaseApi 'POST' 'releases/generate-notes' @{ tag_name = $Tag; target_commitish = $Commit }
    $body = "<!-- timetable-commit:$Commit -->`n`n$($notes.body)"
    $release = Invoke-ReleaseApi 'POST' 'releases' @{
        tag_name = $Tag; target_commitish = $Commit; name = $Tag; body = $body; draft = $true; prerelease = $false
    }
}
# 同じコミットの下書きだけ再実行を許可し、添付済みファイルは上書きしない。
if (!$release.body.Contains("<!-- timetable-commit:$Commit -->")) { throw 'Existing draft was not created for this commit by this workflow.' }
foreach ($path in @($ZipPath, "$ZipPath.sha256")) {
    $name = [IO.Path]::GetFileName($path)
    $assets = @(Invoke-ReleaseApi 'GET' "releases/$($release.id)/assets?per_page=100")
    $existing = @($assets | Where-Object { $_.name -eq $name })
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($existing.Count -gt 0) {
        if ($existing.Count -ne 1 -or $existing[0].state -ne 'uploaded' -or $existing[0].digest -ne "sha256:$hash") {
            throw "Existing asset differs: $name. Inspect the draft before retrying."
        }
        continue
    }
    # 添付がそろうまでは非公開にし、不完全なZIPを更新アプリに見せない。
    $current = Invoke-ReleaseApi 'GET' "releases/$($release.id)"
    if (!$current.draft) { throw 'Release has been published; upload stopped.' }
    $upload = $release.upload_url.Split('{')[0] + '?name=' + [uri]::EscapeDataString($name)
    Invoke-WebRequest -Uri $upload -Method Post -Headers $headers -ContentType 'application/octet-stream' -InFile $path -TimeoutSec 300 | Out-Null
}
# 添付後にGitHub側のハッシュを再照合し、すべて成功した場合だけ最新版として公開する。
$assets = @(Invoke-ReleaseApi 'GET' "releases/$($release.id)/assets?per_page=100")
foreach ($path in @($ZipPath, "$ZipPath.sha256")) {
    $name = [IO.Path]::GetFileName($path)
    $uploaded = @($assets | Where-Object { $_.name -eq $name })
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($uploaded.Count -ne 1 -or $uploaded[0].state -ne 'uploaded' -or $uploaded[0].digest -ne "sha256:$hash") {
        throw "Release asset verification failed: $name. Release remains unpublished."
    }
}
if (!(Assert-ReleaseTag)) { throw 'Release tag disappeared before publishing.' }
$current = Invoke-ReleaseApi 'GET' "releases/$($release.id)"
if (!$current.draft -or $current.prerelease -or !$current.body.Contains("<!-- timetable-commit:$Commit -->")) {
    throw 'Release changed before publishing; publication stopped.'
}
$release = Invoke-ReleaseApi 'PATCH' "releases/$($release.id)" @{ draft = $false; prerelease = $false; make_latest = 'true' }
if ($release.draft -or $release.prerelease) { throw 'Release publication could not be confirmed.' }
$latest = Invoke-ReleaseApi 'GET' 'releases/latest'
if ($null -eq $latest -or $latest.id -ne $release.id) { throw 'Published release is not returned by the latest release API.' }
"### Releaseを公開しました`n`n$($release.html_url)`n`n対象コミット: $Commit`n`nZIP・SHA256添付済み。更新アプリが参照する最新版APIへの反映も確認しました。" >> $env:GITHUB_STEP_SUMMARY
