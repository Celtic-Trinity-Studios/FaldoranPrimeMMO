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
  // --- Named Constants: Name / Appearance ---

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

  // --- Named Constants: Playstyle Affinities (§4.2) ---

  /** Number of playstyle affinity categories. */
  static constexpr int32 PlaystyleAffinityCount = 6;

  /** Default points per playstyle affinity. */
  static constexpr int32 PlaystyleDefaultPoints = 100;

  /** Minimum points allowed per playstyle affinity. */
  static constexpr int32 PlaystyleMinPoints = 90;

  /** Maximum points allowed per playstyle affinity. */
  static constexpr int32 PlaystyleMaxPoints = 150;

  /** Total point budget for the playstyle pool (zero-sum invariant). */
  static constexpr int32 PlaystyleTotalBudget = 600;

  // --- Named Constants: Magical Affinities (§4.3) ---

  /** Number of magical affinity categories. */
  static constexpr int32 MagicalAffinityCount = 8;

  /** Default points per magical affinity. */
  static constexpr int32 MagicalDefaultPoints = 100;

  /** Minimum points allowed per magical affinity. */
  static constexpr int32 MagicalMinPoints = 90;

  /** Maximum points allowed per magical affinity. */
  static constexpr int32 MagicalMaxPoints = 150;

  /** Total point budget for the magical pool (zero-sum invariant). */
  static constexpr int32 MagicalTotalBudget = 800;

  // --- Validation Methods ---

  /**
   * Validate the entire character creation request.
   * Calls ValidateName(), ValidateAppearance(), and both affinity
   * validators in sequence.
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

  /**
   * Validate playstyle affinity distribution.
   * Rules:
   *   - Exactly PlaystyleAffinityCount (6) entries
   *   - All affinity types present (Martial through Survival)
   *   - Each value in [PlaystyleMinPoints, PlaystyleMaxPoints]
   *   - Total = PlaystyleTotalBudget (zero-sum invariant)
   *
   * @param Request   The request containing affinity values.
   * @param OutError  Set to the error description on failure.
   * @return          true if the playstyle affinities are valid.
   */
  static bool
  ValidatePlaystyleAffinities(const FFPMCharacterCreationRequest &Request,
                              FString &OutError);

  /**
   * Validate magical affinity distribution.
   * Rules:
   *   - Exactly MagicalAffinityCount (8) entries
   *   - All affinity types present (Fire through Arcane)
   *   - Each value in [MagicalMinPoints, MagicalMaxPoints]
   *   - Total = MagicalTotalBudget (zero-sum invariant)
   *
   * @param Request   The request containing affinity values.
   * @param OutError  Set to the error description on failure.
   * @return          true if the magical affinities are valid.
   */
  static bool
  ValidateMagicalAffinities(const FFPMCharacterCreationRequest &Request,
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
