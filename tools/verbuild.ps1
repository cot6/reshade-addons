#Requires -version 7.2

using namespace System.IO
using namespace System.Text

$ErrorActionPreference = 'Stop'

$ProjectDir = Get-Location -PSProvider FileSystem | ForEach-Object -MemberName Path

# --------------------------------------
# リポジトリのルート ディレクトリに移動

Push-Location $( git rev-parse --show-toplevel )

# --------------------------------------
# リポジトリにタグがあるかどうか

if ( $( git describe --tags --always ) -notlike 'v*' )
{
    # --------------------------------------
    # 結果: 失敗

    Write-Error ブランチからバージョン` タグが見つからないか、見つかったタグがバージョン形式ではありません。` $( git describe --tags --always )` はSHA-1またはタグです。
}

# --------------------------------------
# タグ文字列を確認する

if ( $( git describe --long --tags ) -notmatch 'v(.+)-(\d+)-g(.+)' )
{
    # --------------------------------------
    # 結果: 失敗

    Write-Error git` error` `(git` describe`)
}

$versionTag = $matches[1]
$version = $versionTag.Split('-')
$versionExtra = $version[1] ? ('-' + $version[1]) : ''
$version = $version[0] + '.' + $(git rev-list HEAD --count)
$version = [Version]::new($version)
$tagRange = [int]::Parse($matches[2])
$tagRangeStr =  $tagRange -eq 0 ? '' : ('+' + $tagRange)
$sha1 = $matches[3]

$developing = $( git status --short ) | Where-Object { $_ -notlike '`?`?*' -and $_ -notlike '!!*' } | Where-Object { $_ -notlike "`?`? deps/reshade" }
$developingStr = $developing ? ' UNCOMMITED' : ''

# --------------------------------------
# メタ データをファイルに出力する

$content = @"
#pragma once

#define ADDON_MAJOR $($version.Major)
#define ADDON_MINOR $($version.Minor)
#define ADDON_BUILD $($version.Build)
#define ADDON_REVISION $($version.Revision)

#define ADDON_FULL $($version.Major).$($version.Minor).$($version.Build).$($version.Revision)

#define ADDON_STRING_FILE "$( git describe --long --tags )$developingStr"
#define ADDON_STRING_PRODUCT "$( git describe --long --tags )/$( git rev-parse --abbrev-ref HEAD )$developingStr"

"@

$VersionHeaderFile = [FileInfo]::new([Path]::Combine($ProjectDir, 'res', 'version.h'))
if (-not $VersionHeaderFile.Directory.Exists)
{
    $VersionHeaderFile.Directory.Create()
}

try
{
    [File]::WriteAllText($VersionHeaderFile, $content, [UTF8Encoding]::new($true, $true))
}
catch
{
    # 他のプロジェクトがビルド中で既に作成された
}

# --------------------------------------
# 結果: 成功

Pop-Location
