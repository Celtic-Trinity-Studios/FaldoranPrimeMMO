// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPMAccountTypes.generated.h"

/**
 * FFPMLoginRequest
 *
 * Data contract for an account login or creation request.
 * Sent from the client to the server via RPC (Phase 4B).
 * The server NEVER trusts these values — all fields are validated server-side.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMLoginRequest {
  GENERATED_BODY()

  /** Username submitted by the client. Validated server-side. */
  UPROPERTY(BlueprintReadWrite)
  FString Username;

  /** Password submitted by the client. Hashed server-side — never stored in
   * plaintext. */
  UPROPERTY(BlueprintReadWrite)
  FString Password;
};

/**
 * FFPMLoginResult
 *
 * Result returned from account creation or login operations.
 * Sent from the server back to the client via Client RPC (Phase 4B).
 * The AccountId is only valid on the server — clients use it as an opaque
 * token.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMLoginResult {
  GENERATED_BODY()

  /** Whether the operation succeeded. */
  UPROPERTY(BlueprintReadOnly)
  bool bSuccess = false;

  /** The account UUID on success. Invalid (zeroed) on failure. */
  UPROPERTY(BlueprintReadOnly)
  FGuid AccountId;

  /** Human-readable error message on failure, empty on success. */
  UPROPERTY(BlueprintReadOnly)
  FString ErrorMessage;
};
