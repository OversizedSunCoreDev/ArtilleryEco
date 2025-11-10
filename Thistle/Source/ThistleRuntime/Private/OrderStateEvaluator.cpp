#include "OrderStateEvaluator.h"

bool FOrderStateEvaluator::Link(FStateTreeLinker& Linker)
{
	return FThistleMSEvaluator::Link(Linker);
}

void FOrderStateEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FThistleMSEvaluator::Tick(Context, DeltaTime);
}
