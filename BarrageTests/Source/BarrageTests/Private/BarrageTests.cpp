// Copyright Radaway Software LLC. 2025. All rights reserved.

#include "BarrageTests.h"

#define LOCTEXT_NAMESPACE "BarrageTests"

struct FBarrageTestsModule : public IBarrageTestsModule
{};

IMPLEMENT_MODULE (FBarrageTestsModule, BarrageTests)

#undef LOCTEXT_NAMESPACE