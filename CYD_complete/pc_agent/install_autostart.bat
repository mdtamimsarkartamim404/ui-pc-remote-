@echo off
rem Sets the token (first time), starts the CYD PC agent HIDDEN now, and at every Windows login.
cd /d "%~dp0"
where python >nul 2>nul || (echo Python 3 is not installed: https://www.python.org/downloads/ ^(tick "Add python to PATH"^) & pause & exit /b 1)
if not exist "%~dp0pc_agent.py" (echo pc_agent.py not found in this folder & pause & exit /b 1)
python -m pip install --quiet --disable-pip-version-check psutil
python -c "import json,sys;sys.exit(0 if json.load(open('agent_config.json')).get('token') else 1)" >nul 2>nul
if errorlevel 1 (
  echo.
  echo First time: type the SAME token that is in the CYD web panel ^(System tab^).
  python pc_agent.py --set-token
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut($env:APPDATA+'\Microsoft\Windows\Start Menu\Programs\Startup\CYD PC Agent.lnk');$s.TargetPath='%~dp0pc_agent_start.vbs';$s.WorkingDirectory='%~dp0';$s.Save()"
if errorlevel 1 (echo Could not create the startup shortcut & pause & exit /b 1)
start "" wscript.exe "%~dp0pc_agent_start.vbs"
echo.
echo Done. The agent runs hidden now and starts automatically at every login.
echo First time only: if Windows asks about the Firewall, allow Python on PRIVATE networks.
echo Log file: %~dp0pc_agent.log   ^(open it with show_log.bat^)
echo.
pause
