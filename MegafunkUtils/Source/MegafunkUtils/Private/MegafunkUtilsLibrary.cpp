

#include "MegafunkUtilsLibrary.h"

#include "Misc/DefinePrivateMemberPtr.h"
// I would noramlly use my own macros but for some reason 
UE_DEFINE_PRIVATE_MEMBER_PTR(void(), GPrivateMacroSendRenderTransform_Concurrent, USceneComponent, SendRenderTransform_Concurrent);

#include "BMPrivateAccess/BMPrivateAccess.h"
DEFINE_PRIVATE_FUNCTION_ACCESSOR(USceneComponent, SendRenderTransform_Concurrent, void)

namespace MegafunkUtils {
	
	void SetWorldTransformFastPathParallelStep(USceneComponent* InComp, const FTransform& InTransform)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MegafunkUtils::SetWorldTransformFastPathParallelStep);
		
		
		// directly set transform and update bounds 
		InComp->SetComponentToWorld(InTransform);

		// InComp->UpdateBounds() is already done in PushPrimitiveRenderData, yay
		
		for (USceneComponent* AttachedComponent : InComp->GetAttachChildren())
		{
			// This * transforms from local space to world space!
			FTransform CompWorldTransform = AttachedComponent->GetRelativeTransform() * InTransform;
	#if 0
			//These are to support non-relative transforms (could probably omit as this is rare?)
			// if (UNLIKELY(InComp->IsUsingAbsoluteLocation()))
			// {
			// 	CompWorldTransform.CopyTranslation(InTransform);
			// }
			//
			// if (UNLIKELY(InComp->IsUsingAbsoluteRotation()))
			// {
			// 	CompWorldTransform.CopyRotation(InTransform);
			// }
			//
			// if (UNLIKELY(InComp->IsUsingAbsoluteScale()))
			// {
			// 	CompWorldTransform.CopyScale3D(InTransform);
			// }
	#endif
			// Recursive!
			SetWorldTransformFastPathParallelStep(AttachedComponent, CompWorldTransform);
		}
	};

	// Dirty the render transform directly with NO main thread step. This assumes render state is created so you had better be doing that
	 void PushPrimitiveRenderData(USceneComponent& InComp, const FTransform& InTransform)
	{
	 	(InComp.*GPrivateMacroSendRenderTransform_Concurrent)();
		for (USceneComponent* AttachChild : InComp.GetAttachChildren())
		{
			(AttachChild->*GPrivateMacroSendRenderTransform_Concurrent)();
		}
	}

	 bool IsFunctionImplementedByBP(FName FunctionName, const UClass& Class) {
		 UFunction* Function = Class.FindFunctionByName(FunctionName);
		 if (!ensureMsgf(Function, TEXT("IsFunctionImplementedByBP could not find function %s:%s"), *Class.GetName(), *FunctionName.ToString())) {
			 return false;
		 }
#if !UE_BUILD_SHIPPING
		 if (!Function->HasAllFunctionFlags(FUNC_BlueprintEvent)) {
			 ensureMsgf(false, TEXT("IsFunctionImplementedByBP called on a function that isn't a blueprint event!"));
		 }
#endif

		 if (!ensure(Function->GetOuter())) {
			 return false;
		 }

		 // Functions with a blueprint generated class as their outer are implemented in BP
		 if (Function->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass())) {
			 return true;
		 }
	 	
	 	return false;
	 };
}