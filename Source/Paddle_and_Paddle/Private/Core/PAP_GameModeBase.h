// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "PAP_GameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class APAP_GameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
	APAP_GameModeBase();
	void BeginPlay();
};
