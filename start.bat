@echo off
rem ============================================================
rem  start.bat - starter KB Musikkstudio
rem  Har du ikke bygget ennå, kjører den bygg.bat først.
rem ============================================================
chcp 65001 >nul
setlocal
cd /d "%~dp0"

set "EXE=build\KBMusikkstudio_artefacts\Release\KB Musikkstudio.exe"

if not exist "%EXE%" (
    echo Programmet er ikke bygget ennå. Bygger nå.
    call bygg.bat
)

if not exist "%EXE%" (
    echo FEIL: Fant ikke "%EXE%". Kjør bygg.bat og se etter feilmeldinger.
    pause
    exit /b 1
)

start "" "%EXE%"
exit /b 0
