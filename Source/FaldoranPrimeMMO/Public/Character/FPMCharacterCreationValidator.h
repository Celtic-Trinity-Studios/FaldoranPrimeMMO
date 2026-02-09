// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Character/FPMCharacterCreationDataContract.h"
#include "CoreMinimal.h"


/**
 * FFPMCharacterCreationValidator
 *
 * Pure validation logic for character creation requests.
 * This is a plain C++ class (not a UObject) to keep validation
 * separate from subsystem lifecycle and database concerns.
 *
 * All methods are static — no instance state needed.
 * Called server-side only by UFPMCharacterCreationSubsystem.
 */
class FALDORANPRIMEMMO_API FFPMCharacterCreationValidator {
public:
  // --- Named Constants (no magic numbers) ---

  /** Minimum allowed character name length. */
  static constexpr int32 MinNameLength = 3;

  /** Maximum allowed character name length. */
  static constexpr int32 MaxNameLength = 20;

  /** Maximum number of body type options. */
  static constexpr uint8 MaxBodyTypeIndex = 3;

  /** Maximum number of hair style options. */
  static constexpr uint8 MaxHairStyleIndex = 7;

  /** Maximum characters per account. */
  static constexpr int32 MaxCharactersPerAccount = 5;

  // --- Validation Methods ---

  /**
   * Validate the entire character creation request.
   * Calls ValidateName() and ValidateAppearance() in sequence.
   * Returns the FIRST error found (fail closed — no partial validation).
   *
   * @param Request       The untrusted client request to validate.
   * @param OutErrorCode  Set to the specific error code on failure.
   * @param OutError      Set to a human-readable error description on failure.
   * @return              true if the request passes all validation.
   */
  static bool ValidateRequest(const FFPMCharacterCreationRequest &Request,
                              EFPMCharacterCreationError &OutErrorCode,
                              FString &OutError);

  /**
   * Validate the character name.
   * Rules:
   *   - Length: MinNameLength to MaxNameLength characters
   *   - Allowed chars: A-Z, a-z, 0-9, space, hyphen, apostrophe
   *   - No leading or trailing spaces
   *   - No consecutive spaces
   *   - Basic profanity filter
   *
   * @param Name      The character name to validate.
   * @param OutError  Set to the error description on failure.
   * @return          true if the name is valid.
   */
  static bool ValidateName(const FString &Name, FString &OutError);

  /**
   * Validate appearance values (body type, skin tone, hair style, hair color).
   * Rules:
   *   - BodyType: 0 to MaxBodyTypeIndex
   *   - HairStyle: 0 to MaxHairStyleIndex
   *   - SkinTone RGB: each component in [0.0, 1.0]
   *   - HairColor RGB: each component in [0.0, 1.0]
   *
   * @param Request   The request containing appearance values.
   * @param OutError  Set to the error description on failure.
   * @return          true if all appearance values are valid.
   */
  static bool ValidateAppearance(const FFPMCharacterCreationRequest &Request,
                                 FString &OutError);

private:
  /**
   * Check if a character name contains profanity.
   * Uses a small server-side blocklist — expandable for production.
   *
   * @param Name  The name to check (case-insensitive).
   * @return      true if the name contains blocked words.
   */
  static bool ContainsProfanity(const FString &Name);

  /**
   * Validate that all RGB components of a color are in [0.0, 1.0].
   *
   * @param Color      The color to validate.
   * @param FieldName  Name of the field for error messaging.
   * @param OutError   Set to error description on failure.
   * @return           true if all components are in range.
   */
  static bool ValidateColorRange(const FLinearColor &Color,
                                 const FString &FieldName, FString &OutError);
};
