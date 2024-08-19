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

# 32-bit [tiff, libpng, efsw] (.lib /MT /MTd)
.\vcpkg install --recurse tiff[core]:x86-windows-static
.\vcpkg install libpng:x86-windows-static efsw:x86-windows-static

# 32-bit [tiff, libpng, efsw] (.lib /MD /MDd)
.\vcpkg install --recurse tiff[core]:x86-windows-static-md
.\vcpkg install libpng:x86-windows-static-md efsw:x86-windows-static-md

# 64-bit [tiff, libpng, efsw] (.lib /MT /MTd)
.\vcpkg install --recurse tiff[core]:x64-windows-static
.\vcpkg install libpng:x64-windows-static efsw:x64-windows-static

# 64-bit [tiff, libpng, efsw] (.lib /MD /MDd)
.\vcpkg install --recurse tiff[core]:x64-windows-static-md
.\vcpkg install libpng:x64-windows-static-md efsw:x64-windows-static-md

# --------------------------------------
# 結果: 成功

Pop-Location
