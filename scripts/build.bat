@echo off
cls
cd ..
if not exist build (
    mkdir build
)
cd build || exit
@REM cmake -G "MinGW Makefiles" -DUSE_CLANG=ON -DCMAKE_TOOLCHAIN_FILE="../scripts/settings.cmake" -DCMAKE_BUILD_TYPE=%1 ..
cmake -G "Unix Makefiles" -DUSE_GCC=ON -DCMAKE_TOOLCHAIN_FILE="../scripts/settings.cmake" -DCMAKE_BUILD_TYPE=%1 ..
@REM cmake -G "Visual Studio 16 2019" ^
@REM -DVS_PATH="C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64" ^
@REM -DUSE_VS2019=ON -DCMAKE_TOOLCHAIN_FILE="../scripts/settings.cmake" -DCMAKE_BUILD_TYPE=%1 ..
cmake --build . -j