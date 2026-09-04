$build_dir = "./build"
# if (Test-Path $build_dir) {
#     Remove-Item $build_dir -Recurse -Force
# }
# New-Item -Path "." -Name $build_dir -ItemType Directory 
# complie
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DBEDROCK_LEVEL_BUILD_APPS=ON -DBEDROCK_LEVEL_BUILD_TESTS=ON -B $build_dir .
cmake --build $build_dir -j 18 --