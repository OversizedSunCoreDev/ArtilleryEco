// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryShell.h"
#include "NoGuaranteeReadOnly.h"

//See Desperate-thor.gif for more information.
//Catch you in the next one, choom.

//This facade offers no guarantee that it is read only. it exists to break a circular dependency in an elegant
//way that also serves to cleanly facade away some quite considerabile complexity.
//it's probably read only. probably.

//See Desperate-thor.gif for more information or FArtilleryNoGuaranteeReadOnly
template class FNoGuaranteeReadOnly<FArtilleryShell>;
using FArtilleryNoGuaranteeReadOnly = FNoGuaranteeReadOnly<FArtilleryShell>;
typedef TSharedPtr<FArtilleryNoGuaranteeReadOnly> FANG_PTR;