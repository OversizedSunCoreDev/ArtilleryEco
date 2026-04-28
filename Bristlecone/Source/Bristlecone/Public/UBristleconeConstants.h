// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "UBristleconeConstants.generated.h"
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Bristlecone Settings"))
class BRISTLECONE_API UBristleconeConstants : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "General", meta= (DisplayName = "Reflector IP"))
	FString default_address_c;

	UPROPERTY(EditAnywhere, Config, Category = "Bristlecone")
	bool log_receive_c;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bristlecone");
	int32 ClientToServerSchemaMapperPort;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bristlecone");
	int32 ClientToServerSchemaPort;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bristlecone");
	int32 ServerToClientMapperPort;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bristlecone");
	FString BackhaulAddress;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bristlecone");
	int32 BackhaulPort;
};

