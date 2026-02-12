// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMChunkOverlay.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"


// ===================================================================
//  Static Members
// ===================================================================

FString FPMChunkOverlayManager::SaveDir;

// ===================================================================
//  Initialization
// ===================================================================

void FPMChunkOverlayManager::Initialize(const FString &SaveDirectory) {
  SaveDir = SaveDirectory;

  // Ensure the directory exists
  IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!PlatformFile.DirectoryExists(*SaveDir)) {
    PlatformFile.CreateDirectoryTree(*SaveDir);
  }

  UE_LOG(LogTemp, Log,
         TEXT("FPM: Chunk overlay manager initialized. SaveDir=%s"), *SaveDir);
}

// ===================================================================
//  File Path Helper
// ===================================================================

FString
FPMChunkOverlayManager::GetOverlayFilePath(const FFPMChunkCoord &Coord) {
  return FPaths::Combine(
      SaveDir, FString::Printf(TEXT("Chunk_%d_%d.json"), Coord.X, Coord.Y));
}

// ===================================================================
//  Load / Save
// ===================================================================

bool FPMChunkOverlayManager::LoadOverlay(const FFPMChunkCoord &Coord,
                                         FFPMChunkOverlay &OutOverlay) {
  OutOverlay.Coord = Coord;
  OutOverlay.Modifications.Empty();
  OutOverlay.bIsLoaded = true;

  const FString FilePath = GetOverlayFilePath(Coord);

  FString JsonString;
  if (!FFileHelper::LoadFileToString(JsonString, *FilePath)) {
    // No overlay file — chunk is unmodified
    return false;
  }

  TSharedPtr<FJsonObject> RootObject;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

  if (!FJsonSerializer::Deserialize(Reader, RootObject) ||
      !RootObject.IsValid()) {
    UE_LOG(LogTemp, Warning, TEXT("FPM: Failed to parse overlay file: %s"),
           *FilePath);
    return false;
  }

  const TArray<TSharedPtr<FJsonValue>> *ModsArray;
  if (RootObject->TryGetArrayField(TEXT("modifications"), ModsArray)) {
    for (const TSharedPtr<FJsonValue> &ModValue : *ModsArray) {
      const TSharedPtr<FJsonObject> ModObj = ModValue->AsObject();
      if (!ModObj.IsValid())
        continue;

      FFPMVertexModification Mod;
      Mod.VertexIndex = ModObj->GetIntegerField(TEXT("idx"));
      Mod.HeightDelta = ModObj->GetNumberField(TEXT("hd"));
      Mod.bBiomeOverridden = ModObj->GetBoolField(TEXT("bo"));
      if (Mod.bBiomeOverridden) {
        Mod.OverriddenBiome =
            static_cast<EFPMBiome>(ModObj->GetIntegerField(TEXT("ob")));
      }

      OutOverlay.Modifications.Add(Mod);
    }
  }

  UE_LOG(LogTemp, Verbose,
         TEXT("FPM: Loaded overlay for chunk %s — %d modifications"),
         *Coord.ToString(), OutOverlay.Modifications.Num());

  return true;
}

bool FPMChunkOverlayManager::SaveOverlay(const FFPMChunkOverlay &Overlay) {
  if (!Overlay.HasModifications()) {
    // Nothing to save — delete the file if it exists
    DeleteOverlay(Overlay.Coord);
    return true;
  }

  TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());

  // Chunk coordinate metadata
  RootObject->SetNumberField(TEXT("chunkX"), Overlay.Coord.X);
  RootObject->SetNumberField(TEXT("chunkY"), Overlay.Coord.Y);

  // Modifications array
  TArray<TSharedPtr<FJsonValue>> ModsArray;
  for (const FFPMVertexModification &Mod : Overlay.Modifications) {
    TSharedPtr<FJsonObject> ModObj = MakeShareable(new FJsonObject());
    ModObj->SetNumberField(TEXT("idx"), Mod.VertexIndex);
    ModObj->SetNumberField(TEXT("hd"), Mod.HeightDelta);
    ModObj->SetBoolField(TEXT("bo"), Mod.bBiomeOverridden);
    if (Mod.bBiomeOverridden) {
      ModObj->SetNumberField(TEXT("ob"),
                             static_cast<int32>(Mod.OverriddenBiome));
    }
    ModsArray.Add(MakeShareable(new FJsonValueObject(ModObj)));
  }
  RootObject->SetArrayField(TEXT("modifications"), ModsArray);

  FString OutputString;
  TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&OutputString);
  FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

  const FString FilePath = GetOverlayFilePath(Overlay.Coord);
  if (!FFileHelper::SaveStringToFile(OutputString, *FilePath)) {
    UE_LOG(LogTemp, Error, TEXT("FPM: Failed to save overlay file: %s"),
           *FilePath);
    return false;
  }

  UE_LOG(LogTemp, Verbose,
         TEXT("FPM: Saved overlay for chunk %s — %d modifications"),
         *Overlay.Coord.ToString(), Overlay.Modifications.Num());
  return true;
}

// ===================================================================
//  Apply Overlay
// ===================================================================

void FPMChunkOverlayManager::ApplyOverlay(const FFPMChunkOverlay &Overlay,
                                          FFPMChunkHeightmapData &InOutData) {
  if (!Overlay.HasModifications() || !InOutData.bIsValid) {
    return;
  }

  for (const FFPMVertexModification &Mod : Overlay.Modifications) {
    if (Mod.VertexIndex < 0 ||
        Mod.VertexIndex >= InOutData.HeightValues.Num()) {
      continue;
    }

    // Apply height delta
    InOutData.HeightValues[Mod.VertexIndex] += Mod.HeightDelta;

    // Apply biome override
    if (Mod.bBiomeOverridden) {
      InOutData.BiomeValues[Mod.VertexIndex] = Mod.OverriddenBiome;
    }
  }
}

// ===================================================================
//  Modification Helpers
// ===================================================================

void FPMChunkOverlayManager::AddHeightModification(FFPMChunkOverlay &Overlay,
                                                   int32 VertexIndex,
                                                   float HeightDelta) {
  // Check if this vertex is already modified
  for (FFPMVertexModification &Mod : Overlay.Modifications) {
    if (Mod.VertexIndex == VertexIndex) {
      Mod.HeightDelta = HeightDelta;
      return;
    }
  }

  // New modification
  FFPMVertexModification NewMod;
  NewMod.VertexIndex = VertexIndex;
  NewMod.HeightDelta = HeightDelta;
  Overlay.Modifications.Add(NewMod);
}

void FPMChunkOverlayManager::AddBiomeOverride(FFPMChunkOverlay &Overlay,
                                              int32 VertexIndex,
                                              EFPMBiome NewBiome) {
  for (FFPMVertexModification &Mod : Overlay.Modifications) {
    if (Mod.VertexIndex == VertexIndex) {
      Mod.bBiomeOverridden = true;
      Mod.OverriddenBiome = NewBiome;
      return;
    }
  }

  FFPMVertexModification NewMod;
  NewMod.VertexIndex = VertexIndex;
  NewMod.bBiomeOverridden = true;
  NewMod.OverriddenBiome = NewBiome;
  Overlay.Modifications.Add(NewMod);
}

void FPMChunkOverlayManager::DeleteOverlay(const FFPMChunkCoord &Coord) {
  const FString FilePath = GetOverlayFilePath(Coord);
  IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (PlatformFile.FileExists(*FilePath)) {
    PlatformFile.DeleteFile(*FilePath);
    UE_LOG(LogTemp, Verbose, TEXT("FPM: Deleted overlay for chunk %s"),
           *Coord.ToString());
  }
}
