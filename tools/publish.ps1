#Requires -version 7.2

using namespace System.IO
using namespace System.Text

$ErrorActionPreference = 'Stop'

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

# --------------------------------------
# 配布用のアーカイブ ファイルを作成

$developing = $( git status --short ) | Where-Object { $_ -notlike '`?`?*' -and $_ -notlike '!!*' } | Where-Object { $_ -notlike "`?`? deps/reshade" }
$developingStr = $developing ? '+UNCOMMITED' : ''

$compressionFilePath = 'publish\reshade-addons-{0}{1}.zip' -f $( git describe --long --tags ), $developingStr

$compressionFiles = [System.Collections.ArrayList]::new()
$compressionFiles.AddRange($('*.md' | Get-Item))
$compressionFiles.AddRange($(Get-ChildItem -Path 'publish\source\addon-editorhistory\bin\*' -Include '*.addon', '*.addon32', '*.addon64'))
$compressionFiles.AddRange($(Get-ChildItem -Path 'publish\source\addon-screenshot\bin\*' -Include '*.addon', '*.addon32', '*.addon64'))
$compressionFiles.AddRange($(Get-ChildItem -Path 'publish\source\addon-uibind\bin\*' -Include '*.addon', '*.addon32', '*.addon64'))

Compress-Archive $compressionFiles -CompressionLevel Optimal -DestinationPath $compressionFilePath -Force

# --------------------------------------
# 結果: 成功

Pop-Location
