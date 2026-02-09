// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "GroomBindingAsset.h"
#include "CCCreateGroomWindow.generated.h"

class SButton;
class UGroomAsset;
class USkeletalMesh;

UCLASS( MinimalAPI, BlueprintType, config = EditorPerProjectUserSettings )
class UCreateCCGroomBindingOptions : public UObject
{
	GENERATED_BODY()

public:
	/** Groom asset */
	UPROPERTY()
	TWeakObjectPtr<UGroomAsset> GroomAsset;

	/** Skeletal mesh on which the groom has been authored. This is optional, and used only if the hair
		binding is done a different mesh than the one which it has been authored, i.e., only if the curves
		roots and the surface geometry don't aligned and need to be wrapped/transformed. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = BuildSettings )
	TObjectPtr<USkeletalMesh> SourceSkeletalMesh;

	/** Skeletal mesh on which the groom is attached to. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = BuildSettings )
	TObjectPtr<USkeletalMesh> TargetSkeletalMesh;

	/** Number of points used for the rbf interpolation */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, meta = ( ShowOnlyInnerProperties, ClampMin = "0", ClampMax = "100" ), Category = HairInterpolation )
	int32 NumInterpolationPoints = 100;

	/** Section to pick to transfer the position */
	UPROPERTY( EditDefaultsOnly, BlueprintReadWrite, meta = ( ShowOnlyInnerProperties ), Category = BuildSettings )
	int32 MatchingSection = 0;

	/** Optional mesh attribute used to filter triangles to bind groom to */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, meta = ( ShowOnlyInnerProperties ), Category = BuildSettings )
	FName TargetBindingAttribute = NAME_None;
};

class SCCCreateGroomWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SCCCreateGroomWindow )
		: _pBindingOptions( nullptr )
		, _pWidgetWindow()
		, _FullPath()
		, _ButtonLabel()
		{
		}

		SLATE_ARGUMENT( UCreateCCGroomBindingOptions*, pBindingOptions )
		SLATE_ARGUMENT( TSharedPtr<SWindow>, pWidgetWindow )
		SLATE_ARGUMENT( FText, FullPath )
		SLATE_ARGUMENT( FText, ButtonLabel )
	SLATE_END_ARGS()

public:
	void Construct( const FArguments& InArgs );
	virtual bool SupportsKeyboardFocus() const override { return true; }

	static TSharedPtr<SCCCreateGroomWindow> DisplayCreateBindingOptions( UCreateCCGroomBindingOptions* pBindingOptions );

	FReply OnCreateBinding()
	{
		bShouldCreate = true;
		if ( pWidgetWindow.IsValid() )
		{
			pWidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bShouldCreate = false;
		if ( pWidgetWindow.IsValid() )
		{
			pWidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		if ( InKeyEvent.GetKey() == EKeys::Escape )
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldCreate() const
	{
		return bShouldCreate;
	}

	SCCCreateGroomWindow()
		: pBindingOptions( nullptr )
		, bShouldCreate( false )
	{
	}

private:

	bool CanCreateBinding() const;
	

private:
	UCreateCCGroomBindingOptions* pBindingOptions;
	TSharedPtr<class IDetailsView> pDetailsView;
	TWeakPtr<SWindow> pWidgetWindow;
	TSharedPtr<SButton> ImportButton;
	bool bShouldCreate;
};
