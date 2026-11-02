@echo off
rem Stops starting the CYD PC agent at login.
del "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\CYD PC Agent.lnk" 2>nul
echo Autostart removed. (Use stop_pc_agent.bat to also stop the running agent.)
pause
