// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class FaldoranPrimeMMO : ModuleRules
{
	public FaldoranPrimeMMO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// --- libpq (PostgreSQL C API) integration ---
		string ThirdPartyPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "libpq");
		string LibpqIncludePath = Path.Combine(ThirdPartyPath, "include");
		string LibpqLibPath = Path.Combine(ThirdPartyPath, "lib");
		string LibpqBinPath = Path.Combine(ThirdPartyPath, "bin");

		// Add header search path
		PublicIncludePaths.Add(LibpqIncludePath);

		// Link libpq.lib
		PublicAdditionalLibraries.Add(Path.Combine(LibpqLibPath, "libpq.lib"));

		// Copy all required DLLs next to the executable at build time
		string[] LibpqDlls = new string[]
		{
			"libpq.dll",
			"libssl-3-x64.dll",
			"libcrypto-3-x64.dll",
			"libintl-9.dll",
			"libiconv-2.dll"
		};

		foreach (string DllName in LibpqDlls)
		{
			string DllPath = Path.Combine(LibpqBinPath, DllName);
			RuntimeDependencies.Add("$(BinaryOutputDir)/" + DllName, DllPath);
			PublicDelayLoadDLLs.Add(DllName);
		}
	}
}
