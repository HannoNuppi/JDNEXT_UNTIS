@echo off
setlocal
title JDNEXT Firebase Backend Deployment

where firebase >nul 2>&1
if errorlevel 1 (
  echo Firebase CLI wurde nicht gefunden.
  echo Installieren mit: npm install -g firebase-tools
  pause
  exit /b 1
)

echo.
echo === JDNEXT / next-untis-plus ===
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
