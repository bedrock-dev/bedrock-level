$build_dir = "./build_msvc"
# if (Test-Path $build_dir) {
#     Remove-Item $build_dir -Recurse -Force
# }
# New-Item -Path "." -Name $build_dir -ItemType Directory
# compile
cmake -G "Visual Studio 17 2022" -A x64 -DBEDROCK_LEVEL_BUILD_APPS=ON -DBEDROCK_LEVEL_BUILD_TESTS=ON -B $build_dir .
cmake --build $build_dir --config Release -j 18 --
