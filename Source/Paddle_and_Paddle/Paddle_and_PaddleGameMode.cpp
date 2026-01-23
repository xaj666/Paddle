// Copyright Epic Games, Inc. All Rights Reserved.

#include "Paddle_and_PaddleGameMode.h"
#include "Paddle_and_PaddleCharacter.h"
#include "UObject/ConstructorHelpers.h"

APaddle_and_PaddleGameMode::APaddle_and_PaddleGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
