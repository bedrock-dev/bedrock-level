$build_dir = "./build"
New-Item -Path "." -Name $build_dir -ItemType Directory 
# complie
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -B $build_dir . 
cmake --build $build_dir --config Release -j 18 -- 