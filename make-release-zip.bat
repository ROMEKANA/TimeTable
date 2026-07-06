@echo off
setlocal

set "PROJECT_DIR=C:\Users\TO\Documents\jukuTimeTable\TimeTable"
set "RELEASE_DIR=C:\Users\TO\Documents\jukuTimeTable\TimeTable\build\Desktop_Qt_6_11_0_MinGW_64_bit-Release"
set "OUTPUT_DIR=C:\Users\TO\Documents\jukuTimeTable\TimeTable\releases"
set "CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
set "WINDEPLOYQT=C:\Qt\6.11.0\mingw_64\bin\windeployqt.exe"
set "APP_EXE=%RELEASE_DIR%\TimeTable.exe"
set "UPDATER_EXE=%RELEASE_DIR%\TimeTableUpdater.exe"

set "PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.0\mingw_64\bin;%PATH%"

if not exist "%CMAKE_EXE%" (
    echo cmake.exe was not found.
    echo %CMAKE_EXE%
    pause
    exit /b 1
)

if not exist "%WINDEPLOYQT%" (
    echo windeployqt.exe was not found.
    echo %WINDEPLOYQT%
    pause
    exit /b 1
)

echo Building Release target...
"%CMAKE_EXE%" --build C:/Users/TO/Documents/jukuTimeTable/TimeTable/build/Desktop_Qt_6_11_0_MinGW_64_bit-Release --target all
if errorlevel 1 (
    echo.
    echo Release build failed.
    pause
    exit /b 1
)

if not exist "%APP_EXE%" (
    echo TimeTable.exe was not found.
    echo %APP_EXE%
    pause
    exit /b 1
)

if not exist "%UPDATER_EXE%" (
    echo TimeTableUpdater.exe was not found.
    echo %UPDATER_EXE%
    pause
    exit /b 1
)

for /f "usebackq delims=" %%V in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$content = Get-Content -Raw -LiteralPath '%PROJECT_DIR%\CMakeLists.txt'; if ($content -match 'project\s*\(\s*TimeTable\s+VERSION\s+([0-9A-Za-z\.\-_]+)') { $matches[1] } else { '0.1.0' }"`) do set "VERSION=%%V"

if "%VERSION%"=="" (
    set "VERSION=0.1.0"
)

set "ZIP_NAME=TimeTable-v%VERSION%-win64.zip"
set "ZIP_PATH=%OUTPUT_DIR%\%ZIP_NAME%"

if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
    if errorlevel 1 (
        echo releases folder could not be created.
        echo %OUTPUT_DIR%
        pause
        exit /b 1
    )
)

echo Deploying Qt files for TimeTable.exe...
"%WINDEPLOYQT%" "%APP_EXE%"
if errorlevel 1 (
    echo.
    echo windeployqt failed for TimeTable.exe.
    pause
    exit /b 1
)

echo.
echo Deploying Qt files for TimeTableUpdater.exe...
"%WINDEPLOYQT%" "%UPDATER_EXE%"
if errorlevel 1 (
    echo.
    echo windeployqt failed for TimeTableUpdater.exe.
    pause
    exit /b 1
)

echo.
echo Creating %ZIP_NAME%...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$source = '%RELEASE_DIR%';" ^
  "$zip = '%ZIP_PATH%';" ^
  "$excluded = @('data', 'schedules', 'schedulePDF', '.cmake', '.lupdate', '.qt', '.qtcreator', '.qtc_clangd', 'CMakeFiles', 'Testing', 'TimeTable_autogen', 'TimeTableUpdater_autogen');" ^
  "$excludedFiles = @('CMakeCache.txt', 'cmake_install.cmake', 'Makefile', 'build.ninja', '.ninja_deps', '.ninja_log');" ^
  "$root = (Resolve-Path -LiteralPath $source).Path;" ^
  "$items = Get-ChildItem -LiteralPath $root -Force | Where-Object { $excluded -notcontains $_.Name -and $excludedFiles -notcontains $_.Name -and $_.FullName -ne $zip };" ^
  "if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force };" ^
  "if ($items.Count -eq 0) { throw 'No files to zip.' }" ^
  "Compress-Archive -LiteralPath $items.FullName -DestinationPath $zip -Force;" ^
  "Write-Host ('Created: ' + $zip);"

if errorlevel 1 (
    echo.
    echo Failed to create zip.
    pause
    exit /b 1
)

echo.
echo Zip created successfully.
echo %ZIP_PATH%
pause
