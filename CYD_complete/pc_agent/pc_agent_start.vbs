' CYD PC agent - starts pc_agent.py HIDDEN (no cmd window). Double-click this file.
' It looks for pc_agent.py in: this folder, the parent folder, or the folder saved in agent_path.txt.
' If it cannot find it, it asks you to pick the folder ONCE and remembers it.
' Output goes to pc_agent.log (in the pc_agent.py folder). Running it twice is harmless.
Option Explicit
Dim sh, fso, here, agentDir, cfg, ts, wmi, p, rc, log
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
here = fso.GetParentFolderName(WScript.ScriptFullName)
cfg = here & "\agent_path.txt"
agentDir = ""

If fso.FileExists(here & "\pc_agent.py") Then
  agentDir = here
ElseIf fso.FileExists(fso.GetParentFolderName(here) & "\pc_agent.py") Then
  agentDir = fso.GetParentFolderName(here)
ElseIf fso.FileExists(cfg) Then
  Set ts = fso.OpenTextFile(cfg, 1)
  If Not ts.AtEndOfStream Then agentDir = Trim(ts.ReadLine)
  ts.Close
  If Not fso.FileExists(agentDir & "\pc_agent.py") Then agentDir = ""
End If

If agentDir = "" Then
  MsgBox "pc_agent.py was not found." & vbCrLf & vbCrLf & "Next: select the folder that contains pc_agent.py (the one where you used run_pc_agent.bat).", 64, "CYD PC agent"
  agentDir = PickFolder()
  If agentDir = "" Then WScript.Quit 1
  If Not fso.FileExists(agentDir & "\pc_agent.py") Then
    MsgBox "There is no pc_agent.py in:" & vbCrLf & agentDir, 16, "CYD PC agent"
    WScript.Quit 1
  End If
  Set ts = fso.CreateTextFile(cfg, True)
  ts.WriteLine agentDir
  ts.Close
End If
sh.CurrentDirectory = agentDir

' already running? then do nothing
Set wmi = GetObject("winmgmts:\\.\root\cimv2")
For Each p In wmi.ExecQuery("SELECT CommandLine FROM Win32_Process WHERE Name='python.exe' OR Name='pythonw.exe'")
  If InStr(LCase(p.CommandLine & ""), "pc_agent.py") > 0 Then WScript.Quit 0
Next

' python installed?
rc = sh.Run("cmd /c python --version >nul 2>nul", 0, True)
If rc <> 0 Then
  MsgBox "Python 3 is not installed (tick ""Add python to PATH"" in the installer):" & vbCrLf & "https://www.python.org/downloads/", 16, "CYD PC agent"
  WScript.Quit 1
End If

' psutil is installed only when it is missing
rc = sh.Run("cmd /c python -c ""import psutil"" >nul 2>nul", 0, True)
If rc <> 0 Then sh.Run "cmd /c python -m pip install --quiet --disable-pip-version-check psutil", 0, True

' keep the log small (> 1 MB = start a new one)
log = agentDir & "\pc_agent.log"
If fso.FileExists(log) Then
  If fso.GetFile(log).Size > 1000000 Then fso.DeleteFile log, True
End If

' start hidden (window style 0), do not wait
sh.Run "cmd /c python -u pc_agent.py >> ""pc_agent.log"" 2>&1", 0, False

Function PickFolder()
  Dim app, f
  Set app = CreateObject("Shell.Application")
  Set f = app.BrowseForFolder(0, "Select the folder that contains pc_agent.py", &H51, 0)
  If f Is Nothing Then
    PickFolder = ""
  Else
    PickFolder = f.Self.Path
  End If
End Function
