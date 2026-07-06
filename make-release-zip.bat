@echo off
setlocal

set "SOURCE_DIR=%~dp0"
set "SOURCE_DIR=%SOURCE_DIR:~0,-1%"
for %%I in ("%SOURCE_DIR%") do set "FOLDER_NAME=%%~nxI"
set "ZIP_NAME=%FOLDER_NAME%.zip"

if not "%~1"=="" (
    set "ZIP_NAME=%~1"
)

if /I not "%ZIP_NAME:~-4%"==".zip" (
    set "ZIP_NAME=%ZIP_NAME%.zip"
)

for %%I in ("%SOURCE_DIR%") do set "PARENT_DIR=%%~dpI"
set "ZIP_PATH=%PARENT_DIR%%ZIP_NAME%"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$source = '%SOURCE_DIR%';" ^
  "$zip = '%ZIP_PATH%';" ^
  "$script = '%~f0';" ^
  "$excluded = @('data', 'schedules');" ^
  "$root = (Resolve-Path -LiteralPath $source).Path;" ^
  "$items = Get-ChildItem -LiteralPath $root -Force | Where-Object { $excluded -notcontains $_.Name -and $_.FullName -ne $zip -and $_.FullName -ne $script };" ^
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
