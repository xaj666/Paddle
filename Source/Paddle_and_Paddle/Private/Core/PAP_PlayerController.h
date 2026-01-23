// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PAP_PlayerController.generated.h"


class APAP_Boat;
/**
 * 
 */
UCLASS()
class APAP_PlayerController : public APlayerController
{
	GENERATED_BODY()
	
	public:
	// 构造函数
	APAP_PlayerController();

protected:
	// 开始播放时绑定输入
	virtual void BeginPlay() override;

	// 输入绑定核心函数
	void SetupInputComponent() override;

	// ---- 输入响应函数 ----
	// A键按下：触发左侧划桨（按下时标记状态）
	UFUNCTION()
	void OnLeftPaddlePressed();
	// A键松开：取消左侧划桨状态
	UFUNCTION()
	void OnLeftPaddleReleased();

	// D键按下：触发右侧划桨（按下时标记状态）
	UFUNCTION()
	void OnRightPaddlePressed();
	// D键松开：取消右侧划桨状态
	UFUNCTION()
	void OnRightPaddleReleased();

	// 空格键按下：触发挥动船桨（单次触发）
	UFUNCTION()
	void OnPaddleSwingPressed();

private:
	// 缓存当前控制的船对象
	UPROPERTY()
	APAP_Boat* ControlledBoat;

	// 左右划桨状态标记
	bool bIsLeftPaddling = false;
	bool bIsRightPaddling = false;

	// 获取并缓存当前控制的船
	void CacheControlledBoat();

};
