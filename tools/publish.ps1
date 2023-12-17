#Requires -version 7.2

using namespace System.IO
using namespace System.Text

$ErrorActionPreference = 'Stop'

# --------------------------------------
# リポジトリのルート ディレクトリに移動

Push-Location $( git rev-parse --show-toplevel )

# --------------------------------------
# 配布用のアーカイブ ファイルを作成

foreach ( $compressionFile in Get-ChildItem -File -Path 'publish\source\*\bin\*.addon??' )
{
    Compress-Archive @( @( Get-ChildItem -File -Filter *.md ) + @( $compressionFile ) ) `
        -CompressionLevel Optimal `
        -DestinationPath "publish\$([Path]::GetFileNameWithoutExtension($compressionFile.Name)).zip" `
        -Force
}

Compress-Archive @( @( Get-ChildItem -File -Filter *.md ) + @( Get-ChildItem -File -Path 'publish\source\*\bin\*.addon??' ) ) `
    -CompressionLevel Optimal `
    -DestinationPath "publish\ReShade-Addons-By-seri14.zip" `
    -Force

# --------------------------------------
# 結果: 成功

Pop-Location
