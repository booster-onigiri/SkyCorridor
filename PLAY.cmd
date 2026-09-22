@echo off
setlocal
cd /d "%~dp0"
"%~dp0Windows\EndlessWorld.exe" -EWDataDir="%~dp0PlayData" %*
