@echo off
rem Stops the hidden CYD PC agent.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'python*' -and $_.CommandLine -like '*pc_agent.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"
echo PC agent stopped.
pause
