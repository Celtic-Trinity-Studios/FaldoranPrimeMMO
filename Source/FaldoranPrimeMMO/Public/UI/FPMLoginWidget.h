// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "FPMLoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UCanvasPanel;
class UImage;
class UVerticalBox;
class USizeBox;
class UBorder;
class UOverlay;
class USpacer;
class UTexture2D;

/**
 * UFPMLoginWidget
 *
 * Self-contained login screen widget for Faldoran Prime.
 *
 * Renders a full-screen opaque background (solid colour by default;
 * assign BackgroundTexture to display a static or animated image)
 * over which the "Glass & Gold" login panel sits.
 *
 * Future: set BackgroundTexture to a Media Player / animated texture
 * to display a cinematic or looping splash video — no code change
 * required, just assign the asset.
 *
 * NOTE: WBP_LoginScreen Blueprint root must be a single empty Canvas
 * Panel. All children are built in C++ via NativeConstruct → BuildUI().
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMLoginWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /**
   * Optional background image/texture for the login screen.
   *
   * Leave null  → solid deep-navy background (current default).
   * Assign here → the texture fills the full screen behind the panel.
   *
   * Supports any UTexture2D, including media textures (animated/video)
   * and render targets. Set this before the widget is constructed
   * (e.g., in the PlayerController before calling ShowLoginWidget).
   *
   * EDITOR: Set it on the WBP_LoginScreen Blueprint class defaults,
   * or assign it programmatically from AFPMPlayerController.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Visual",
            meta = (DisplayName = "Background Texture"))
  TObjectPtr<UTexture2D> BackgroundTexture;

  /** Update the result text shown to the user (e.g., error or success). */
  UFUNCTION(BlueprintCallable, Category = "FPM|UI")
  void SetResultMessage(const FString &Message, bool bIsError);

protected:
  virtual void NativeConstruct() override;
  virtual void NativeTick(const FGeometry &MyGeometry,
                          float InDeltaTime) override;

private:
  // --- Programmatically created widgets ---

  /**
   * Full-screen background image. Opaque by default (solid dark colour).
   * If BackgroundTexture is assigned, it is applied to this image's brush
   * so the texture fills the screen behind the login panel.
   */
  UPROPERTY()
  TObjectPtr<UImage> BackgroundImage;

  /**
   * Thin gold shimmer line that sweeps across the panel every 4 seconds.
   */
  UPROPERTY()
  TObjectPtr<UImage> ShimmerLine;

  /** Title text ("FALDORAN PRIME"). */
  UPROPERTY()
  TObjectPtr<UTextBlock> TitleText;

  /** Subtitle / tagline text. */
  UPROPERTY()
  TObjectPtr<UTextBlock> SubtitleText;

  /** Server status indicator text. */
  UPROPERTY()
  TObjectPtr<UTextBlock> StatusText;

  UPROPERTY()
  TObjectPtr<UEditableTextBox> UsernameInput;

  UPROPERTY()
  TObjectPtr<UEditableTextBox> PasswordInput;

  UPROPERTY()
  TObjectPtr<UButton> LoginButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> LoginButtonText;

  UPROPERTY()
  TObjectPtr<UButton> CreateAccountButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> CreateAccountButtonText;

  UPROPERTY()
  TObjectPtr<UTextBlock> ResultText;

  UPROPERTY()
  TObjectPtr<UTextBlock> VersionText;

  UPROPERTY()
  TObjectPtr<UTextBlock> CopyrightText;

  // --- Animation state ---

  /** Accumulated time for breathing glow on title. */
  float AnimTime = 0.0f;

  /**
   * Shimmer position [0, 1] as fraction across the panel width.
   * Loops every ShimmerPeriodSeconds.
   */
  float ShimmerPhase = 0.0f;

  /** Seconds for one full shimmer sweep. */
  static constexpr float ShimmerPeriodSeconds = 4.0f;

  /** Panel width in slate units (updated in NativeTick). */
  float CachedPanelWidth = 440.0f;

  /** Canvas slot for the shimmer line (to reposition it). */
  UPROPERTY()
  TObjectPtr<class UCanvasPanelSlot> ShimmerSlot;

  // --- Helpers ---

  /** Build the entire UI tree and add it to the root canvas. */
  void BuildUI();

  /** Called when the Login button is clicked. */
  UFUNCTION()
  void OnLoginClicked();

  /** Called when the Create Account button is clicked. */
  UFUNCTION()
  void OnCreateAccountClicked();
};
