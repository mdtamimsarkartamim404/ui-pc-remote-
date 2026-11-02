CYD PC agent (monitor + remote) - hidden background version
===========================================================
Everything is in this folder. Python 3 must be installed ("Add python to PATH").

1) Double-click  install_autostart.bat   (ONCE)
   - installs psutil, asks for the token the first time (type the SAME token as in the CYD web panel > System),
   - starts the agent HIDDEN (no cmd window) and at every Windows login.
   - Windows Firewall question: allow Python on PRIVATE networks.
2) show_log.bat      -> open pc_agent.log (port, OK / DENIED lines for every button press)
3) set_token.bat     -> change the token (agent restarts by itself)
4) stop_pc_agent.bat -> stop the agent         remove_autostart.bat -> no auto-start
5) pc_agent_start.vbs -> start by hand (harmless if already running)

Rules
- Token EMPTY = shutdown / restart / sleep / app: / url: are refused (safety). Normal keys work.
- Port 8080 is used; if busy the next free port (max +12) - the CYD finds it by itself.
- Everything the agent prints goes to pc_agent.log (cleared when > 1 MB). Config: agent_config.json.
- If you change pc_agent.py: stop_pc_agent.bat, then pc_agent_start.vbs.
