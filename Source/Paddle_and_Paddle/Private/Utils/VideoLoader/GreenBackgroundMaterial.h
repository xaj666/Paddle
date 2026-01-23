#pragma once

#include "CoreMinimal.h"


class UMaterial;
class FString;
class UTexture;

class FGreenBackgroundMaterial
{
public:
	// 头文件里先缓存好
	UPROPERTY()
	UMaterialInterface* ChromaKeyMat;   // 你在蓝图里做好的材质

	UPROPERTY()
	UMaterialInstanceDynamic* ChromaMID;
};