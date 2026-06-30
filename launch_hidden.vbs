' Shanghai Metro Route Planner — Silent Launch Script
' Hides all console windows, only shows browser frontend
'
' Usage: Double-click this file to start
' To stop: end python.exe and backend-service.exe in Task Manager

Dim shell, fso, rootDir
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
rootDir = fso.GetParentFolderName(WScript.ScriptFullName)

' ===== 1. Start HTTP Server (fully hidden) =====
shell.CurrentDirectory = rootDir & "\frontend\web"

' Try pythonw.exe first (no console window), fallback to python.exe
Dim pythonCmd
pythonCmd = "pythonw.exe -m http.server 8080"
Dim ret
ret = shell.Run(pythonCmd, 0, False)

If ret <> 0 Then
    pythonCmd = "python.exe -m http.server 8080"
    shell.Run pythonCmd, 0, False
End If

' Wait for server to be ready
WScript.Sleep 2000

' ===== 2. Open browser =====
shell.Run "http://localhost:8080"

' ===== 3. Start backend service (fully hidden) =====
Dim metroPath

' backend-service.exe in project root
metroPath = rootDir & "\backend-service.exe"

If fso.FileExists(metroPath) Then
    shell.Run chr(34) & metroPath & chr(34) & " --hidden", 0, False
End If
