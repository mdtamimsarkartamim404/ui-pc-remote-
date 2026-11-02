@echo off
rem Change the token (must be the SAME as in the CYD web panel > System). The agent restarts by itself.
cd /d "%~dp0"
python pc_agent.py --set-token
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'python*' -and $_.CommandLine -like '*pc_agent.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"
start "" wscript.exe "%~dp0pc_agent_start.vbs"
echo Agent restarted with the new token.
pause
