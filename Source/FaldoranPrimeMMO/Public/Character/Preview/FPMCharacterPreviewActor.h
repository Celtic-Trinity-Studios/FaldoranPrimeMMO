// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "FPMCharacterPreviewActor.generated.h"

class UPointLightComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class USpringArmComponent;
class UTextureRenderTarget2D;

/**
 * AFPMCharacterPreviewActor
 *
 * Client-only actor that renders a 3D mannequin preview during character
 * creation. Spawned by UFPMCharacterCreationWidget, destroyed when the
 * creation screen closes.
 *
 * NEVER replicated — this is purely a cosmetic client-side preview.
 * The server has zero knowledge of this actor.
 *
 * Features:
 *   - UE5 mannequin skeletal mesh (placeholder for Mutable/CC5)
 *   - UMaterialInstanceDynamic for real-time skin tone and hair color
 *   - Key + fill lighting for flattering preview rendering
 *   - Scene capture for rendering to a UMG Image widget
 *   - Camera orbit (mouse drag) and zoom (mouse wheel)
 *
 * See Character_Creation_System.md §4 — Client-Only (Preview) domain.
 */
UCLASS(NotPlaceable)
class FALDORANPRIMEMMO_API AFPMCharacterPreviewActor : public AActor {
  GENERATED_BODY()

public:
  AFPMCharacterPreviewActor();

  // --- Appearance Setters (called by widget when sliders change) ---

  /** Set the skin tone on the dynamic material instance. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetSkinTone(const FLinearColor &NewColor);

  /** Set the hair style index (mesh swap placeholder). */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetHairStyle(uint8 StyleIndex);

  /** Set the hair color on the dynamic material instance. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetHairColor(const FLinearColor &NewColor);

  /** Set the body type (morph target or mesh swap placeholder). */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetBodyType(uint8 TypeIndex);

  // --- Camera Orbit Controls ---

  /**
   * Rotate the preview actor around the vertical axis.
   * Called from mouse drag input. Positive = clockwise when viewed from above.
   */
  void AddYawRotation(float DeltaYaw);

  /**
   * Zoom the camera in/out by adjusting the spring arm length.
   * Called from mouse wheel input. Positive = zoom out.
   */
  void AddZoom(float DeltaZoom);

  /** Get the render target that the scene capture writes to. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  UTextureRenderTarget2D *GetRenderTarget() const;

protected:
  virtual void BeginPlay() override;

private:
  // --- Visual Components ---

  /** Root pivot point — rotated for camera orbit. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview")
  TObjectPtr<USceneComponent> PreviewRoot;

  /** Mannequin skeletal mesh for the character preview. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview")
  TObjectPtr<USkeletalMeshComponent> PreviewMesh;

  // --- Lighting ---

  /** Primary key light — positioned front-right for main illumination. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Lighting")
  TObjectPtr<UPointLightComponent> KeyLight;

  /** Fill light — positioned front-left for softer shadow fill. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Lighting")
  TObjectPtr<UPointLightComponent> FillLight;

  /** Rim/back light — positioned behind for edge definition. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Lighting")
  TObjectPtr<UPointLightComponent> RimLight;

  // --- Scene Capture ---

  /** Spring arm for camera distance control (zoom). */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Camera")
  TObjectPtr<USpringArmComponent> CameraBoom;

  /** Scene capture that renders the preview to a render target. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Camera")
  TObjectPtr<USceneCaptureComponent2D> SceneCapture;

  // --- Material ---

  /** Dynamic material instance created at runtime for color changes. */
  UPROPERTY()
  TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

  /** Create the dynamic material instance from the mesh's base material. */
  void CreateDynamicMaterial();

  // --- Constants ---

  /** Material parameter name for the skin tone color. */
  static const FName SkinToneParamName;

  /** Material parameter name for the hair color. */
  static const FName HairColorParamName;

  /** Minimum spring arm length (closest zoom). */
  static constexpr float MinZoomDistance = 80.0f;

  /** Maximum spring arm length (farthest zoom). */
  static constexpr float MaxZoomDistance = 400.0f;

  /** Default spring arm length. */
  static constexpr float DefaultZoomDistance = 150.0f;

  /** Zoom speed multiplier per mouse wheel tick. */
  static constexpr float ZoomSpeed = 20.0f;

  /** Current active body type index. */
  uint8 CurrentBodyType = 0;

  /** Current active hair style index. */
  uint8 CurrentHairStyle = 0;
};
