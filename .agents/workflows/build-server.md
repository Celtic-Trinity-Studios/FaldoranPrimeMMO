---
description: How to build the Unreal Engine project (editor/development build)
---
// turbo-all

## Build the FaldoranPrimeMMO Editor (Development)

> [!CAUTION]
> **ALWAYS use the Installed Build** at `E:\UEInstalled\Windows\`.
> **NEVER use** the source build at `E:\UESource\UnrealEngine\` — it forces a full plugin recompile (~10+ minutes) every time.

1. Run the build script from the project root:
```
.\BuildProject.bat
```
Working directory: `e:\FaldoranPrimeMMO`

This invokes UnrealBuildTool from the **Installed Build** at:
```
E:\UEInstalled\Windows\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe
```

Target: `FaldoranPrimeMMOEditor` (Win64 Development)

Build typically takes ~20-60 seconds (game module only). A successful build ends with `Result: Succeeded` and exit code 0.

## Engine Paths (IMPORTANT)

| Path | Purpose | Use for builds? |
|------|---------|----------------|
| `E:\UEInstalled\Windows\` | **Installed Build** (pre-compiled engine + plugins) | ✅ YES — always use this |
| `E:\UESource\UnrealEngine\` | Source build (full engine source) | ❌ NO — causes full plugin rebuild |

## Other Build Targets

- `FaldoranPrimeMMOServer.Target.cs` — Dedicated server build
- `FaldoranPrimeMMOClient.Target.cs` — Client build
- `FaldoranPrimeMMOEditor.Target.cs` — Editor build (default)
