// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMLoginWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Player/FPMPlayerController.h"


DEFINE_LOG_CATEGORY_STATIC(LogFPMLoginWidget, Log, All);

void UFPMLoginWidget::NativeConstruct() {
  Super::NativeConstruct();

  if (LoginButton) {
    LoginButton->OnClicked.AddDynamic(this, &UFPMLoginWidget::OnLoginClicked);
  }

  if (CreateAccountButton) {
    CreateAccountButton->OnClicked.AddDynamic(
        this, &UFPMLoginWidget::OnCreateAccountClicked);
  }

  // Clear any placeholder text
  SetResultMessage(TEXT(""), false);

  UE_LOG(LogFPMLoginWidget, Log, TEXT("FPM Login: Widget constructed."));
}

void UFPMLoginWidget::SetResultMessage(const FString &Message, bool bIsError) {
  if (!ResultText) {
    return;
  }

  ResultText->SetText(FText::FromString(Message));

  // Red for errors, white for success/info
  const FSlateColor Color = bIsError ? FSlateColor(FLinearColor::Red)
                                     : FSlateColor(FLinearColor::White);
  ResultText->SetColorAndOpacity(Color);
}

void UFPMLoginWidget::OnLoginClicked() {
  if (!UsernameInput || !PasswordInput) {
    return;
  }

  const FString Username = UsernameInput->GetText().ToString();
  const FString Password = PasswordInput->GetText().ToString();

  if (Username.IsEmpty() || Password.IsEmpty()) {
    SetResultMessage(TEXT("Please enter username and password."), true);
    return;
  }

  // Forward to the PlayerController which sends the Server RPC
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestLogin(Username, Password);
    SetResultMessage(TEXT("Logging in..."), false);
  }
}

void UFPMLoginWidget::OnCreateAccountClicked() {
  if (!UsernameInput || !PasswordInput) {
    return;
  }

  const FString Username = UsernameInput->GetText().ToString();
  const FString Password = PasswordInput->GetText().ToString();

  if (Username.IsEmpty() || Password.IsEmpty()) {
    SetResultMessage(TEXT("Please enter username and password."), true);
    return;
  }

  // Forward to the PlayerController which sends the Server RPC
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestCreateAccount(Username, Password);
    SetResultMessage(TEXT("Creating account..."), false);
  }
}
