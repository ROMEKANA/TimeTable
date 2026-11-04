@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "RELEASE_DIR=%~dp0build\Desktop_Qt_6_11_0_MinGW_64_bit-Release"
set "OUTPUT_DIR=%~dp0releases"
set "CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
set "QT_ROOT=C:\Qt\6.11.0\mingw_64"
set "PATH=C:\Qt\Tools\mingw1310_64\bin;%QT_ROOT%\bin;%PATH%"

if not exist "%CMAKE_EXE%" (
    echo cmake.exe was not found: %CMAKE_EXE%
    pause
    exit /b 1
)

"%CMAKE_EXE%" --build "%RELEASE_DIR%" --target all
if errorlevel 1 (
    echo Release build failed.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%scripts\new-release-package.ps1" -BuildDirectory "%RELEASE_DIR%" -QtRoot "%QT_ROOT%" -QpdfRoot "%RELEASE_DIR%\qpdf12.3.2" -OutputDirectory "%OUTPUT_DIR%"
if errorlevel 1 (
    echo Release packaging failed. See the error above.
    pause
    exit /b 1
)

echo Release ZIP created and verified in %OUTPUT_DIR%
pause
