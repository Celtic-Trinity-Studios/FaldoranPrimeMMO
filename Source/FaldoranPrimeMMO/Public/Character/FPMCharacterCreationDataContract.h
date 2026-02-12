// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPMCharacterCreationDataContract.generated.h"

/**
 * EFPMPlaystyleAffinity
 *
 * The 6 playstyle affinities distributed at character creation.
 * These are permanent XP/effectiveness multipliers — cannot be changed
 * after creation. See 00_Rules_and_Constraints.md §4.2.
 */
UENUM(BlueprintType)
enum class EFPMPlaystyleAffinity : uint8 {
  Martial = 0,
  Ranged,
  Magic,
  Crafting,
  Social,
  Survival,
  MAX UMETA(Hidden) // Sentinel for iteration and bounds checking
};

/**
 * EFPMMagicalAffinity
 *
 * The 8 magical affinities distributed at character creation.
 * Represent bonuses to specific elemental damage types.
 * Separate pool from playstyle affinities. See 00_Rules_and_Constraints.md
 * §4.3.
 */
UENUM(BlueprintType)
enum class EFPMMagicalAffinity : uint8 {
  Fire = 0,
  Water,
  Earth,
  Air,
  Light,
  Shadow,
  Nature,
  Arcane,
  MAX UMETA(Hidden) // Sentinel for iteration and bounds checking
};

/**
 * EFPMCharacterCreationError
 *
 * Comprehensive error codes for character creation validation.
 * Each value maps to a specific class of failure so the client
 * can display an appropriate message without leaking server internals.
 */
UENUM(BlueprintType)
enum class EFPMCharacterCreationError : uint8 {
  /** No error — creation succeeded. */
  None = 0,

  /** Character name failed validation (length, characters, profanity). */
  InvalidName,

  /** Character name is already taken by another character. */
  NameTaken,

  /** Appearance values are out of valid bounds. */
  InvalidAppearance,

  /** Affinity point distribution is invalid (wrong total, out of range). */
  InvalidAffinities,

  /** Account already has the maximum number of characters. */
  TooManyCharacters,

  /** Too many creation requests in a short window. */
  RateLimited,

  /** An unexpected server-side error occurred. */
  ServerError
};

/**
 * FFPMPlaystyleAffinityEntry
 *
 * Single key-value pair for playstyle affinity distribution.
 * TArray of these is used instead of TMap for RPC compatibility
 * (UE5 UHT does not support TMap in replicated structs).
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMPlaystyleAffinityEntry {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadWrite)
  EFPMPlaystyleAffinity Affinity = EFPMPlaystyleAffinity::Martial;

  UPROPERTY(BlueprintReadWrite)
  int32 Points = 0;
};

/**
 * FFPMMagicalAffinityEntry
 *
 * Single key-value pair for magical affinity distribution.
 * TArray of these is used instead of TMap for RPC compatibility.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMMagicalAffinityEntry {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadWrite)
  EFPMMagicalAffinity Affinity = EFPMMagicalAffinity::Fire;

  UPROPERTY(BlueprintReadWrite)
  int32 Points = 0;
};

/**
 * FFPMCharacterCreationRequest
 *
 * Data contract for a character creation request sent from client to server.
 * The server NEVER trusts these values — all fields are validated server-side
 * by UFPMCharacterCreationValidator before any database operations occur.
 *
 * This is the UNTRUSTED client payload. The server is the sole authority.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMCharacterCreationRequest {
  GENERATED_BODY()

  /** Desired character name. Validated for length, characters, profanity. */
  UPROPERTY(BlueprintReadWrite)
  FString CharacterName;

  /** Body type index. Validated against design data bounds (0-3). */
  UPROPERTY(BlueprintReadWrite)
  uint8 BodyType = 0;

  /** Skin tone color. RGB values must be in range [0.0, 1.0]. */
  UPROPERTY(BlueprintReadWrite)
  FLinearColor SkinTone = FLinearColor(0.8f, 0.6f, 0.5f, 1.0f);

  /** Hair style index. Validated against design data bounds. */
  UPROPERTY(BlueprintReadWrite)
  uint8 HairStyle = 0;

  /** Hair color. RGB values must be in range [0.0, 1.0]. */
  UPROPERTY(BlueprintReadWrite)
  FLinearColor HairColor = FLinearColor(0.2f, 0.15f, 0.1f, 1.0f);

  /**
   * Playstyle affinity point distribution. Zero-sum pool: total must equal 600.
   * Each value must be in [90, 150]. All 6 affinities must be present.
   * Uses TArray instead of TMap for UE5 RPC/replication compatibility.
   * See 00_Rules_and_Constraints.md §4.2.
   */
  UPROPERTY(BlueprintReadWrite)
  TArray<FFPMPlaystyleAffinityEntry> PlaystyleAffinities;

  /**
   * Magical affinity point distribution. Zero-sum pool: total must equal 800.
   * Each value must be in [90, 150]. All 8 affinities must be present.
   * Uses TArray instead of TMap for UE5 RPC/replication compatibility.
   * See 00_Rules_and_Constraints.md §4.3.
   */
  UPROPERTY(BlueprintReadWrite)
  TArray<FFPMMagicalAffinityEntry> MagicalAffinities;
};

/**
 * FFPMCharacterCreationResult
 *
 * Result returned from the server after processing a character creation
 * request. Sent back to the client via Client RPC.
 *
 * On success: bSuccess=true, CharacterId is the new character's UUID.
 * On failure: bSuccess=false, ErrorCode identifies the failure type,
 *             ErrorMessage provides a human-readable description.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMCharacterCreationResult {
  GENERATED_BODY()

  /** Whether character creation succeeded. */
  UPROPERTY(BlueprintReadOnly)
  bool bSuccess = false;

  /** The new character's UUID on success. Invalid (zeroed) on failure. */
  UPROPERTY(BlueprintReadOnly)
  FGuid CharacterId;

  /** Specific error code on failure. None on success. */
  UPROPERTY(BlueprintReadOnly)
  EFPMCharacterCreationError ErrorCode = EFPMCharacterCreationError::None;

  /** Human-readable error message on failure. Empty on success. */
  UPROPERTY(BlueprintReadOnly)
  FString ErrorMessage;
};

/**
 * FFPMCharacterSummary
 *
 * Lightweight summary of a character for the character select list.
 * Sent from the server to the client via Client RPC. Contains only
 * the fields needed for display — full appearance data is loaded
 * when the player enters the world.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMCharacterSummary {
  GENERATED_BODY()

  /** The character's unique identifier. */
  UPROPERTY(BlueprintReadOnly)
  FGuid CharacterId;

  /** The character's display name. */
  UPROPERTY(BlueprintReadOnly)
  FString CharacterName;

  /** Body type index for display purposes. */
  UPROPERTY(BlueprintReadOnly)
  uint8 BodyType = 0;

  /** When this character was last played (human-readable string). */
  UPROPERTY(BlueprintReadOnly)
  FString LastPlayed;
};
