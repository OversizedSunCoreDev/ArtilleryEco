#include "PinSupport.h"

void SInventoryLoadedSoundNames::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	if (auto* EdWorld = InGraphPinObj->GetOuter()->GetWorld())
	{
		if (auto* InventoryDispatch = EdWorld->GetSubsystem<UInventoryDispatch>())
		{
			for (auto& Fme : InventoryDispatch->SoundNamesForPulldown)
			{
				//this step isn't optional, and I actually do not know why. It has to be 
				TSharedPtr<FName> RowNameItem = MakeShareable(new FName(Fme));
				NameList.Add(RowNameItem);
			}
		}
		else
		{
			NameList.Add( MakeShareable(new FName(NAME_None)));
		}
	}
	SGraphPinNameList::Construct(SGraphPinNameList::FArguments(), InGraphPinObj, NameList);
}
