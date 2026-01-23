// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PAP_PlayerController.h"
#include "Character/PAP_Boat.h"


APAP_PlayerController::APAP_PlayerController()
{
	//默认情况下，鼠标隐藏，后面涉及UI部分再说
	bShowMouseCursor = false; 
}

void APAP_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	// 缓存控制的船
	CacheControlledBoat();
}

void APAP_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 绑定A键（按下/松开）
	InputComponent->BindAction("LeftPaddle", IE_Pressed, this, &APAP_PlayerController::OnLeftPaddlePressed);
	InputComponent->BindAction("LeftPaddle", IE_Released, this, &APAP_PlayerController::OnLeftPaddleReleased);

	// 绑定D键（按下/松开）
	InputComponent->BindAction("RightPaddle", IE_Pressed, this, &APAP_PlayerController::OnRightPaddlePressed);
	InputComponent->BindAction("RightPaddle", IE_Released, this, &APAP_PlayerController::OnRightPaddleReleased);

	// 绑定空格键（仅按下）
	InputComponent->BindAction("SwingPaddle", IE_Pressed, this, &APAP_PlayerController::OnPaddleSwingPressed);
}

// ---- 输入响应实现 ----
void APAP_PlayerController::OnLeftPaddlePressed()
{
	bIsLeftPaddling = true;
	if (ControlledBoat)
	{
		// 通知船：左侧划桨开始
		ControlledBoat->StartLeftPaddle();
	}
}

void APAP_PlayerController::OnLeftPaddleReleased()
{
	bIsLeftPaddling = false;
	if (ControlledBoat)
	{
		ControlledBoat->StopLeftPaddle();
	}
}

void APAP_PlayerController::OnRightPaddlePressed()
{
	bIsRightPaddling = true;
	if (ControlledBoat)
	{
		ControlledBoat->StartRightPaddle();
	}
}

void APAP_PlayerController::OnRightPaddleReleased()
{
	bIsRightPaddling = false;
	if (ControlledBoat)
	{
		ControlledBoat->StopRightPaddle();
	}
}

void APAP_PlayerController::OnPaddleSwingPressed()
{
	if (ControlledBoat)
	{
		// 通知船：挥动船桨（需区分左右状态，或统一挥动）
		ControlledBoat->SwingPaddle(bIsLeftPaddling, bIsRightPaddling);
	}
}

// 缓存控制的船
void APAP_PlayerController::CacheControlledBoat()
{
	// 获取当前控制的Pawn并转为船类
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledBoat = Cast<APAP_Boat>(ControlledPawn);
	}
}