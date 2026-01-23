// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/PAP_BaseCharacter.h"
#include "PAP_Boat.generated.h"

/**
 * 
 */
UCLASS()
class APAP_Boat : public APAP_BaseCharacter
{
	GENERATED_BODY()

    APAP_Boat();
	
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

	void StartLeftPaddle();
	void StopLeftPaddle();
	void StartRightPaddle();
	void StopRightPaddle();
	void SwingPaddle(bool bIsLeftPaddling, bool bIsRightPaddling);

private:
    // 船体移动组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|Movement", meta = (AllowPrivateAccess = "true"))
    USkeletalMeshComponent* BoatMesh;

    UCharacterMovementComponent* MoveComp;

    // 前进动力大小
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Paddle")
    float PaddleForwardForce;

    // 转向角度（弧度）
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Paddle")
    float PaddleTurnInputScale;

    UPROPERTY(EditDefaultsOnly, Category = "Boat|Paddle")
    float PaddleTurnTorque;

    float HowLongHas_LBtn_Pressed = 0.0f;
    float HowLongHas_RBtn_Pressed = 0.0f;



}; 
