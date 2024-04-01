#Requires -version 7.2

using namespace System.Collections
using namespace System.IO
using namespace System.Text

$ErrorActionPreference = 'Stop'

$ProjectDir = Get-Location -PSProvider FileSystem | ForEach-Object -MemberName Path

# --------------------------------------
# リポジトリのルート ディレクトリに移動

Push-Location $( git rev-parse --show-toplevel )

# --------------------------------------
# リポジトリにタグがあるかどうか

if ( $( git describe --match "v*" --tags --always ) -notlike 'v*' )
{
    # --------------------------------------
    # 結果: 失敗

    Write-Error "The version tag is missing in the branch, or the detected tag is not formatted as version. $( git describe --tags --always ) is SHA-1 or tag."
}

# --------------------------------------
# タグ文字列を確認する

if ( $( git describe --match "v*" --long --tags ) -notmatch 'v(.+)-(\d+)-g(.+)' )
{
    # --------------------------------------
    # 結果: 失敗

    Write-Error git` error` `(git` describe`)
}

$versionTag = $matches[1]
$version = $versionTag.Split('-')
$versionExtra = $version[1] ? ('-' + $version[1]) : ''
$version = $version[0] + '.' + $matches[2]
$version = [Version]::new($version)
$tagRange = [int]::Parse($matches[2])
$tagRangeStr =  $tagRange -gt 0 ? ('+' + $tagRange) : ''
$sha1 = $matches[3]

$developing = $( git status --short ) | Where-Object { $_ -notlike '`?`?*' -and $_ -notlike '!!*' } | Where-Object { $_ -notlike "`?`? deps/reshade" }
$developingStr = $developing ? ' UNCOMMITED' : ''

$fileFlags = [ArrayList]::new()
if ($developing) {
    $fileFlags.Add('VS_FF_PATCHED') | Out-Null
    Write-Warning 'Uncommitted changes exist. Please commit before releasing.'
}
if ($tagRange) {
    $fileFlags.Add('VS_FF_PRERELEASE') | Out-Null
    Write-Warning 'Not ready for release. The last digit of the version should be 0.'
}
if (-not $Env:CI -and -not $Env:APPVEYOR) {
    $fileFlags.Add('VS_FF_PRIVATEBUILD') | Out-Null
    Write-Warning 'Building on a personal development machine. You may encounter issues caused by binary compatibility, dependencies, and other factors.'
}
if ($fileFlags.Count -lt 1) {
    $fileFlags.Add('0x0L') | Out-Null
}
$fileFlags = $fileFlags -join ' | '

# --------------------------------------
# メタ データをファイルに出力する

$content = @"
#pragma once

#define ADDON_MAJOR $($version.Major)
#define ADDON_MINOR $($version.Minor)
#define ADDON_BUILD $($version.Build)
#define ADDON_REVISION $($version.Revision)

#define ADDON_FULL $($version.Major).$($version.Minor).$($version.Build).$($version.Revision)

#define ADDON_STRING_FILE "$( git describe --match "v*" --long --tags )$developingStr"
#define ADDON_STRING_PRODUCT "$( git describe --match "v*" --long --tags )/$( git rev-parse --abbrev-ref HEAD )$developingStr"

#ifdef _DEBUG
#define ADDON_FILEFLAGS ($fileFlags | VS_FF_DEBUG)
#else
#define ADDON_FILEFLAGS ($fileFlags)
#endif

#if ADDON_FILEFLAGS & VS_FF_PRIVATEBUILD
#define ADDON_PRIVATEBUILD `"Built by $(git config user.name) ($(git config user.email))` at $([DateTimeOffset]::Now.ToString('O'))"
#endif

"@

$VersionHeaderFile = [FileInfo]::new([Path]::Combine($ProjectDir, 'res', 'version.h'))
if (-not $VersionHeaderFile.Directory.Exists)
{
    $VersionHeaderFile.Directory.Create()
}

try
{
    [File]::WriteAllText($VersionHeaderFile, $content, [UTF8Encoding]::new($false, $true))
}
catch
{
    # 他のプロジェクトがビルド中で既に作成された
}

# --------------------------------------
# 結果: 成功

Pop-Location
