// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IconLoad.generated.h"

USTRUCT(BlueprintType)
struct FIconStruct {
	GENERATED_BODY()

	TArray<UTexture2D*> TextureArray;

	TArray<FString> TextureNameArray;

};


UCLASS()
class AIconLoader : public AActor {

	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	UTexture2D* LoadTexture(const FString& path);

	UFUNCTION(BlueprintCallable)
	TArray<UTexture2D*> LoadTextures(const FString& path);

	UFUNCTION(BlueprintCallable)
	TArray<FString> LoadNames(const FString& path);



	UFUNCTION(BlueprintPure, Category = "Icon")
	static FString GetIconFolderPath();

	UFUNCTION(BlueprintCallable, Category = "Icon")
	TArray<UTexture2D*> LoadAllIcons();

	UFUNCTION(BlueprintCallable, Category = "Icon")
	TArray<FString> LoadAllIconsName();

	TSharedPtr<class IImageWrapper> GetImageWarpper(const FString& path);
};
