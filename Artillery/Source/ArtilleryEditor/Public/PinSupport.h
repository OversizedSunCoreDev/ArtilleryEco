#pragma once
#include "EdGraphSchema_K2.h"
#include "InventoryDispatch.h"
#include "SGraphPinNameList.h"

//this needs to be factored out into a small helper set specifically for the pins. It's almost impossible to parse how this works unless you have some priors.
//may even need to pull it up to the editor module, tbh.
class SInventoryLoadedSoundNames : public SGraphPinNameList
{
public:
	SLATE_BEGIN_ARGS(SInventoryLoadedSoundNames)
		{
		}

	SLATE_END_ARGS()

	TArray<TSharedPtr<FName>> CachedRowNames;
	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	SInventoryLoadedSoundNames()
	{
	}


	~SInventoryLoadedSoundNames()
	{
	}

protected:
};

class FArtillerySoundsPinFactory : public FGraphPanelPinFactory
{
public:
	// Overrides the base function to spawn your custom pin widget
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (!InPin)
		{
			return nullptr;
		}


		// Example: Check if the pin is a struct type matching your custom struct name
		if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			UScriptStruct* PinStructType = Cast<UScriptStruct>(InPin->PinType.PinSubCategoryObject.Get());
			if (PinStructType && PinStructType->GetName() == TEXT("FSoundPinBag"))
			{
				// Return your custom Slate widget
				return SNew(SInventoryLoadedSoundNames, InPin);
			}
		}

		// Return nullptr to let other factories handle the pin
		return nullptr;
	};
};
