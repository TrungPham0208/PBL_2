@echo off
setlocal
set "PATH=C:\Qt\6.8.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;%PATH%"
cd /d "%~dp0"

if not exist "build\pbl2_simulator.exe" (
    echo [INFO] Chua tim thay pbl2_simulator.exe. Dang tien hanh build...
    "C:\Qt\Tools\CMake_64\bin\cmake.exe" -B build -G Ninja
    "C:\Qt\Tools\Ninja\ninja.exe" -C build
)

echo [INFO] Dang khoi chay PBL2 Network Simulator GUI...
start "" "%~dp0build\pbl2_simulator.exe"
endlocal
