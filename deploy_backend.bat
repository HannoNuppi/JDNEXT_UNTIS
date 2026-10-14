@echo off
setlocal
title Legacy JDNEXT / next-untis-plus Deployment

where firebase >nul 2>&1
if errorlevel 1 (
  echo Firebase CLI wurde nicht gefunden.
  echo Installieren mit: npm install -g firebase-tools
  pause
  exit /b 1
)

echo.
echo === LEGACY JDNEXT / next-untis-plus ===
echo Hinweis: Dieses Skript betrifft NICHT das Projekt tobiservices.
echo Neue Hausaufgaben werden in tobiservices gespeichert. Dessen Regeln liegen im Repository
echo HannoNuppi/Tobiservices-Account und werden dort mit deploy_tobiservices_rules.bat veroeffentlicht.
echo.
firebase use next-untis-plus
if errorlevel 1 goto :fail

echo.
echo === Discord Bot Token als Secret setzen ===
echo Der Token wird NICHT in eine Datei geschrieben.
echo.
firebase functions:secrets:set DISCORD_BOT_TOKEN
if errorlevel 1 goto :fail

echo.
echo === Functions + Firestore deployen ===
firebase deploy --only functions,firestore
if errorlevel 1 goto :fail

echo.
echo Fertig.
pause
exit /b 0

:fail
echo.
echo Deployment fehlgeschlagen.
pause
exit /b 1
