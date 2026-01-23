// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PAP_PawnBoat.generated.h"

UCLASS()
class APAP_PawnBoat : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	APAP_PawnBoat();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
