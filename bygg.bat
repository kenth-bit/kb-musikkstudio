@echo off
rem ============================================================
rem  bygg.bat - bygger KB Musikkstudio på Windows (Visual Studio 2022)
rem  Dobbeltklikk på filen, eller kjør den fra en terminal.
rem  Valgfritt: bygg.bat test   (kjører testene etter bygget)
rem ============================================================
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo.
echo === KB Musikkstudio: bygger programmet ===
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo FEIL: Fant ikke CMake. Installer CMake fra https://cmake.org/download/
    echo       og velg "Add CMake to the system PATH" under installasjonen.
    goto feil
)

rem Peker du på ASIO SDK med KB_ASIO_SDK_DIR, eller legger den i third_party\asiosdk, brukes ASIO.
if not exist "third_party\asiosdk\common\iasiodrv.h" if "%KB_ASIO_SDK_DIR%"=="" (
    echo TIPS: Fant ikke ASIO SDK. Programmet bygges med Windows Audio i stedet for ASIO.
    echo       Se README.md for hvordan du slår på ASIO med Scarlett.
    echo.
)

echo Setter opp bygget. Første gang lastes JUCE ned, det kan ta noen minutter.
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto feil

echo.
echo Bygger i Release. Dette kan ta noen minutter første gang.
cmake --build build --config Release --parallel
if errorlevel 1 goto feil

if /i "%~1"=="test" (
    echo.
    echo Kjører testene.
    ctest --test-dir build -C Release --output-on-failure
    if errorlevel 1 goto feil
)

echo.
echo === Ferdig! ===
echo Start programmet med start.bat, eller kjør lag-snarvei.bat for å få en snarvei på skrivebordet.
echo.
pause
exit /b 0

:feil
echo.
echo === Noe gikk galt. Les meldingene over. ===
echo Hjelp finner du i README.md under "Hvis noe går galt".
echo.
pause
exit /b 1
