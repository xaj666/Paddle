// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PAP_GameModeBase.h"

#include "Core/PAP_PlayerController.h"
#include "Character/PAP_Boat.h"
#include "GameFramework/HUD.h"

class APawn;
class AHUD;

APAP_GameModeBase::APAP_GameModeBase() {

    //初始化默认类
    //PlayerControllerClass = APAP_PlayerController::StaticClass();

    static ConstructorHelpers::FClassFinder<APawn> BoatBPClass(TEXT("/Game/PAP/BluePrint/BP_PawnBoat"));
    if (BoatBPClass.Succeeded())
        DefaultPawnClass = BoatBPClass.Class;
    else
        DefaultPawnClass = APAP_Boat::StaticClass();


    static ConstructorHelpers::FClassFinder<AHUD> PAPHUDClass(TEXT("/Game/PAP/UI/PAP_HUD"));
    if (PAPHUDClass.Succeeded())
        HUDClass = PAPHUDClass.Class;

    static ConstructorHelpers::FClassFinder<APlayerController> PCClass(TEXT("/Game/PAP/PC/BP_PC"));
    if (PCClass.Succeeded())
        PlayerControllerClass = PCClass.Class;
}

void APAP_GameModeBase::BeginPlay() {
	Super::BeginPlay();


    
}
