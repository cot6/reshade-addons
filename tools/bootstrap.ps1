#Requires -version 7.2

# --------------------------------------
# リポジトリのルート ディレクトリに移動

Push-Location $( git rev-parse --show-toplevel )

# --------------------------------------
# [vcpkg]

Push-Location deps\vcpkg

# --------------------------------------
# [vcpkg] 利用準備の実行

.\bootstrap-vcpkg.bat

# --------------------------------------
# [vcpkg] 依存関係を用意

# 32-bit [zlib, libpng] (.lib /MT /MTd)
.\vcpkg install zlib:x86-windows-static
.\vcpkg install libpng:x86-windows-static

# 32-bit [zlib, libpng] (.lib /MD /MDd)
.\vcpkg install zlib:x86-windows-static-md
.\vcpkg install libpng:x86-windows-static-md

# 64-bit [zlib, libpng] (.lib /MT /MTd)
.\vcpkg install zlib:x64-windows-static
.\vcpkg install libpng:x64-windows-static

# 64-bit [zlib, libpng] (.lib /MD /MDd)
.\vcpkg install zlib:x64-windows-static-md
.\vcpkg install libpng:x64-windows-static-md

# --------------------------------------
# 結果: 成功

Pop-Location
