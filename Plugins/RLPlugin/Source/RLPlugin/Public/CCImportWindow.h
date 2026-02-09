#pragma once


//#include "SlateBasics.h"

class UCCImportUI;
class SCCImportWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCCImportWindow)
		: _ImportUI(NULL)
		, _WidgetWindow()
	{}

	SLATE_ARGUMENT(UCCImportUI*, ImportUI)
	SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
private:
	UCCImportUI*	ccImportUI;
	TWeakPtr< SWindow > WidgetWindow;
	TSharedPtr< SButton > ImportButton;
	void OnCCAutoSetupChanged(ECheckBoxState InCheckedState);
	TSharedPtr< SCheckBox > CCAutoSetup_CheckBox;
	TSharedPtr< SCheckBox > CCControlRig_CheckBox;
	FReply OnCancel();
	FReply OnNext();
    FReply OnMore();
	void OnBrowserLinkClicked( const TMap< FString, FString >& Metadata );
};