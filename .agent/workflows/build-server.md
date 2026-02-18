---
description: How to build and deploy the dedicated server
---

# Building and Deploying the FaldoranPrime Dedicated Server

## Prerequisites
- Unreal Engine installed at `E:\UEInstalled`
- Project at `e:\FaldoranPrimeMMO`
- Server target file exists: `Source/FaldoranPrimeMMOServer.Target.cs`

## 1. Build the Dedicated Server Binary

The server is a headless (no GPU/rendering) executable.  Build it with:

// turbo
```powershell
& "E:\UEInstalled\Windows\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" FaldoranPrimeMMOServer Win64 Development "-Project=e:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject" -WaitMutex -NoHotReloadFromIDE
```

The server binary will be at:
`e:\FaldoranPrimeMMO\Binaries\Win64\FaldoranPrimeMMOServer.exe`

## 2. Package the Server for Deployment

To create a standalone package you can copy to your server machine:

```powershell
& "E:\UEInstalled\Windows\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" FaldoranPrimeMMOServer Win64 Shipping "-Project=e:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject" -WaitMutex
```

Or use RunUAT for a full cook + stage:

```powershell
& "E:\UEInstalled\Windows\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="e:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject" -noP4 -platform=Win64 -clientconfig=Development -serverconfig=Development -cook -server -noclient -build -stage -pak -stagingdirectory="e:\FaldoranPrimeMMO\PackagedServer"
```

## 3. Running the Dedicated Server

**On the server machine (152.86.63.18):**

```
FaldoranPrimeMMOServer.exe L_PrototypeWorld -log -port=7777
```

Key command line args:
- `-log` — show log console window
- `-port=7777` — listen port (default)
- `-MaxPlayers=100` — max concurrent players

## 4. Client Auto-Connect

The client reads `Config/DefaultGame.ini` section `[FPM.Server]`:

```ini
[FPM.Server]
ServerIP=152.86.63.18
ServerPort=7777
bAutoConnect=false
```

- Set `bAutoConnect=true` **in packaged client builds** to auto-connect on startup
- Keep `bAutoConnect=false` for editor/PIE testing (uses listen-server)
- You can also call `ConnectToDedicatedServer()` from Blueprint/UI

## 5. Firewall / Network

Ensure port **7777 UDP** and **TCP** are open on the server:
- UDP 7777 — game traffic
- TCP 7777 — initial connection handshake

## 6. Linux Server (if your server runs Linux)

Change the platform to Linux:

```powershell
& "E:\UEInstalled\Windows\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="e:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject" -noP4 -platform=Linux -clientconfig=Development -serverconfig=Development -cook -server -noclient -build -stage -pak -stagingdirectory="e:\FaldoranPrimeMMO\PackagedServerLinux"
```

Note: You need the Linux cross-compilation toolchain installed for this.

## 7. Quick Test (PIE)

To test client→server flow in-editor without packaging:
1. Set PIE to "Standalone Game" (not play in editor)
2. Change `bAutoConnect=true` in DefaultGame.ini temporarily
3. Start a listen server via console: `open L_PrototypeWorld?listen`
4. The client will auto-connect after 1 second
