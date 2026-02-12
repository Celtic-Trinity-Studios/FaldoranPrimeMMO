// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Character/FPMCharacterCreationValidator.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterValidator, Log, All);

// -------------------------------------------------------------------
// Profanity blocklist — server-side only, expandable for production
// -------------------------------------------------------------------
// Kept small for prototype. Production should load from a data file
// or database table for easier updates without recompilation.
static const TArray<FString> ProfanityBlocklist = {
    TEXT("admin"),   TEXT("moderator"), TEXT("gamemaster"), TEXT("fuck"),
    TEXT("shit"),    TEXT("ass"),       TEXT("damn"),       TEXT("bitch"),
    TEXT("bastard"), TEXT("dick"),      TEXT("cock"),       TEXT("cunt"),
    TEXT("nigger"),  TEXT("nigga"),     TEXT("faggot"),     TEXT("retard"),
    TEXT("whore"),   TEXT("slut")};

// -------------------------------------------------------------------
// ValidateRequest
// -------------------------------------------------------------------

bool FFPMCharacterCreationValidator::ValidateRequest(
    const FFPMCharacterCreationRequest &Request,
    EFPMCharacterCreationError &OutErrorCode, FString &OutError) {
  // Validate name first — most common user-facing error
  if (!ValidateName(Request.CharacterName, OutError)) {
    OutErrorCode = EFPMCharacterCreationError::InvalidName;
    return false;
  }

  // Validate appearance values
  if (!ValidateAppearance(Request, OutError)) {
    OutErrorCode = EFPMCharacterCreationError::InvalidAppearance;
    return false;
  }

  // Validate affinity pools (only if provided — empty maps are valid
  // for backward compatibility with the original creation flow)
  if (Request.PlaystyleAffinities.Num() > 0) {
    if (!ValidatePlaystyleAffinities(Request, OutError)) {
      OutErrorCode = EFPMCharacterCreationError::InvalidAffinities;
      return false;
    }
  }

  if (Request.MagicalAffinities.Num() > 0) {
    if (!ValidateMagicalAffinities(Request, OutError)) {
      OutErrorCode = EFPMCharacterCreationError::InvalidAffinities;
      return false;
    }
  }

  OutErrorCode = EFPMCharacterCreationError::None;
  return true;
}

// -------------------------------------------------------------------
// ValidateName
// -------------------------------------------------------------------

bool FFPMCharacterCreationValidator::ValidateName(const FString &Name,
                                                  FString &OutError) {
  // --- Length check ---
  if (Name.Len() < MinNameLength) {
    OutError = FString::Printf(
        TEXT("Character name must be at least %d characters."), MinNameLength);
    return false;
  }

  if (Name.Len() > MaxNameLength) {
    OutError = FString::Printf(
        TEXT("Character name must be at most %d characters."), MaxNameLength);
    return false;
  }

  // --- No leading or trailing spaces ---
  if (Name.StartsWith(TEXT(" ")) || Name.EndsWith(TEXT(" "))) {
    OutError = TEXT("Character name cannot start or end with a space.");
    return false;
  }

  // --- Character whitelist and consecutive space check ---
  bool bPreviousWasSpace = false;
  for (int32 i = 0; i < Name.Len(); ++i) {
    const TCHAR Char = Name[i];

    const bool bIsAlphaNum = FChar::IsAlnum(Char);
    const bool bIsSpace = (Char == TEXT(' '));
    const bool bIsHyphen = (Char == TEXT('-'));
    const bool bIsApostrophe = (Char == TEXT('\''));

    if (!bIsAlphaNum && !bIsSpace && !bIsHyphen && !bIsApostrophe) {
      OutError = TEXT("Character name may only contain letters, numbers, "
                      "spaces, hyphens, and apostrophes.");
      return false;
    }

    // No consecutive spaces
    if (bIsSpace && bPreviousWasSpace) {
      OutError = TEXT("Character name cannot contain consecutive spaces.");
      return false;
    }

    bPreviousWasSpace = bIsSpace;
  }

  // --- Profanity filter ---
  if (ContainsProfanity(Name)) {
    OutError = TEXT("Character name contains inappropriate language.");
    return false;
  }

  return true;
}

// -------------------------------------------------------------------
// ValidateAppearance
// -------------------------------------------------------------------

bool FFPMCharacterCreationValidator::ValidateAppearance(
    const FFPMCharacterCreationRequest &Request, FString &OutError) {
  // --- Body type bounds ---
  if (Request.BodyType > MaxBodyTypeIndex) {
    OutError = FString::Printf(TEXT("Invalid body type (must be 0-%d)."),
                               MaxBodyTypeIndex);
    return false;
  }

  // --- Hair style bounds ---
  if (Request.HairStyle > MaxHairStyleIndex) {
    OutError = FString::Printf(TEXT("Invalid hair style (must be 0-%d)."),
                               MaxHairStyleIndex);
    return false;
  }

  // --- Skin tone color range ---
  if (!ValidateColorRange(Request.SkinTone, TEXT("Skin tone"), OutError)) {
    return false;
  }

  // --- Hair color range ---
  if (!ValidateColorRange(Request.HairColor, TEXT("Hair color"), OutError)) {
    return false;
  }

  return true;
}

// -------------------------------------------------------------------
// Private Helpers
// -------------------------------------------------------------------

bool FFPMCharacterCreationValidator::ContainsProfanity(const FString &Name) {
  // Case-insensitive substring check against blocklist
  const FString LowerName = Name.ToLower();

  for (const FString &BlockedWord : ProfanityBlocklist) {
    if (LowerName.Contains(BlockedWord)) {
      UE_LOG(LogFPMCharacterValidator, Warning,
             TEXT("FPM Validator: Name '%s' blocked — matched '%s'"), *Name,
             *BlockedWord);
      return true;
    }
  }

  return false;
}

bool FFPMCharacterCreationValidator::ValidateColorRange(
    const FLinearColor &Color, const FString &FieldName, FString &OutError) {
  static constexpr float MinColorValue = 0.0f;
  static constexpr float MaxColorValue = 1.0f;

  if (Color.R < MinColorValue || Color.R > MaxColorValue ||
      Color.G < MinColorValue || Color.G > MaxColorValue ||
      Color.B < MinColorValue || Color.B > MaxColorValue) {
    OutError =
        FString::Printf(TEXT("%s color values must be between %.1f and %.1f."),
                        *FieldName, MinColorValue, MaxColorValue);
    return false;
  }

  return true;
}

// -------------------------------------------------------------------
// ValidatePlaystyleAffinities
// -------------------------------------------------------------------

bool FFPMCharacterCreationValidator::ValidatePlaystyleAffinities(
    const FFPMCharacterCreationRequest &Request, FString &OutError) {
  const auto &Entries = Request.PlaystyleAffinities;

  // Correct number of entries
  if (Entries.Num() != PlaystyleAffinityCount) {
    OutError = FString::Printf(
        TEXT("Playstyle affinities must have exactly %d entries (got %d)."),
        PlaystyleAffinityCount, Entries.Num());
    return false;
  }

  // Track which affinity types we've seen (detect duplicates + missing)
  const int32 EnumMax = static_cast<int32>(EFPMPlaystyleAffinity::MAX);
  TArray<bool> Seen;
  Seen.SetNumZeroed(EnumMax);

  int32 Total = 0;

  for (const FFPMPlaystyleAffinityEntry &Entry : Entries) {
    const int32 Index = static_cast<int32>(Entry.Affinity);

    // Bounds check the enum value
    if (Index < 0 || Index >= EnumMax) {
      OutError =
          FString::Printf(TEXT("Invalid playstyle affinity type %d."), Index);
      return false;
    }

    // Duplicate check
    if (Seen[Index]) {
      OutError =
          FString::Printf(TEXT("Duplicate playstyle affinity type %d."), Index);
      return false;
    }
    Seen[Index] = true;

    // Range check
    if (Entry.Points < PlaystyleMinPoints ||
        Entry.Points > PlaystyleMaxPoints) {
      OutError = FString::Printf(
          TEXT("Playstyle affinity %d value %d is out of range [%d, %d]."),
          Index, Entry.Points, PlaystyleMinPoints, PlaystyleMaxPoints);
      return false;
    }

    Total += Entry.Points;
  }

  // All types must be present (covered by count + no-duplicates check)

  // Zero-sum invariant
  if (Total != PlaystyleTotalBudget) {
    OutError =
        FString::Printf(TEXT("Playstyle affinity total must be %d (got %d)."),
                        PlaystyleTotalBudget, Total);
    return false;
  }

  return true;
}

// -------------------------------------------------------------------
// ValidateMagicalAffinities
// -------------------------------------------------------------------

bool FFPMCharacterCreationValidator::ValidateMagicalAffinities(
    const FFPMCharacterCreationRequest &Request, FString &OutError) {
  const auto &Entries = Request.MagicalAffinities;

  // Correct number of entries
  if (Entries.Num() != MagicalAffinityCount) {
    OutError = FString::Printf(
        TEXT("Magical affinities must have exactly %d entries (got %d)."),
        MagicalAffinityCount, Entries.Num());
    return false;
  }

  // Track which affinity types we've seen
  const int32 EnumMax = static_cast<int32>(EFPMMagicalAffinity::MAX);
  TArray<bool> Seen;
  Seen.SetNumZeroed(EnumMax);

  int32 Total = 0;

  for (const FFPMMagicalAffinityEntry &Entry : Entries) {
    const int32 Index = static_cast<int32>(Entry.Affinity);

    // Bounds check
    if (Index < 0 || Index >= EnumMax) {
      OutError =
          FString::Printf(TEXT("Invalid magical affinity type %d."), Index);
      return false;
    }

    // Duplicate check
    if (Seen[Index]) {
      OutError =
          FString::Printf(TEXT("Duplicate magical affinity type %d."), Index);
      return false;
    }
    Seen[Index] = true;

    // Range check
    if (Entry.Points < MagicalMinPoints || Entry.Points > MagicalMaxPoints) {
      OutError = FString::Printf(
          TEXT("Magical affinity %d value %d is out of range [%d, %d]."), Index,
          Entry.Points, MagicalMinPoints, MagicalMaxPoints);
      return false;
    }

    Total += Entry.Points;
  }

  // Zero-sum invariant
  if (Total != MagicalTotalBudget) {
    OutError =
        FString::Printf(TEXT("Magical affinity total must be %d (got %d)."),
                        MagicalTotalBudget, Total);
    return false;
  }

  return true;
}
