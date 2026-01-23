// Fill out your copyright notice in the Description page of Project Settings.

#include "Utils/IconLoad/IconLoad.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "HAL/FileManagerGeneric.h"

UTexture2D* AIconLoader::LoadTexture(const FString& path) {
	UTexture2D* Texture = nullptr;
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*path))
		return nullptr;
	TArray<uint8>Data;
	if (!FFileHelper::LoadFileToArray(Data, *path))
		return nullptr;

	TSharedPtr<IImageWrapper> ImageWrapper = GetImageWarpper(*path);
	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(Data.GetData(), Data.Num())) {
		TArray<uint8> ConRGB;
		if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, ConRGB)) {
			Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_R8G8B8A8);
			void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, ConRGB.GetData(), ConRGB.Num());
			Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
			Texture->UpdateResource();
		}
	}
	
	return Texture;
}

TArray<UTexture2D*> AIconLoader::LoadTextures(const FString& Path) {
	TArray<UTexture2D*> TextureArray;

	if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Path))
		return TextureArray;

	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFiles(Files, *Path, TEXT("*.png"));
	FileManager.FindFiles(Files, *Path, TEXT("*.jpg"));

	for (const FString& File : Files)
	{
		FString FullPath = Path / File;
		if (UTexture2D* Texture = LoadTexture(FullPath))
		{
			TextureArray.Add(Texture);
		}
	}

	return TextureArray;
}


FString AIconLoader::GetIconFolderPath()
{
	// FPaths::ProjectContentDir() 会自动处理编辑器和打包后的路径差异
	// 编辑器: D:/YourProject/Content/
	// 打包后: D:/PackagedGame/YourGame/Content/

	FString IconPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("PAP/MemeIcon"));

	UE_LOG(LogTemp, Log, TEXT("[IconLoader] Icon folder path: %s"), *IconPath);

	return IconPath;
}

TArray<UTexture2D*> AIconLoader::LoadAllIcons()
{
	return LoadTextures(GetIconFolderPath());
}

TSharedPtr<class IImageWrapper> AIconLoader::GetImageWarpper(const FString& path) {
	IImageWrapperModule& module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	if (path.EndsWith("png"))
		return module.CreateImageWrapper(EImageFormat::PNG);
	if (path.EndsWith("jpg"))
		return module.CreateImageWrapper(EImageFormat::JPEG);
	return nullptr;
}