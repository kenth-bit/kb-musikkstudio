@echo off
rem ============================================================
rem  lag-snarvei.bat - lager en snarvei med ikon på skrivebordet
rem  Kjør bygg.bat først, så finnes programmet som snarveien peker på.
rem ============================================================
chcp 65001 >nul
setlocal
cd /d "%~dp0"

set "EXE=%~dp0build\KBMusikkstudio_artefacts\Release\KB Musikkstudio.exe"
set "IKON=%~dp0resources\icon.ico"

if not exist "%EXE%" (
    echo FEIL: Fant ikke programmet. Kjør bygg.bat først, og prøv så igjen.
    pause
    exit /b 1
)

echo Lager snarvei på skrivebordet.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$skrivebord = [Environment]::GetFolderPath('Desktop');" ^
  "$snarvei = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $skrivebord 'KB Musikkstudio.lnk'));" ^
  "$snarvei.TargetPath = $env:EXE;" ^
  "$snarvei.WorkingDirectory = (Split-Path $env:EXE);" ^
  "$snarvei.IconLocation = $env:IKON;" ^
  "$snarvei.Description = 'KB Musikkstudio';" ^
  "$snarvei.Save()"

if errorlevel 1 (
    echo FEIL: Klarte ikke å lage snarveien.
    pause
    exit /b 1
)

echo Ferdig! Du finner "KB Musikkstudio" på skrivebordet.
pause
exit /b 0
