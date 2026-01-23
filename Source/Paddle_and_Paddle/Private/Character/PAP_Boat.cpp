// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/PAP_Boat.h"
#include "GameFramework/CharacterMovementComponent.h"

APAP_Boat::APAP_Boat() {
    BoatMesh = GetMesh();
    MoveComp = GetCharacterMovement();


    PrimaryActorTick.bCanEverTick = true;

    //BoatMesh基本参数设置
    if (BoatMesh) {
        BoatMesh->SetSimulatePhysics(true);       // 启用物理模拟
        BoatMesh->SetEnableGravity(true);
    }
    
 
}

void APAP_Boat::BeginPlay() {
    Super::BeginPlay(); 
}


void APAP_Boat::Tick(float DeltaTime) {

    Super::Tick(DeltaTime);

    //ActionInput 转化为 AxisInput ， 并Tick更新输入
    //该部分模拟AxisInput的持续按下，让一次按键能够模拟几秒的持续按下
    if (HowLongHas_LBtn_Pressed > 0.0f) {
        HowLongHas_LBtn_Pressed -= DeltaTime;

        FVector ForwardVector = GetControlRotation().Vector();
        BoatMesh->AddForce(ForwardVector * PaddleForwardForce * DeltaTime);
        //BoatMesh->AddTorqueInDegrees(FVector(-PaddleTurnTorque, 0, 0), NAME_None, true);
        BoatMesh->AddTorqueInRadians(FVector(0, 0, -PaddleTurnTorque), NAME_None, true);


    }

    if (HowLongHas_RBtn_Pressed > 0.0f) {
        HowLongHas_RBtn_Pressed -= DeltaTime;

        FVector ForwardVector = GetControlRotation().Vector();
        BoatMesh->AddForce(ForwardVector * PaddleForwardForce * DeltaTime);
        //BoatMesh->AddTorqueInDegrees(FVector(+PaddleTurnTorque, 0, 0), NAME_None, true);
        BoatMesh->AddTorqueInRadians(FVector(0, 0, PaddleTurnTorque), NAME_None, true);
    }
}






void APAP_Boat::StartLeftPaddle()
    {
    if (!BoatMesh) return;
    if (!MoveComp) return;

    HowLongHas_LBtn_Pressed = 5.0f;
}

void APAP_Boat::StopLeftPaddle() {


}

void APAP_Boat::StartRightPaddle()
{
    if (!BoatMesh) return;
    if (!MoveComp) return;

    HowLongHas_RBtn_Pressed = 5.0f;
}

void APAP_Boat::StopRightPaddle() {


}

void APAP_Boat::SwingPaddle(bool bIsLeftPaddling, bool bIsRightPaddling) {


}