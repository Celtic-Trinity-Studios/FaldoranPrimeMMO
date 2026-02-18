using UnrealBuildTool;
using System;
using System.IO;

internal static class SapientPrecompiledBinaryUtils
{
    public static void Configure(ModuleRules Rules, ReadOnlyTargetRules Target, string ModuleName)
    {
        string PluginPath = Path.GetFullPath(Path.Combine(Rules.ModuleDirectory, "../../"));

        string UEVersion = "5.6";
        try
        {
            UEVersion = $"{Target.Version.MajorVersion}.{Target.Version.MinorVersion}";
        }
        catch (Exception)
        {
        }

        string PrecompiledPath = Path.Combine(PluginPath, "Source", ModuleName, "ThirdParty", "PrecompiledBinaries", $"UE_{UEVersion}", Target.Platform.ToString());
        if (!Directory.Exists(PrecompiledPath))
        {
            PrecompiledPath = Path.Combine(PluginPath, "Source", ModuleName, "ThirdParty", "PrecompiledBinaries", Target.Platform.ToString());
        }

        string BinariesPath = Path.Combine(PluginPath, "Binaries", Target.Platform.ToString());
        if (!Directory.Exists(BinariesPath))
        {
            Directory.CreateDirectory(BinariesPath);
        }

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string ConfigName = Target.Configuration.ToString();

            string DllName = Target.Configuration == UnrealTargetConfiguration.DebugGame
                ? $"UnrealEditor-{ModuleName}-Win64-DebugGame.dll"
                : $"UnrealEditor-{ModuleName}.dll";

            string PdbName = Target.Configuration == UnrealTargetConfiguration.DebugGame
                ? $"UnrealEditor-{ModuleName}-Win64-DebugGame.pdb"
                : $"UnrealEditor-{ModuleName}.pdb";

            string LibName = Target.Configuration == UnrealTargetConfiguration.DebugGame
                ? $"UnrealEditor-{ModuleName}-Win64-DebugGame.lib"
                : $"UnrealEditor-{ModuleName}.lib";

            string SourceDll = Path.Combine(PrecompiledPath, DllName);
            string DestDll = Path.Combine(BinariesPath, DllName);
            if (File.Exists(SourceDll))
            {
                bool ShouldCopy = true;
                if (File.Exists(DestDll))
                {
                    try
                    {
                        var SourceInfo = new FileInfo(SourceDll);
                        var DestInfo = new FileInfo(DestDll);
                        
                        if (SourceInfo.Length == DestInfo.Length && 
                            SourceInfo.LastWriteTime == DestInfo.LastWriteTime)
                        {
                            ShouldCopy = false;
                            System.Console.WriteLine($"{ModuleName}: {ConfigName} DLL already up-to-date, skipping copy");
                        }
                    }
                    catch
                    {
                    }
                }
                
                if (ShouldCopy)
                {
                    try
                    {
                        File.Copy(SourceDll, DestDll, true);
                        System.Console.WriteLine($"{ModuleName}: Copied {ConfigName} DLL from PrecompiledBinaries");
                    }
                    catch (System.IO.IOException Ex)
                    {
                        if (Ex.Message.Contains("being used by another process") || 
                            Ex.Message.Contains("cannot access the file"))
                        {
                            System.Console.WriteLine($"{ModuleName}: {ConfigName} DLL is locked (Live Coding active?), skipping copy. This is normal during hot reload.");
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }

            string SourcePdb = Path.Combine(PrecompiledPath, PdbName);
            string DestPdb = Path.Combine(BinariesPath, PdbName);
            if (File.Exists(SourcePdb))
            {
                bool ShouldCopy = true;
                if (File.Exists(DestPdb))
                {
                    try
                    {
                        var SourceInfo = new FileInfo(SourcePdb);
                        var DestInfo = new FileInfo(DestPdb);
                        
                        if (SourceInfo.Length == DestInfo.Length && 
                            SourceInfo.LastWriteTime == DestInfo.LastWriteTime)
                        {
                            ShouldCopy = false;
                            System.Console.WriteLine($"{ModuleName}: {ConfigName} PDB already up-to-date, skipping copy");
                        }
                    }
                    catch
                    {
                    }
                }
                
                if (ShouldCopy)
                {
                    try
                    {
                        File.Copy(SourcePdb, DestPdb, true);
                        System.Console.WriteLine($"{ModuleName}: Copied {ConfigName} PDB from PrecompiledBinaries");
                    }
                    catch (System.IO.IOException Ex)
                    {
                        if (Ex.Message.Contains("being used by another process") || 
                            Ex.Message.Contains("cannot access the file"))
                        {
                            System.Console.WriteLine($"{ModuleName}: {ConfigName} PDB is locked, skipping copy.");
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }

            string PrecompiledLib = Path.Combine(PrecompiledPath, LibName);
            if (File.Exists(PrecompiledLib))
            {
                string IntermediateLibPath = Path.Combine(
                    PluginPath,
                    "Intermediate",
                    "Build",
                    Target.Platform.ToString(),
                    "x64",
                    "UnrealEditor",
                    ConfigName,
                    ModuleName,
                    LibName);

                string IntermediateLibDir = Path.GetDirectoryName(IntermediateLibPath);
                if (!Directory.Exists(IntermediateLibDir))
                {
                    Directory.CreateDirectory(IntermediateLibDir);
                }

                bool ShouldCopy = true;
                if (File.Exists(IntermediateLibPath))
                {
                    try
                    {
                        var SourceInfo = new FileInfo(PrecompiledLib);
                        var DestInfo = new FileInfo(IntermediateLibPath);
                        
                        if (SourceInfo.Length == DestInfo.Length && 
                            SourceInfo.LastWriteTime == DestInfo.LastWriteTime)
                        {
                            ShouldCopy = false;
                            System.Console.WriteLine($"{ModuleName}: {ConfigName} LIB already up-to-date, skipping copy");
                        }
                    }
                    catch
                    {
                    }
                }
                
                if (ShouldCopy)
                {
                    try
                    {
                        File.Copy(PrecompiledLib, IntermediateLibPath, true);
                        System.Console.WriteLine($"{ModuleName}: Copied {ConfigName} LIB from PrecompiledBinaries");
                    }
                    catch (System.IO.IOException Ex)
                    {
                        if (Ex.Message.Contains("being used by another process") || 
                            Ex.Message.Contains("cannot access the file"))
                        {
                            System.Console.WriteLine($"{ModuleName}: {ConfigName} LIB is locked, skipping copy.");
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
                Rules.PublicAdditionalLibraries.Add(PrecompiledLib);
            }

            Rules.RuntimeDependencies.Add(DestDll, StagedFileType.NonUFS);
            if (File.Exists(DestPdb))
            {
                Rules.RuntimeDependencies.Add(DestPdb, StagedFileType.DebugNonUFS);
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string DylibName = $"UnrealEditor-{ModuleName}.dylib";
            string SourceDylib = Path.Combine(PrecompiledPath, DylibName);
            string DestDylib = Path.Combine(BinariesPath, DylibName);
            if (File.Exists(SourceDylib))
            {
                bool ShouldCopy = true;
                if (File.Exists(DestDylib))
                {
                    try
                    {
                        var SourceInfo = new FileInfo(SourceDylib);
                        var DestInfo = new FileInfo(DestDylib);
                        
                        if (SourceInfo.Length == DestInfo.Length && 
                            SourceInfo.LastWriteTime == DestInfo.LastWriteTime)
                        {
                            ShouldCopy = false;
                            System.Console.WriteLine($"{ModuleName}: DYLIB already up-to-date, skipping copy");
                        }
                    }
                    catch
                    {
                    }
                }
                
                if (ShouldCopy)
                {
                    try
                    {
                        File.Copy(SourceDylib, DestDylib, true);
                        System.Console.WriteLine($"{ModuleName}: Copied DYLIB from PrecompiledBinaries");
                    }
                    catch (System.IO.IOException Ex)
                    {
                        if (Ex.Message.Contains("being used by another process") || 
                            Ex.Message.Contains("cannot access the file"))
                        {
                            System.Console.WriteLine($"{ModuleName}: DYLIB is locked, skipping copy.");
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }
            Rules.RuntimeDependencies.Add(DestDylib, StagedFileType.NonUFS);
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string SoName = $"UnrealEditor-{ModuleName}.so";
            string SourceSo = Path.Combine(PrecompiledPath, SoName);
            string DestSo = Path.Combine(BinariesPath, SoName);
            if (File.Exists(SourceSo))
            {
                bool ShouldCopy = true;
                if (File.Exists(DestSo))
                {
                    try
                    {
                        var SourceInfo = new FileInfo(SourceSo);
                        var DestInfo = new FileInfo(DestSo);
                        
                        if (SourceInfo.Length == DestInfo.Length && 
                            SourceInfo.LastWriteTime == DestInfo.LastWriteTime)
                        {
                            ShouldCopy = false;
                            System.Console.WriteLine($"{ModuleName}: SO already up-to-date, skipping copy");
                        }
                    }
                    catch
                    {
                    }
                }
                
                if (ShouldCopy)
                {
                    try
                    {
                        File.Copy(SourceSo, DestSo, true);
                        System.Console.WriteLine($"{ModuleName}: Copied SO from PrecompiledBinaries");
                    }
                    catch (System.IO.IOException Ex)
                    {
                        if (Ex.Message.Contains("being used by another process") || 
                            Ex.Message.Contains("cannot access the file"))
                        {
                            System.Console.WriteLine($"{ModuleName}: SO is locked, skipping copy.");
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }
            Rules.RuntimeDependencies.Add(DestSo, StagedFileType.NonUFS);
        }
    }
}
