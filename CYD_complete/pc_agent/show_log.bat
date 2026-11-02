@echo off
rem Opens the agent log (shows DENIED / port / commands).
cd /d "%~dp0"
set "AGENTDIR=%~dp0"
if exist "%~dp0agent_path.txt" set /p AGENTDIR=<"%~dp0agent_path.txt"
if exist "%AGENTDIR%\pc_agent.log" (start "" notepad "%AGENTDIR%\pc_agent.log") else if exist "%~dp0..\pc_agent.log" (start "" notepad "%~dp0..\pc_agent.log") else (echo No pc_agent.log found yet - start the agent first. & pause)
