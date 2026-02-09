#pragma once
#include "IDetailCustomization.h"

struct FGroomBindingAttributeSelection
{
	int32 SelectedBindingAttribute = INDEX_NONE;
	TArray<FName> BindingAttributeNames;
};


class CCCreateGroomBindingUICustomization : public IDetailCustomization
{
public:
	/** Begin IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& LayoutBuilder ) override;
	/** End IDetailCustomization interface */
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<CCCreateGroomBindingUICustomization>(); }
	FGroomBindingAttributeSelection BindingAttributeSelection;
};