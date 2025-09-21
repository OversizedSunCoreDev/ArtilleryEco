// Copyright Epic Games, Inc. All Rights Reserved.

#include "BarrageEditorModule.h"

#define LOCTEXT_NAMESPACE "FBarrageEditorModule"

struct FBarrageEditorModule : public IBarrageEditorModule
{
    FBarrageEditorModule()
    {

    }

    virtual void StartupModule() override
    {

    }

    virtual void ShutdownModule() override
    {

    }
};

IMPLEMENT_MODULE(FBarrageEditorModule, IBarrageEditorModule)

#undef LOCTEXT_NAMESPACE
