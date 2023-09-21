$build_dir = "./build"
New-Item -Path "." -Name $build_dir -ItemType Directory
# 编译
cmake -G "MinGW Makefiles" -B $build_dir .
cmake --build $build_dir --config Release -j 18 -- 