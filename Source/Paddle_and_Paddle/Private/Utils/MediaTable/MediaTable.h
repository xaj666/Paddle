// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FileMediaSource.h"
#include "MediaTable.generated.h"

class UMediaTexture;

USTRUCT(BlueprintType)
struct FVideoRow : public FTableRowBase {

	GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName GiftName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<class UFileMediaSource> MediaSource;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UMediaTexture> MediaTexture;
};
