// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "FPMCharacterPreviewActor.generated.h"

class UPointLightComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class USkyLightComponent;
class USpringArmComponent;
class UAnimationAsset;
class UTextureRenderTarget2D;

/**
 * AFPMCharacterPreviewActor
 *
 * Client-only actor that renders a 3D CC5 character preview during
 * character creation. Spawned by UFPMCharacterCreationWidget,
 * destroyed when the creation screen closes.
 *
 * NEVER replicated — this is purely a cosmetic client-side preview.
 * The server has zero knowledge of this actor.
 *
 * Features:
 *   - CC5 skeletal mesh with morph targets (240 facial blendshapes)
 *   - Idle animation via CC5 Animation Blueprint
 *   - SetMorphValue(FName, float) for any morph target
 *   - UMaterialInstanceDynamic per slot (skin, eye, hair)
 *   - SetSkinTone(), SetEyeColor(), SetHairColor() for materials
 *   - SetHairStyle(int32) for hair mesh swaps
 *   - Three-point lighting + ambient sky light
 *   - Scene capture for rendering to a UMG Image widget
 *   - Camera orbit (drag), zoom (scroll), reset (double-click)
 *
 * See Pillar_04_CC5_Character_Creation.md §4 — Phase 4B.
 */
UCLASS(NotPlaceable)
class FALDORANPRIMEMMO_API AFPMCharacterPreviewActor : public AActor {
  GENERATED_BODY()

public:
  AFPMCharacterPreviewActor();

  // --- Morph Target Control ---

  /**
   * Set any morph target on the preview mesh by name.
   * Value is clamped to [0.0, 1.0]. Works with all 240 CC5 morphs.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetMorphValue(FName MorphName, float Value);

  // --- Material Color Setters ---

  /** Set the skin tone on the skin dynamic material instance. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetSkinTone(const FLinearColor &NewColor);

  /** Set the eye (iris) color on the eye dynamic material instance. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetEyeColor(const FLinearColor &NewColor);

  /** Set the hair color on the hair dynamic material instance. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetHairColor(const FLinearColor &NewColor);

  // --- Hair Style ---

  /**
   * Set the hair style by index. Swaps the hair skeletal mesh
   * attachment from the HairMeshes array.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetHairStyle(int32 HairIndex);

  // --- Gender Selection ---

  /** Switch between male (false) and female (true) preview mesh. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void SetIsFemale(bool bFemale);

  /** Get current gender selection. */
  bool GetIsFemale() const { return bIsFemale; }

  // --- Camera Orbit Controls ---

  /**
   * Rotate the camera boom around the vertical axis.
   * Called from mouse drag input. Positive = clockwise from above.
   */
  void AddYawRotation(float DeltaYaw);

  /**
   * Zoom the camera in/out by adjusting the spring arm length.
   * Called from mouse wheel input. Positive = zoom out.
   */
  void AddZoom(float DeltaZoom);

  /** Reset the camera to the default front-facing view and zoom. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Preview")
  void ResetCameraToFront();

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

  /** CC5 skeletal mesh for the character body preview. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview")
  TObjectPtr<USkeletalMeshComponent> PreviewMesh;

  /** Separate skeletal mesh for interchangeable hair styles. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview")
  TObjectPtr<USkeletalMeshComponent> HairMeshComp;

  // --- Lighting ---

  /** Primary key light — front-right, warm. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Lighting")
  TObjectPtr<UPointLightComponent> KeyLight;

  /** Fill light — front-left, cool, softer. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Lighting")
  TObjectPtr<UPointLightComponent> FillLight;

  /** Rim/back light — behind, for edge definition. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Lighting")
  TObjectPtr<UPointLightComponent> RimLight;

  /** Ambient sky light for soft global illumination fill. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Lighting")
  TObjectPtr<USkyLightComponent> AmbientLight;

  // --- Scene Capture ---

  /** Spring arm for camera distance control (zoom). */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Camera")
  TObjectPtr<USpringArmComponent> CameraBoom;

  /** Scene capture that renders the preview to a render target. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Preview|Camera")
  TObjectPtr<USceneCaptureComponent2D> SceneCapture;

  // --- Dynamic Materials (one per CC5 material slot) ---

  /** Skin material instance (slot 0 on CC5 body mesh). */
  UPROPERTY()
  TObjectPtr<UMaterialInstanceDynamic> SkinMID;

  /** Eye material instance (identified by slot name). */
  UPROPERTY()
  TObjectPtr<UMaterialInstanceDynamic> EyeMID;

  /** Hair material instance (slot 0 on hair mesh). */
  UPROPERTY()
  TObjectPtr<UMaterialInstanceDynamic> HairMID;

  // --- Internal Helpers ---

  /** Create per-slot dynamic material instances from the CC5 mesh. */
  void CreateDynamicMaterials();

  /**
   * Find a material slot by searching slot names for a keyword.
   * Returns INDEX_NONE if no match found.
   */
  int32 FindMaterialSlotByKeyword(const FString &Keyword) const;

  // --- Hair Mesh Data ---

  /**
   * Array of hair skeletal meshes loaded at construction.
   * Index 0 = bald (nullptr), subsequent indices are hair styles.
   */
  UPROPERTY()
  TArray<TObjectPtr<USkeletalMesh>> HairMeshes;

  /** Current active hair style index. */
  int32 CurrentHairIndex = 0;

  /** Current gender: false = male, true = female. */
  bool bIsFemale = false;

  /** Cached skeletal meshes for gender swap. */
  UPROPERTY()
  TObjectPtr<USkeletalMesh> MaleMesh;
  UPROPERTY()
  TObjectPtr<USkeletalMesh> FemaleMesh;

  /** Idle animation played in a loop on the preview mesh. */
  UPROPERTY()
  TObjectPtr<UAnimationAsset> IdleAnimation;

  /** Apply current body mesh, restart animation, rebuild materials. */
  void ApplyCurrentBody();

  // --- Material Parameter Names ---

  static const FName SkinColorParamName;
  static const FName IrisColorParamName;
  static const FName HairColorParamName;

  // --- Camera Constants ---

  static constexpr float MinZoomDistance = 80.0f;
  static constexpr float MaxZoomDistance = 400.0f;
  static constexpr float DefaultZoomDistance = 150.0f;
  static constexpr float ZoomSpeed = 20.0f;
  static constexpr float DefaultCameraPitch = -10.0f;
};
