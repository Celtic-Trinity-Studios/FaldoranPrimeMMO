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
