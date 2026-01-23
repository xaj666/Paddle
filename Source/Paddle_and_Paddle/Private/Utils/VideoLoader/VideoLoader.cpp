#include "VideoLoader.h"
#include "FileMediaSource.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"

#include "GameMapsSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "GameFramework/GameUserSettings.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"  

#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Components/Image.h"

#include "Utils/VideoLoader/GreenBackgroundMaterial.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/GameInstance.h"
#include "MediaSoundComponent.h"
#include "Blueprint/UserWidget.h"

#include "../MediaTable/MediaTable.h"



/*===================================================================================*/
/*----------------------------------UVideoLoader-------------------------------------*/
/*===================================================================================*/

void UVideoLoader::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
        {
            ScanMoviesFolderAndCreateFMS();
            return false;
        }), 0.5f);
}

void UVideoLoader::Deinitialize()
{
    Super::Deinitialize();
}



void UVideoLoader::LoadSnapshot(FMediaSnapshot& Out)
{
    const FString ConfigDir = FPaths::ProjectSavedDir() / TEXT("Config");
    const FString FilePath = ConfigDir / TEXT("MediaSnapshot.json");

    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        FString JsonStr;
        if (FFileHelper::LoadFileToString(JsonStr, *FilePath))
        {
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

            if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
            {
                TArray<TSharedPtr<FJsonValue>> PathsArray = JsonObject->GetArrayField(TEXT("RelPaths"));
                for (const auto& PathValue : PathsArray)
                {
                    Out.RelPaths.Add(PathValue->AsString());
                }
            }
        }
    }
}

void UVideoLoader::SaveSnapshot(const FMediaSnapshot& In)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> PathsArray;

    for (const FString& Path : In.RelPaths)
    {
        PathsArray.Add(MakeShareable(new FJsonValueString(Path)));
    }

    JsonObject->SetArrayField(TEXT("RelPaths"), PathsArray);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    const FString ConfigDir = FPaths::ProjectSavedDir() / TEXT("Config");
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ConfigDir);

    const FString FilePath = ConfigDir / TEXT("MediaSnapshot.json");
    FFileHelper::SaveStringToFile(JsonStr, *FilePath);
}



void UVideoLoader::ScanMoviesFolderAndCreateFMS()
{
    const FString ContentPath = FPaths::ProjectContentDir();
    const FString MoviesPath = FPaths::Combine(ContentPath, TEXT("Movies"));

    TArray<FString> CurrentAbsFiles;
    IFileManager::Get().FindFilesRecursive(CurrentAbsFiles, *MoviesPath, TEXT("*.mp4"), true, false);

    TSet<FString> CurrentRelSet;
    for (FString& Abs : CurrentAbsFiles)
    {
        FPaths::MakePathRelativeTo(Abs, *ContentPath);
        CurrentRelSet.Add(Abs);
    }

    FMediaSnapshot LastSnap;
    LoadSnapshot(LastSnap);
    TSet<FString> LastRelSet(LastSnap.RelPaths);

    TSet<FString> Missing = LastRelSet.Difference(CurrentRelSet);

    FAssetRegistryModule& AssetReg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    for (const FString& Rel : Missing)
    {
        FString AssetName = FPaths::GetBaseFilename(Rel);
        {
            FString FMSPackagePath = TEXT("/Game/AutoMedia/FileMediaSource/") + AssetName;
            FAssetData FMSData = AssetReg.Get().GetAssetByObjectPath(FSoftObjectPath(FMSPackagePath + TEXT(".") + AssetName));
            if (!FMSData.IsValid()) continue;

            if (UObject* Obj = FMSData.GetAsset())
            {
                Obj->ClearFlags(RF_Standalone | RF_Public);
                Obj->Rename(nullptr, GetTransientPackage());
                Obj->ConditionalBeginDestroy();
            }
            if (UPackage* Pkg = FindPackage(nullptr, *FMSPackagePath))
            {
                Pkg->SetDirtyFlag(true);
                UPackage::SavePackage(Pkg, nullptr,
                    RF_Standalone | RF_Public,
                    *FPackageName::LongPackageNameToFilename(FMSPackagePath, FPackageName::GetAssetPackageExtension()));
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[MediaSnapshot] Removed FMS & MT for %s"), *Rel);
    }

    for (const FString& Rel : CurrentRelSet)
    {
        FString AssetName = FPaths::GetBaseFilename(Rel);
        FString FMSPackagePath = TEXT("/Game/AutoMedia/FileMediaSource/") + AssetName;
        FString FMSPackageName = FMSPackagePath + TEXT(".") + AssetName;

        bool bFMSExists = AssetReg.Get().GetAssetByObjectPath(FSoftObjectPath(FMSPackageName)).IsValid();
        UFileMediaSource* FMS = nullptr;

        if (!bFMSExists)
        {
            UPackage* Pkg = CreatePackage(*FMSPackagePath);
            FMS = NewObject<UFileMediaSource>(Pkg, *AssetName, RF_Public | RF_Standalone);
            FMS->SetFilePath(TEXT("./") + Rel);
            AssetReg.AssetCreated(FMS);
            FMS->MarkPackageDirty();
        }
    }

    FMediaSnapshot NewSnap;
    NewSnap.RelPaths = CurrentRelSet.Array();
    SaveSnapshot(NewSnap);

    UpdateVideoDataTable(CurrentRelSet);
}



UDataTable* UVideoLoader::LoadVideoDataTable()
{
    const FString DataTablePath = TEXT("/Game/PAP/UI/Video/VideoDataTable");

    UDataTable* VideoDataTable = LoadObject<UDataTable>(nullptr, *DataTablePath);

    if (!VideoDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MediaScanner] Failed to load VideoDataTable at path: %s"), *DataTablePath);

        FAssetRegistryModule& AssetReg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FAssetData AssetData = AssetReg.Get().GetAssetByObjectPath(FSoftObjectPath(DataTablePath));

        if (AssetData.IsValid())
        {
            VideoDataTable = Cast<UDataTable>(AssetData.GetAsset());
        }
    } 

    return VideoDataTable;
}


void UVideoLoader::UpdateVideoDataTable(const TSet<FString>& CurrentRelSet)
{
    UDataTable* VideoDataTable = LoadVideoDataTable();
    if (!VideoDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[MediaScanner] Cannot update DataTable - failed to load"));
        return;
    }

    if (VideoDataTable->RowStruct != FVideoRow::StaticStruct())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MediaScanner] DataTable row structure mismatch. Expected FVideoRow."));
        return;
    }

    TArray<FName> ExistingRowNames = VideoDataTable->GetRowNames();
    TSet<FName> ExistingNamesSet(ExistingRowNames);

    TSet<FName> VideoNamesToProcess;
    TMap<FName, FVideoRow> RowsToUpdate;

    int32 VideoIndex = 1;
    for (const FString& RelPath : CurrentRelSet)
    {
        FString VideoName = FPaths::GetBaseFilename(RelPath);

        FString GiftNameStr = VideoName;
        FName RowName = FName(*GiftNameStr);
        VideoNamesToProcess.Add(RowName);

        FString PackagePath = TEXT("/Game/AutoMedia/FileMediaSource/") + VideoName;
        FString FMSAssetPath = PackagePath + TEXT(".") + VideoName;

        FVideoRow NewRow;
        NewRow.GiftName = FName(*GiftNameStr);
        NewRow.MediaSource = TSoftObjectPtr<UFileMediaSource>(FSoftObjectPath(*FMSAssetPath));
        RowsToUpdate.Add(RowName, NewRow);

        UE_LOG(LogTemp, Log, TEXT("[MediaScanner] Processing video: %s -> %s"), *VideoName, *GiftNameStr);
    }

    TSet<FName> RowsToRemove = ExistingNamesSet.Difference(VideoNamesToProcess);
    for (const FName& RowName : RowsToRemove)
    {
        VideoDataTable->RemoveRow(RowName);
        UE_LOG(LogTemp, Log, TEXT("[MediaScanner] Removed row: %s"), *RowName.ToString());
    }

    for (const auto& RowPair : RowsToUpdate)
    {
        const FName& RowName = RowPair.Key;
        const FVideoRow& NewRow = RowPair.Value;

        FVideoRow* ExistingRow = VideoDataTable->FindRow<FVideoRow>(RowName, TEXT(""));

        if (ExistingRow)
        {
            *ExistingRow = NewRow;
            UE_LOG(LogTemp, Log, TEXT("[MediaScanner] Updated row: %s"), *RowName.ToString());
        }
        else
        {
            VideoDataTable->AddRow(RowName, NewRow);
            UE_LOG(LogTemp, Log, TEXT("[MediaScanner] Added new row: %s"), *RowName.ToString());
        }
    }

    VideoDataTable->MarkPackageDirty();

    UPackage* Pkg = VideoDataTable->GetPackage();
    if (Pkg)
    {
        Pkg->SetDirtyFlag(true);

        FString PackageFilename;
        if (FPackageName::DoesPackageExist(Pkg->GetName(), &PackageFilename))
        {
            UPackage::SavePackage(Pkg, nullptr,
                RF_Standalone | RF_Public,
                *PackageFilename);

            UE_LOG(LogTemp, Log, TEXT("[MediaScanner] Saved VideoDataTable with %d video entries"), RowsToUpdate.Num());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[MediaScanner] Cannot save DataTable - package file not found"));
        }
    }
}





/*===================================================================================*/
/*---------------------------------UGiftVideoPool------------------------------------*/
/*===================================================================================*/

void UGiftVideoPool::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    VideoUIClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/PAP/UI/Video/UI_Video.UI_Video_C"));
    InitPool();
}

void UGiftVideoPool::Deinitialize()
{

    // [Claude] 清理所有单次礼物队列
    ClearAllSingleGiftQueues();

    // [Claude] 清理所有 Trigger 队列
    ClearAllTriggerQueues();

    // 清理顺序播放队列
    StopAllSequentialPlays();

    // [Claude] 清理触发队列上下文
    TriggerQueueContextMap.Empty();

    // 清理逻辑调用定时器
    CleanupLogicTimers();

    // 清理批次播放的 Timer
    UWorld* World = GetWorld();
    if (World)
    {
        for (FBatchPlayRequest& Batch : BatchRequests)
        {
            if (Batch.IntervalTimerHandle.IsValid())
            {
                World->GetTimerManager().ClearTimer(Batch.IntervalTimerHandle);
            }
        }
    }
    BatchRequests.Empty();

    CleanupProxies();

    for (FSlot& S : VideoSlots)
    {
        if (S.Player)
        {
            S.Player->Close();
            S.Player->OnEndReached.RemoveAll(this);
        }

        if (S.SoundComp)
        {
            S.SoundComp->DestroyComponent();
            S.SoundComp = nullptr;
        }
    }
    VideoSlots.Empty();

    Super::Deinitialize();
}


void UGiftVideoPool::InitPool()
{
    UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();

    for (int32 i = 0; i < POOL_SIZE; ++i)
    {
        FSlot& S = VideoSlots.AddDefaulted_GetRef();
        S.Player = NewObject<UMediaPlayer>(this);
        S.Texture = NewObject<UMediaTexture>(this);

        S.Texture->SetMediaPlayer(S.Player);
        S.Texture->AutoClear = true;
        S.Texture->ClearColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);  // 绿色背景，会被 Chroma Key 抠掉
        S.Texture->UpdateResource();

        S.PoolIndex = i;
        S.UI = CreateWidget<UUserWidget>(GetWorld(), VideoUIClass);

        if (S.UI)
        {
            S.VideoImage = Cast<UImage>(S.UI->GetWidgetFromName(TEXT("VideoImage")));
        }

        // 提前绑定材质
        if (Chroma && S.VideoImage && S.Texture)
        {
            Chroma->SetVideoToImage(S.VideoImage, S.Texture);
        }

        CreateProxyForPlayer(S.Player, i);

        S.bBusy = false;
    }

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] InitPool completed with %d slots"), POOL_SIZE);
}


FSlot* UGiftVideoPool::FindFreeSlot()
{
    for (auto& S : VideoSlots)
    {
        if (!S.bBusy && (S.Player->IsClosed() || (!S.Player->IsPlaying() && !S.Player->IsPreparing())))
        {
            return &S;
        }
    }
    return nullptr;
}


FSlot* UGiftVideoPool::CreateNewSlot(AActor* Actor)
{
    FSlot& NewSlot = VideoSlots.AddDefaulted_GetRef();
    NewSlot.Player = NewObject<UMediaPlayer>(this);
    NewSlot.Texture = NewObject<UMediaTexture>(this);

    NewSlot.Texture->SetMediaPlayer(NewSlot.Player);
    NewSlot.Texture->AutoClear = true;
    NewSlot.Texture->ClearColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
    NewSlot.Texture->UpdateResource();

    NewSlot.PoolIndex = VideoSlots.Num() - 1;
    NewSlot.UI = CreateWidget<UUserWidget>(GetWorld(), VideoUIClass);


    if (NewSlot.UI)
    {
        NewSlot.VideoImage = Cast<UImage>(NewSlot.UI->GetWidgetFromName(TEXT("VideoImage")));
    }

    NewSlot.SoundComp = NewObject<UMediaSoundComponent>(Actor);
    NewSlot.SoundComp->RegisterComponent();
    NewSlot.SoundComp->SetMediaPlayer(NewSlot.Player);
    NewSlot.SoundComp->bIsUISound = true;
    NewSlot.SoundComp->SetActive(false);

    // 绑定材质
    UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();
    if (Chroma && NewSlot.VideoImage && NewSlot.Texture)
    {
        Chroma->SetVideoToImage(NewSlot.VideoImage, NewSlot.Texture);
    }

    CreateProxyForPlayer(NewSlot.Player, NewSlot.PoolIndex);

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Created new slot %d"), NewSlot.PoolIndex);

    return &NewSlot;
}


int32 UGiftVideoPool::PlayGiftVideo(UFileMediaSource* MediaSource, AActor* Actor)
{
    //if (!MediaSource) return -1;

    //FSlot* Slot = FindFreeSlot();

    //if (!Slot)
    //{
    //    Slot = CreateNewSlot(Actor);
    //}

    //// 标记为忙碌
    //Slot->bBusy = true;

    //// 确保 UI 不在视口
    //if (Slot->UI && Slot->UI->IsInViewport())
    //{
    //    Slot->UI->RemoveFromParent();
    //}

    //// 关闭音频
    //if (Slot->SoundComp)
    //{
    //    Slot->SoundComp->SetActive(false);
    //}

    //// 确保 MT 绑定到 MP
    //Slot->Texture->SetMediaPlayer(Slot->Player);
    //Slot->Texture->UpdateResource();

    //if (!Slot->SoundComp)
    //{
    //    Slot->SoundComp = NewObject<UMediaSoundComponent>(Actor);
    //    Slot->SoundComp->RegisterComponent();
    //    Slot->SoundComp->SetMediaPlayer(Slot->Player);
    //    Slot->SoundComp->bIsUISound = true;
    //    Slot->SoundComp->SetActive(false);
    //}

    //// 绑定材质
    //UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();
    //if (Chroma && Slot->VideoImage && Slot->Texture)
    //{
    //    Chroma->SetVideoToImage(Slot->VideoImage, Slot->Texture);
    //}

    //FMediaPlayerOptions Options;
    //Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Enabled;  // 打开后自动播放
    //Options.Loop = EMediaPlayerOptionBooleanOverride::Disabled;
    //Slot->Player->OpenSourceWithOptions(MediaSource, Options);

    //return Slot->PoolIndex;
    return -1;
}


void UGiftVideoPool::PlayGiftVideoBatch(UFileMediaSource* MediaSource, AActor* Actor, int32 PlayCount, float Interval)
{
    //if (!MediaSource || PlayCount <= 0) return;

    //UE_LOG(LogTemp, Log, TEXT("[VideoPool] BatchPlay: Count=%d, Interval=%.2fs"), PlayCount, Interval);

    //// 创建批次请求
    //FBatchPlayRequest& Batch = BatchRequests.AddDefaulted_GetRef();
    //Batch.MediaSource = MediaSource;
    //Batch.PlayCount = PlayCount;
    //Batch.Interval = Interval;
    //Batch.CurrentIndex = 0;
    //Batch.OwnerActor = Actor;

    //// 预分配所有需要的 Slot
    //for (int32 i = 0; i < PlayCount; ++i)
    //{
    //    FSlot* Slot = FindFreeSlot();

    //    if (!Slot)
    //    {
    //        Slot = CreateNewSlot(Actor);
    //    }

    //    // 标记为忙碌，防止被其他请求使用
    //    Slot->bBusy = true;

    //    // 预加载媒体（但不播放）
    //    Slot->Texture->SetMediaPlayer(Slot->Player);
    //    Slot->Texture->UpdateResource();

    //    if (!Slot->SoundComp)
    //    {
    //        Slot->SoundComp = NewObject<UMediaSoundComponent>(Actor);
    //        Slot->SoundComp->RegisterComponent();
    //        Slot->SoundComp->SetMediaPlayer(Slot->Player);
    //        Slot->SoundComp->bIsUISound = true;
    //    }
    //    Slot->SoundComp->SetActive(false);

    //    // 绑定材质
    //    UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();
    //    if (Chroma && Slot->VideoImage && Slot->Texture)
    //    {
    //        Chroma->SetVideoToImage(Slot->VideoImage, Slot->Texture);
    //    }

    //    // 预加载但不播放
    //    FMediaPlayerOptions Options;
    //    Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Disabled;
    //    Options.Loop = EMediaPlayerOptionBooleanOverride::Disabled;
    //    Slot->Player->OpenSourceWithOptions(MediaSource, Options);

    //    Batch.SlotIndices.Add(Slot->PoolIndex);

    //    UE_LOG(LogTemp, Log, TEXT("[VideoPool] BatchPlay: Preloaded slot %d for index %d"), Slot->PoolIndex, i);
    //}

    //int32 BatchIndex = BatchRequests.Num() - 1;

    //// 立即播放第一个
    //StartSlotPlayback(Batch.SlotIndices[0]);
    //Batch.CurrentIndex = 1;

    //// 如果还有更多，设置定时器
    //if (PlayCount > 1)
    //{
    //    UWorld* World = GetWorld();
    //    if (World)
    //    {
    //        World->GetTimerManager().SetTimer(
    //            Batch.IntervalTimerHandle,
    //            FTimerDelegate::CreateUObject(this, &UGiftVideoPool::OnBatchIntervalTick, BatchIndex),
    //            Interval,
    //            true  // 循环
    //        );
    //    }
    //}
}


void UGiftVideoPool::OnBatchIntervalTick(int32 BatchIndex)
{
    //if (!BatchRequests.IsValidIndex(BatchIndex))
    //    return;

    //FBatchPlayRequest& Batch = BatchRequests[BatchIndex];

    //if (Batch.CurrentIndex >= Batch.PlayCount)
    //{
    //    // 批次完成
    //    UWorld* World = GetWorld();
    //    if (World && Batch.IntervalTimerHandle.IsValid())
    //    {
    //        World->GetTimerManager().ClearTimer(Batch.IntervalTimerHandle);
    //    }

    //    UE_LOG(LogTemp, Log, TEXT("[VideoPool] BatchPlay completed: %d videos"), Batch.PlayCount);

    //    // 广播完成事件
    //    OnBatchPlayCompleted.Broadcast();

    //    return;
    //}

    //// 播放下一个
    //int32 SlotIndex = Batch.SlotIndices[Batch.CurrentIndex];
    //StartSlotPlayback(SlotIndex, SoundValue);

    //UE_LOG(LogTemp, Log, TEXT("[VideoPool] BatchPlay: Playing index %d, slot %d"), Batch.CurrentIndex, SlotIndex);

    //Batch.CurrentIndex++;
}


void UGiftVideoPool::StartSlotPlayback(int32 SlotIndex)
{
    if (!VideoSlots.IsValidIndex(SlotIndex))
        return;

    FSlot& Slot = VideoSlots[SlotIndex];

    // 显示 UI
    if (Slot.UI && !Slot.UI->IsInViewport())
        Slot.UI->AddToViewport();

    // 激活音频
    if (Slot.SoundComp)
        Slot.SoundComp->Activate(true);

    // 从头开始播放
    if (Slot.Player){
        Slot.Player->Seek(FTimespan::Zero());
        Slot.Player->Play();
    }
}


UMediaTexture* UGiftVideoPool::GetSlotTexture(int32 SlotIndex)
{
    if (VideoSlots.IsValidIndex(SlotIndex))
    {
        return VideoSlots[SlotIndex].Texture;
    }
    return nullptr;
}

UMediaPlayer* UGiftVideoPool::GetMediaPlayerBySlot(int32 SlotIndex)
{
    if (VideoSlots.IsValidIndex(SlotIndex))
    {
        return VideoSlots[SlotIndex].Player;
    }
    return nullptr;
}

UUserWidget* UGiftVideoPool::GetVideoUIBySlot(int32 SlotIndex)
{
    if (VideoSlots.IsValidIndex(SlotIndex))
    {
        return VideoSlots[SlotIndex].UI;
    }
    return nullptr;
}

void UGiftVideoPool::ActivateSoundCompBySlotIndex(int32 SlotIndex)
{
    if (VideoSlots.IsValidIndex(SlotIndex))
    {
        VideoSlots[SlotIndex].SoundComp->SetActive(true);
    }
}

void UGiftVideoPool::ResumeVideo(int32 SlotIndex)
{
    if (UMediaPlayer* Player = GetMediaPlayerBySlot(SlotIndex))
    {
        Player->Play();
    }
}

void UGiftVideoPool::PauseVideo(int32 SlotIndex)
{
    if (UMediaPlayer* Player = GetMediaPlayerBySlot(SlotIndex))
    {
        Player->Pause();
    }
}

void UGiftVideoPool::PlayFromStart(int32 SlotIndex)
{
    if (UMediaPlayer* Player = GetMediaPlayerBySlot(SlotIndex))
    {
        Player->Seek(FTimespan::Zero());
        Player->Play();
    }
}

UAudioComponent* UGiftVideoPool::GetSlotAudioComponent(int32 SlotIndex)
{
    if (!VideoSlots.IsValidIndex(SlotIndex))
        return nullptr;

    return nullptr;
}


void UGiftVideoPool::CreateProxyForPlayer(UMediaPlayer* Player, int32 PlayerID)
{
    if (!Player || PlayerToProxyMap.Contains(Player)) return;

    UMediaEndProxy* Proxy = NewObject<UMediaEndProxy>(this);
    Proxy->InitProxy(PlayerID, this);

    Player->OnMediaOpened.AddDynamic(Proxy, &UMediaEndProxy::OnProxyMediaOpened);
    Player->OnEndReached.AddDynamic(Proxy, &UMediaEndProxy::OnProxyMediaEnd);

    PlayerToProxyMap.Add(Player, Proxy);
}


void UGiftVideoPool::HandleMediaOpened(int32 PlayerID)
{
    if (!VideoSlots.IsValidIndex(PlayerID))
        return;

    FSlot& Slot = VideoSlots[PlayerID];

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Slot %d: Media opened"), PlayerID);

    // 显示 UI
    if (Slot.UI && !Slot.UI->IsInViewport())
    {
        Slot.UI->AddToViewport();
    }

    // 激活音频
    if (Slot.SoundComp)
    {
        Slot.SoundComp->Activate(true);
    }
}


void UGiftVideoPool::HandleMediaEnd(int32 PlayerID)
{
    if (!VideoSlots.IsValidIndex(PlayerID))
        return;

    FSlot& Slot = VideoSlots[PlayerID];

    /*if (!Slot.bBusy)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VideoPool] Slot %d: HandleMediaEnd called but slot is not busy, ignoring duplicate"), PlayerID);
        return;
    }*/

    // ========== 新增：先处理顺序播放逻辑 ==========
    // 检查是否属于顺序播放队列（在重置状态之前）
    bool bIsSequentialSlot = SlotToQueueMap.Contains(PlayerID);

    // 重置 UI 配置到默认
    ResetUIToDefault(PlayerID);

    // 移除 UI
    if (Slot.UI && Slot.UI->IsInViewport())
        Slot.UI->RemoveFromParent();

    // 关闭音频
    if (Slot.SoundComp)
        Slot.SoundComp->SetActive(false);

    // 释放槽位
    Slot.bBusy = false;

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Slot %d: Media ended, slot released"), PlayerID);

    // 广播通用的视频结束委托
    OnVideoEndedinBP.Broadcast(PlayerID);

    // ========== 新增：处理顺序播放 ==========
    if (bIsSequentialSlot)
    {
        HandleSequentialMediaEnd(PlayerID);
    }
}

void UGiftVideoPool::BindUIWithPlayerID(int32 PlayerID, UUserWidget* TargetUI)
{
    if (TargetUI)
    {
        IDToWidgetMap.Add(PlayerID, TargetUI);
    }
}

void UGiftVideoPool::CleanupProxies()
{
    for (auto& Pair : PlayerToProxyMap)
    {
        if (Pair.Key)
        {
            Pair.Key->OnEndReached.RemoveAll(Pair.Value);
        }
        if (Pair.Value)
        {
            Pair.Value->ConditionalBeginDestroy();
        }
    }
    PlayerToProxyMap.Empty();
    IDToWidgetMap.Empty();
}



// VideoLoader.cpp 中添加实现

void UGiftVideoPool::PlayGiftVideoWithLogic(const FVideoPlayConfig& Config, float SoundValue, AActor* Actor)
{
    if (!Config.MediaSources[0] || Config.PlayCount <= 0)
        return;


    // 预分配所有需要的 Slot
    TArray<int32> AllocatedSlots;
    for (int32 i = 0; i < Config.PlayCount; ++i)
    {
        FSlot* Slot = FindFreeSlot();
        if (!Slot)
        {
            Slot = CreateNewSlot(Actor);
        }

        Slot->bBusy = true;
        Slot->Texture->SetMediaPlayer(Slot->Player);
        Slot->Texture->UpdateResource();

        if (!Slot->SoundComp)
        {
            Slot->SoundComp = NewObject<UMediaSoundComponent>(Actor);
            Slot->SoundComp->RegisterComponent();
            Slot->SoundComp->SetMediaPlayer(Slot->Player);
            Slot->SoundComp->bIsUISound = true;  
        }
        Slot->SoundComp->SetActive(false);
        Slot->SoundComp->SetVolumeMultiplier(SoundValue);


        // 绑定材质
        UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();
        if (Chroma && Slot->VideoImage && Slot->Texture)
        {
            Chroma->SetVideoToImage(Slot->VideoImage, Slot->Texture);
        }

        // 预加载但不播放
        FMediaPlayerOptions Options;
        Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Disabled;
        Options.Loop = EMediaPlayerOptionBooleanOverride::Disabled;
        Slot->Player->OpenSourceWithOptions(Config.MediaSources[0], Options);

        AllocatedSlots.Add(Slot->PoolIndex);
    }

    UWorld* World = GetWorld();
    if (!World) return;

    // 为每次视频播放安排视频启动和逻辑触发
    for (int32 i = 0; i < AllocatedSlots.Num(); ++i)
    {
        float FirstVideoDelayTime = 0.3f;
        float VideoStartTime = FirstVideoDelayTime + (i - 1) * Config.VideoInterval;
        int32 SlotIndex = AllocatedSlots[i];

        // 如果使用自定义 UI 配置，先应用配置
        if (Config.bUseCustomUIConfig)
            ApplyUIConfig(SlotIndex, Config.UIDisplayConfig[0]);


        /*-----------*/
        // 安排视频播放
        /*-----------*/
        if (i == 0)   
            StartSlotPlayback(SlotIndex); // 立即播放第一个
        else if (i == 1) {
            FTimerHandle VideoTimerHandle;
            World->GetTimerManager().SetTimer(
                VideoTimerHandle,
                FTimerDelegate::CreateLambda([this, SlotIndex]()
                    {
                        StartSlotPlayback(SlotIndex);
                    }),
                FirstVideoDelayTime,// 第一个视频用自定义的间隔时长
                false
            );
        }
        else
        {
            FTimerHandle VideoTimerHandle;
            World->GetTimerManager().SetTimer(
                VideoTimerHandle,
                FTimerDelegate::CreateLambda([this, SlotIndex]()
                    {
                        StartSlotPlayback(SlotIndex);
                    }),
                VideoStartTime,// 后面的视频用固定间隔
                false
            );
        }


        /*-----------*/
        //安排逻辑触发
        /*-----------*/
        // 第一个视频不触发逻辑（与原函数保持一致）
        if (i != 0)
            if (i == 1)
                for (int32 j = 0; j < Config.LogicCallCount; ++j)
                {
                    float LogicTriggerTime = VideoStartTime + Config.LogicTimeOffset;

                    // 确保逻辑触发时间不为负
                    if (LogicTriggerTime < 0.0f)
                    {
                        LogicTriggerTime = 0.0f;
                    }

                    ScheduleLogicCall(
                        Config.GiftEnumValue[0],
                        j + 1,  // ActivateTime: 第几次调用
                        Config.LogicDelayTime,
                        FirstVideoDelayTime + Config.LogicTimeOffset // 第一个视频的逻辑用自定义的间隔时长
                    );
                }
            else
                // 安排逻辑触发（可能多次）
                for (int32 j = 0; j < Config.LogicCallCount; ++j)
                {
                    float LogicTriggerTime = VideoStartTime + Config.LogicTimeOffset;

                    // 确保逻辑触发时间不为负
                    if (LogicTriggerTime < 0.0f)
                    {
                        LogicTriggerTime = 0.0f;
                    }

                    ScheduleLogicCall(
                        Config.GiftEnumValue[0],
                        j + 1,  // ActivateTime: 第几次调用
                        Config.LogicDelayTime,
                        LogicTriggerTime // 往后的视频用固定间隔
                    );
                }
    }




    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Scheduled %d video plays and %d logic calls"),
        Config.PlayCount, Config.PlayCount * Config.LogicCallCount);
}



void UGiftVideoPool::PlayRandomGiftVideoWithLogic(const FVideoPlayConfig& Config, float SoundValue, AActor* Actor)
{
    if (Config.MediaSources.Num() <= 0)
        return;

    const int32 PlayCount = 1 + Config.MediaSources.Num();

    // 预分配所有需要的 Slot，并为每个 Slot 设置对应的 MediaSource
    TArray<int32> AllocatedSlots;
    for (int32 i = 0; i < PlayCount; ++i)
    {
        /*if(i == 0)
            continue;*/
        /*if(i == Config.MediaSources.Num()){
            continue;
        }*/
        int j = i - 1;//通过J和I的映射，让MediaSource的第一个成员多复制一份占据列表中[0]的位置，这样在后面Index=0的槽位被跳过的时候能抵消掉
        if (j == -1) {
            UFileMediaSource* CurrentMediaSource = Config.MediaSources[j + 1];

            FSlot* Slot = FindFreeSlot();
            if (!Slot)
            {
                Slot = CreateNewSlot(Actor);
            }

            Slot->bBusy = true;
            Slot->Texture->SetMediaPlayer(Slot->Player);
            Slot->Texture->UpdateResource();

            if (!Slot->SoundComp)
            {
                Slot->SoundComp = NewObject<UMediaSoundComponent>(Actor);
                Slot->SoundComp->RegisterComponent();
                Slot->SoundComp->SetMediaPlayer(Slot->Player);
                Slot->SoundComp->bIsUISound = true;
            }
            Slot->SoundComp->SetActive(false);
            Slot->SoundComp->SetVolumeMultiplier(SoundValue);

            // 绑定材质
            UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();
            if (Chroma && Slot->VideoImage && Slot->Texture)
            {
                Chroma->SetVideoToImage(Slot->VideoImage, Slot->Texture);
            }

            // 预加载对应的 MediaSource（但不播放）
            FMediaPlayerOptions Options;
            Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Disabled;
            Options.Loop = EMediaPlayerOptionBooleanOverride::Disabled;
            Slot->Player->OpenSourceWithOptions(CurrentMediaSource, Options);

            AllocatedSlots.Add(Slot->PoolIndex);
        }
        else {
            UFileMediaSource* CurrentMediaSource = Config.MediaSources[j];

            FSlot* Slot = FindFreeSlot();
            if (!Slot)
            {
                Slot = CreateNewSlot(Actor);
            }

            Slot->bBusy = true;
            Slot->Texture->SetMediaPlayer(Slot->Player);
            Slot->Texture->UpdateResource();

            if (!Slot->SoundComp)
            {
                Slot->SoundComp = NewObject<UMediaSoundComponent>(Actor);
                Slot->SoundComp->RegisterComponent();
                Slot->SoundComp->SetMediaPlayer(Slot->Player);
                Slot->SoundComp->bIsUISound = true;
            }
            Slot->SoundComp->SetActive(false);
            Slot->SoundComp->SetVolumeMultiplier(SoundValue);

            // 绑定材质
            UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();
            if (Chroma && Slot->VideoImage && Slot->Texture)
            {
                Chroma->SetVideoToImage(Slot->VideoImage, Slot->Texture);
            }

            // 预加载对应的 MediaSource（但不播放）
            FMediaPlayerOptions Options;
            Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Disabled;
            Options.Loop = EMediaPlayerOptionBooleanOverride::Disabled;
            Slot->Player->OpenSourceWithOptions(CurrentMediaSource, Options);

            AllocatedSlots.Add(Slot->PoolIndex);
        }
            

        

    }

    //给计时器用
    UWorld* World = GetWorld();
            if (!World) return;

    // 为每次视频播放安排视频启动和逻辑触发
    for (int32 i = 0; i < AllocatedSlots.Num(); ++i)
    {
        float FirstVideoDelayTime = 0.3f;
        float VideoStartTime = FirstVideoDelayTime + (i - 1) * Config.VideoInterval;
        int32 SlotIndex = AllocatedSlots[i];

        // 如果使用自定义 UI 配置，先应用配置
        if (i != 0) {
            int j = i - 1;
            if (Config.bUseCustomUIConfig)
                ApplyUIConfig(SlotIndex, Config.UIDisplayConfig[j]);
        }
            


        /*-----------*/
        // 安排视频播放
        /*-----------*/
        if (i == 0)
        {
            // 立即播放第一个
            StartSlotPlayback(SlotIndex);
        }
        else if(i == 1){
            FTimerHandle VideoTimerHandle;
            World->GetTimerManager().SetTimer(
                VideoTimerHandle,
                FTimerDelegate::CreateLambda([this, SlotIndex]()
                    {
                        StartSlotPlayback(SlotIndex);
                    }),
                FirstVideoDelayTime,// 第一个视频用自定义的间隔时长
                false
            );
        }
        else
        {
            FTimerHandle VideoTimerHandle;
            World->GetTimerManager().SetTimer(
                VideoTimerHandle,
                FTimerDelegate::CreateLambda([this, SlotIndex]()
                    {
                        StartSlotPlayback(SlotIndex);
                    }),
                VideoStartTime,// 后面的视频用固定间隔
                false
            );
        }


        /*-----------*/
        //安排逻辑触发
        /*-----------*/
        // 第一个视频不触发逻辑（与原函数保持一致）
        if (i == 0)
            continue;
        if (i == 1) {
            for (int32 j = 0; j < Config.LogicCallCount; ++j)
            {
                float LogicTriggerTime = VideoStartTime + Config.LogicTimeOffset;

                // 确保逻辑触发时间不为负
                if (LogicTriggerTime < 0.0f)
                {
                    LogicTriggerTime = 0.0f;
                }
                if (i <= Config.GiftEnumValue.Num())
                    ScheduleLogicCall(
                        Config.GiftEnumValue[i-1],
                        j + 1,  // ActivateTime: 第几次调用
                        Config.LogicDelayTime,
                        FirstVideoDelayTime // 第一个视频的逻辑用自定义的间隔时长
                    );
            } 
        }
        else {
            // 安排逻辑触发（可能多次）
            for (int32 j = 0; j < Config.LogicCallCount; ++j)
            {
                float LogicTriggerTime = VideoStartTime + Config.LogicTimeOffset;

                // 确保逻辑触发时间不为负
                if (LogicTriggerTime < 0.0f)
                {
                    LogicTriggerTime = 0.0f;
                }
                if (i <= Config.GiftEnumValue.Num())
                    ScheduleLogicCall(
                        Config.GiftEnumValue[i-1],
                        j + 1,  // ActivateTime: 第几次调用
                        Config.LogicDelayTime,
                        LogicTriggerTime // 往后的视频用固定间隔
                    );
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] RandomPlay: Scheduled %d video plays and %d logic calls"),
        AllocatedSlots.Num(), (AllocatedSlots.Num() > 0 ? AllocatedSlots.Num() - 1 : 0) * Config.LogicCallCount);
}



void UGiftVideoPool::TriggerGiftLogicBatch(
    uint8 GiftEnumValue,
    int32 TriggerCount,
    float TriggerInterval,
    int32 LogicCallCountPerTrigger,
    float LogicDelayTime,
    float InitialDelay)
{
    if (TriggerCount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VideoPool] TriggerGiftLogicBatch: Invalid TriggerCount=%d"), TriggerCount);
        return;
    }

    if (TriggerInterval < 0.0f) TriggerInterval = 0.0f;
    if (LogicCallCountPerTrigger <= 0) LogicCallCountPerTrigger = 1;
    if (InitialDelay < 0.0f) InitialDelay = 0.0f;

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] TriggerGiftLogicBatch: GiftEnum=%d, Count=%d, Interval=%.2f, CallsPerTrigger=%d"),
        GiftEnumValue, TriggerCount, TriggerInterval, LogicCallCountPerTrigger);

    UWorld* World = GetWorld();
    if (!World) return;

    for (int32 i = 0; i < TriggerCount; ++i)
    {
        float TriggerTime = InitialDelay + (i * TriggerInterval);

        for (int32 j = 0; j < LogicCallCountPerTrigger; ++j)
        {
            ScheduleLogicCall(
                GiftEnumValue,
                j + 1,
                LogicDelayTime,
                TriggerTime
            );
        }
    }
}

void UGiftVideoPool::ScheduleLogicCall(uint8 GiftEnumValue, int32 ActivateTime, float DelayTime, float TimeOffset)
{
    UWorld* World = GetWorld();
    if (!World) return;

    FLogicCallRequest& Request = PendingLogicCalls.AddDefaulted_GetRef();
    Request.GiftEnumValue = GiftEnumValue;
    Request.ActivateTime = ActivateTime;
    Request.DelayTime = DelayTime;

    int32 RequestIndex = PendingLogicCalls.Num() - 1;

    if (TimeOffset <= 0.0f)
    {
        // 立即执行
        ExecuteLogicCall(RequestIndex);
    }
    else
    {
        // 延迟执行
        World->GetTimerManager().SetTimer(
            Request.TimerHandle,
            FTimerDelegate::CreateUObject(this, &UGiftVideoPool::ExecuteLogicCall, RequestIndex),
            TimeOffset,
            false
        );
    }
}

void UGiftVideoPool::ExecuteLogicCall(int32 RequestIndex)
{
    if (!PendingLogicCalls.IsValidIndex(RequestIndex))
    {
        return;
    }

    const FLogicCallRequest& Request = PendingLogicCalls[RequestIndex];

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Executing logic call: GiftEnum=%d, ActivateTime=%d, DelayTime=%.2f"),
        Request.GiftEnumValue, Request.ActivateTime, Request.DelayTime);

    // 广播委托，蓝图中监听此委托来调用 GiftFunc
    OnGiftLogicTrigger.Broadcast(Request.GiftEnumValue, Request.ActivateTime, Request.DelayTime);
}

void UGiftVideoPool::CleanupLogicTimers()
{
    UWorld* World = GetWorld();
    if (World)
    {
        for (FLogicCallRequest& Request : PendingLogicCalls)
        {
            if (Request.TimerHandle.IsValid())
            {
                World->GetTimerManager().ClearTimer(Request.TimerHandle);
            }
        }
    }
    PendingLogicCalls.Empty();
}

void UGiftVideoPool::ApplyUIConfig(int32 SlotIndex, const FGiftUIDisplayConfig& Config)
{
    if (!VideoSlots.IsValidIndex(SlotIndex))
        return;

    FSlot& Slot = VideoSlots[SlotIndex];
    if (!Slot.UI)
        return;

    // 保存配置
    Slot.CurrentUIConfig = Config;
    Slot.bHasCustomConfig = true;

    // 构建 RenderTransform
    FWidgetTransform Transform;
    Transform.Translation = Config.Translation;
    Transform.Scale = Config.Scale;
    Transform.Angle = Config.Rotation;

    Slot.UI->SetRenderTransform(Transform);

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Slot %d: Applied UI config - Scale(%.2f, %.2f), Translation(%.2f, %.2f)"),
        SlotIndex, Config.Scale.X, Config.Scale.Y, Config.Translation.X, Config.Translation.Y);
}

void UGiftVideoPool::ResetUIToDefault(int32 SlotIndex)
{
    if (!VideoSlots.IsValidIndex(SlotIndex))
        return;

    FSlot& Slot = VideoSlots[SlotIndex];
    if (!Slot.UI || !Slot.bHasCustomConfig)
        return;

    // 重置为默认
    FWidgetTransform DefaultTransform;
    DefaultTransform.Translation = FVector2D::ZeroVector;
    DefaultTransform.Scale = FVector2D(1.0f, 1.0f);
    DefaultTransform.Angle = 0.0f;

    Slot.UI->SetRenderTransform(DefaultTransform);
    Slot.bHasCustomConfig = false;

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Slot %d: Reset UI to default"), SlotIndex);
}



// ========== 顺序播放系统实现 ==========

int32 UGiftVideoPool::StartSequentialPlay(const TArray<FSequentialPlayItem>& Items, float SoundValue, AActor* Actor)
{
    if (Items.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VideoPool] StartSequentialPlay: Empty items array"));
        return INDEX_NONE;
    }

    // 创建新队列
    FSequentialPlayQueue& Queue = SequentialQueues.AddDefaulted_GetRef();
    Queue.QueueID = NextQueueID++;
    Queue.Items = Items;
    Queue.CurrentIndex = 0;
    Queue.SoundValue = SoundValue;
    Queue.OwnerActor = Actor;
    Queue.bIsPlaying = true;

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Created sequential queue %d with %d items"), Queue.QueueID, Items.Num());

    // 开始播放第一个
    PlayNextInQueue(Queue.QueueID);

    return Queue.QueueID;
}

void UGiftVideoPool::StopSequentialPlay(int32 QueueID)
{
    UWorld* World = GetWorld();

    for (int32 i = SequentialQueues.Num() - 1; i >= 0; --i)
    {
        if (SequentialQueues[i].QueueID == QueueID)
        {
            FSequentialPlayQueue& Queue = SequentialQueues[i];

            // 清理定时器
            if (World)
            {
                if (Queue.NoVideoTimerHandle.IsValid())
                {
                    World->GetTimerManager().ClearTimer(Queue.NoVideoTimerHandle);
                }
                // [Claude] 清理间隔定时器
                if (Queue.IntervalTimerHandle.IsValid())
                {
                    World->GetTimerManager().ClearTimer(Queue.IntervalTimerHandle);
                }
            }

            // 如果当前有正在播放的槽位，停止它
            if (Queue.CurrentSlotIndex != INDEX_NONE && VideoSlots.IsValidIndex(Queue.CurrentSlotIndex))
            {
                FSlot& Slot = VideoSlots[Queue.CurrentSlotIndex];
                if (Slot.Player)
                {
                    Slot.Player->Close();
                }
                // 清理映射
                SlotToQueueMap.Remove(Queue.CurrentSlotIndex);
            }

            Queue.bIsPlaying = false;
            SequentialQueues.RemoveAt(i);

            UE_LOG(LogTemp, Log, TEXT("[VideoPool] Stopped sequential queue %d"), QueueID);
            return;
        }
    }
}


void UGiftVideoPool::StopAllSequentialPlays()
{
    UWorld* World = GetWorld();

    for (FSequentialPlayQueue& Queue : SequentialQueues)
    {
        // 清理定时器
        if (World)
        {
            if (Queue.NoVideoTimerHandle.IsValid())
            {
                World->GetTimerManager().ClearTimer(Queue.NoVideoTimerHandle);
            }
            if (Queue.HaSaGiTimerHandle.IsValid())
            {
                World->GetTimerManager().ClearTimer(Queue.HaSaGiTimerHandle);
            }
            // [Claude] 清理间隔定时器
            if (Queue.IntervalTimerHandle.IsValid())
            {
                World->GetTimerManager().ClearTimer(Queue.IntervalTimerHandle);
            }
        }

        if (Queue.CurrentSlotIndex != INDEX_NONE && VideoSlots.IsValidIndex(Queue.CurrentSlotIndex))
        {
            FSlot& Slot = VideoSlots[Queue.CurrentSlotIndex];
            if (Slot.Player)
            {
                Slot.Player->Close();
            }
        }
    }

    SequentialQueues.Empty();
    SlotToQueueMap.Empty();

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Stopped all sequential queues"));
}

int32 UGiftVideoPool::GetSequentialPlayProgress(int32 QueueID)
{
    if (FSequentialPlayQueue* Queue = FindQueue(QueueID))
    {
        return Queue->CurrentIndex;
    }
    return INDEX_NONE;
}

FSequentialPlayQueue* UGiftVideoPool::FindQueue(int32 QueueID)
{
    for (FSequentialPlayQueue& Queue : SequentialQueues)
    {
        if (Queue.QueueID == QueueID)
        {
            return &Queue;
        }
    }
    return nullptr;
}

void UGiftVideoPool::PlayNextInQueue(int32 QueueID)
{
    FSequentialPlayQueue* Queue = FindQueue(QueueID);
    if (!Queue || !Queue->bIsPlaying)
    {
        return;
    }

    // 清理之前可能存在的定时器
    UWorld* World = GetWorld();
    if (World)
    {
        if (Queue->NoVideoTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(Queue->NoVideoTimerHandle);
        }
        if (Queue->IntervalTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(Queue->IntervalTimerHandle);
        }
    }

    // 检查是否已播放完所有视频
    if (Queue->CurrentIndex >= Queue->Items.Num())
    {
        UE_LOG(LogTemp, Log, TEXT("[VideoPool] Sequential queue %d completed"), QueueID);
        Queue->bIsPlaying = false;

        // 广播完成委托
        OnSequentialPlayCompleted.Broadcast(QueueID);

        // 从列表中移除队列
        for (int32 i = SequentialQueues.Num() - 1; i >= 0; --i)
        {
            if (SequentialQueues[i].QueueID == QueueID)
            {
                SequentialQueues.RemoveAt(i);
                break;
            }
        }
        return;
    }

    const FSequentialPlayItem& Item = Queue->Items[Queue->CurrentIndex];

    // ========== 处理无视频的情况 ==========
    if (!Item.MediaSource)
    {
        UE_LOG(LogTemp, Log, TEXT("[VideoPool] Sequential queue %d item %d has no MediaSource, triggering logic only"),
            QueueID, Queue->CurrentIndex);

        // 触发逻辑调用
        for (int32 j = 0; j < Item.LogicCallCount; ++j)
        {
            ScheduleLogicCall(Item.GiftEnumValue, j + 1, 0.0f, Item.LogicDelayTime);
        }

        int32 CompletedIndex = Queue->CurrentIndex;

        // 广播单个项目完成（SlotIndex 为 INDEX_NONE 表示无视频）
        OnSequentialItemCompleted.Broadcast(QueueID, CompletedIndex, INDEX_NONE);

        // 移动到下一个
        Queue->CurrentIndex++;

        // 使用定时器延迟播放下一个
        float DelayTime = Item.NoVideoDuration;
        if (DelayTime <= 0.0f)
        {
            DelayTime = 0.1f;
        }

        if (World)
        {
            World->GetTimerManager().SetTimer(
                Queue->NoVideoTimerHandle,
                FTimerDelegate::CreateUObject(this, &UGiftVideoPool::PlayNextInQueue, QueueID),
                DelayTime,
                false
            );
        }

        return;
    }

    // ========== 正常有视频的情况 ==========
    // 分配槽位
    FSlot* Slot = FindFreeSlot();
    if (!Slot)
    {
        Slot = CreateNewSlot(Queue->OwnerActor);
    }

    Slot->bBusy = true;
    Queue->CurrentSlotIndex = Slot->PoolIndex;

    // [Claude] 只有在等待模式下才建立槽位到队列的映射
    if (Item.bWaitForVideoEnd)
    {
        SlotToQueueMap.Add(Slot->PoolIndex, QueueID);
    }

    // 设置媒体
    Slot->Texture->SetMediaPlayer(Slot->Player);
    Slot->Texture->UpdateResource();

    if (!Slot->SoundComp)
    {
        Slot->SoundComp = NewObject<UMediaSoundComponent>(Queue->OwnerActor);
        Slot->SoundComp->RegisterComponent();
        Slot->SoundComp->SetMediaPlayer(Slot->Player);
        Slot->SoundComp->bIsUISound = true;
    }
    Slot->SoundComp->SetActive(false);
    Slot->SoundComp->SetVolumeMultiplier(Queue->SoundValue);

    // 绑定材质
    UChormaProcessor* Chroma = GetGameInstance()->GetSubsystem<UChormaProcessor>();
    if (Chroma && Slot->VideoImage && Slot->Texture)
    {
        Chroma->SetVideoToImage(Slot->VideoImage, Slot->Texture);
    }

    // 应用UI配置
    if (Item.bUseCustomUIConfig)
    {
        ApplyUIConfig(Slot->PoolIndex, Item.UIConfig);
    }

    // 如果是哈撒给，需要额外添加计时器和代理来通知重力更新
    if (Item.GiftEnumValue == 27)
    {
        float DelayTime = 1;
        if (World)
        {
            World->GetTimerManager().SetTimer(
                Queue->HaSaGiTimerHandle,
                FTimerDelegate::CreateUObject(this, &UGiftVideoPool::OnHaSaGiPlayEnd),
                DelayTime,
                false
            );
        }
    }

    // 触发逻辑调用
    for (int32 j = 0; j < Item.LogicCallCount; ++j)
    {
        ScheduleLogicCall(Item.GiftEnumValue, j + 1, 0.0f, Item.IntervalFromBar);
    }

    // 打开并播放
    FMediaPlayerOptions Options;
    Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Enabled;
    Options.Loop = EMediaPlayerOptionBooleanOverride::Disabled;
    Slot->Player->OpenSourceWithOptions(Item.MediaSource, Options);

    UE_LOG(LogTemp, Log, TEXT("[VideoPool] Sequential queue %d: Playing item %d in slot %d (WaitMode=%s)"),
        QueueID, Queue->CurrentIndex, Slot->PoolIndex, Item.bWaitForVideoEnd ? TEXT("true") : TEXT("false"));

    // [Claude] 定时模式：不等待视频结束，按间隔时间触发下一个
    if (!Item.bWaitForVideoEnd)
    {
        int32 CompletedIndex = Queue->CurrentIndex;

        // 广播单个项目完成
        OnSequentialItemCompleted.Broadcast(QueueID, CompletedIndex, Slot->PoolIndex);

        // 移动到下一个
        Queue->CurrentIndex++;

        // 设置间隔定时器
        float IntervalDelay = Item.IntervalFromBar;
        if (IntervalDelay <= 0.0f)
        {
            IntervalDelay = 0.1f;
        }

        if (World && Queue->CurrentIndex < Queue->Items.Num())
        {
            World->GetTimerManager().SetTimer(
                Queue->IntervalTimerHandle,
                FTimerDelegate::CreateUObject(this, &UGiftVideoPool::PlayNextInQueue, QueueID),
                IntervalDelay,
                false
            );

            UE_LOG(LogTemp, Log, TEXT("[VideoPool] Sequential queue %d: Scheduled next item in %.2fs (TimedMode)"),
                QueueID, IntervalDelay);
        }
        else if (Queue->CurrentIndex >= Queue->Items.Num())
        {
            // [Claude] 定时模式下最后一个，设置延迟完成
            if (World)
            {
                World->GetTimerManager().SetTimer(
                    Queue->IntervalTimerHandle,
                    FTimerDelegate::CreateUObject(this, &UGiftVideoPool::PlayNextInQueue, QueueID),
                    IntervalDelay,
                    false
                );
            }
        }
    }
    // 等待模式：由 HandleSequentialMediaEnd 触发下一个
}

void UGiftVideoPool::HandleSequentialMediaEnd(int32 SlotIndex)
{
    // 检查并移除映射（原子操作）
    int32 QueueID = INDEX_NONE;
    if (int32* QueueIDPtr = SlotToQueueMap.Find(SlotIndex))
    {
        QueueID = *QueueIDPtr;
        SlotToQueueMap.Remove(SlotIndex);
    }
    else
    {
        // [Claude] 映射不存在，可能是定时模式的视频结束，无需处理队列逻辑
        UE_LOG(LogTemp, Log, TEXT("[VideoPool] HandleSequentialMediaEnd: SlotIndex=%d not in queue map (TimedMode or already processed)"), SlotIndex);
        return;
    }

    FSequentialPlayQueue* Queue = FindQueue(QueueID);
    if (!Queue)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VideoPool] HandleSequentialMediaEnd: Queue %d not found"), QueueID);
        return;
    }

    int32 CompletedIndex = Queue->CurrentIndex;

    // 广播单个视频完成
    OnSequentialItemCompleted.Broadcast(QueueID, CompletedIndex, SlotIndex);

    // 移动到下一个
    Queue->CurrentIndex++;
    Queue->CurrentSlotIndex = INDEX_NONE;

    // [Claude] 获取刚完成的项的间隔时间
    float IntervalDelay = 0.0f;
    if (Queue->Items.IsValidIndex(CompletedIndex))
    {
        IntervalDelay = Queue->Items[CompletedIndex].IntervalFromBar;
    }

    // [Claude] 如果有间隔时间，使用定时器延迟播放下一个
    UWorld* World = GetWorld();
    if (IntervalDelay > 0.0f && World)
    {
        World->GetTimerManager().SetTimer(
            Queue->NoVideoTimerHandle,
            FTimerDelegate::CreateUObject(this, &UGiftVideoPool::PlayNextInQueue, QueueID),
            IntervalDelay,
            false
        );
        UE_LOG(LogTemp, Log, TEXT("[VideoPool] Sequential queue %d: Delaying next item by %.2fs (WaitMode)"), QueueID, IntervalDelay);
    }
    else
    {
        // 无间隔，立即播放下一个
        PlayNextInQueue(QueueID);
    }
}


void UGiftVideoPool::OnHaSaGiPlayEnd() {
    OnHaSaGiPlayCompleted.Broadcast();
}



/*===================================================================================*/
/*--------------------------------UChormaProcessor-----------------------------------*/
/*===================================================================================*/

void UChormaProcessor::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ChromaKeyMat = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/PAP/UI/Video/ChormaMaterial.ChormaMaterial"));
}

void UChormaProcessor::Deinitialize()
{
    ChromaKeyMat = nullptr;
    Super::Deinitialize();
}

void UChormaProcessor::SetVideoToImage(UImage* TargetImage, UMediaTexture* MediaTex)
{
    if (!TargetImage || !MediaTex || !ChromaKeyMat) return;

    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(
        TargetImage->GetBrush().GetResourceObject());
    if (!MID)
    {
        MID = UMaterialInstanceDynamic::Create(ChromaKeyMat, this);
        TargetImage->SetBrushFromMaterial(MID);
    }

    MID->SetTextureParameterValue("VideoTexture", MediaTex);

    MediaTex->UpdateResource();

    TargetImage->ForceLayoutPrepass();
}



/*===================================================================================*/
/*---------------------------------UMediaEndProxy------------------------------------*/
/*===================================================================================*/

void UMediaEndProxy::InitProxy(int32 InPlayerID, UGiftVideoPool* InOwnerPool)
{
    PlayerID = InPlayerID;
    OwnerPool = InOwnerPool;
    // [Claude] 初始化时重置状态
    LastEndTime = 0.0;
}


void UMediaEndProxy::OnProxyMediaOpened(FString OpenedUrl)
{
    // [Claude] 媒体打开时重置结束状态，为新的播放周期做准备
    LastEndTime = 0.0;

    if (UGiftVideoPool* Pool = OwnerPool.Get())
    {
        Pool->HandleMediaOpened(PlayerID);
    }
}


void UMediaEndProxy::OnProxyMediaEnd()
{
    // [Claude] 防重入：检查是否是短时间内的重复调用
    double CurrentTime = FPlatformTime::Seconds();
    if (LastEndTime > 0.0 && (CurrentTime - LastEndTime) < END_DEBOUNCE_THRESHOLD)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MediaEndProxy] PlayerID=%d: Duplicate OnEndReached ignored (interval=%.3fs)"),
            PlayerID, CurrentTime - LastEndTime);
        return;
    }
    LastEndTime = CurrentTime;
    // [Claude] 防重入逻辑结束

    if (UGiftVideoPool* Pool = OwnerPool.Get())
    {
        Pool->HandleMediaEnd(PlayerID);
    }
}


// [Claude] 重置防重入状态
void UMediaEndProxy::ResetEndState()
{
    LastEndTime = 0.0;
}




UGiftVideoPool* UVideoSubsystemLibrary::GetGiftVideoPool(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return nullptr;

    return GI->GetSubsystem<UGiftVideoPool>();
}

UChormaProcessor* UVideoSubsystemLibrary::GetChormaProcessor(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return nullptr;

    return GI->GetSubsystem<UChormaProcessor>();
}


void UVideoSubsystemLibrary::SetTabNavigationEnabled(bool bEnabled)
{
    if (bEnabled)
    {
        // 启用默认导航（包含Tab键）
        FSlateApplication::Get().SetNavigationConfig(MakeShared<FNavigationConfig>());
    }
    else
    {
        // 禁用Tab键导航
        TSharedRef<FNavigationConfig> NavigationConfig = MakeShared<FNavigationConfig>();
        NavigationConfig->KeyEventRules.Emplace(EKeys::Tab, EUINavigation::Invalid);
        //NavigationConfig->KeyEventRules.Emplace(EKeys::Tab, EUINavigation::Invalid, EModifierKey::Shift); // 禁用Shift+Tab
        FSlateApplication::Get().SetNavigationConfig(NavigationConfig);
    }
}
















// ========== [Claude] Box/Special Trigger 顺序播放系统实现 ==========

int32 UGiftVideoPool::PlayBoxTriggerSequence(
    const TArray<uint8>& GiftEnums,
    const TArray<float>& LogicStartOffsets,
    float Interval,
    int32 RandomTimes,
    const FString& GiftName,
    float SoundValue,
    AActor* Actor)
{
    if (GiftEnums.Num() == 0 || RandomTimes <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Claude][BoxTrigger] Invalid parameters: GiftEnums=%d, RandomTimes=%d"),
            GiftEnums.Num(), RandomTimes);
        return INDEX_NONE;
    }

    // 查找视频源
    UFileMediaSource* MediaSource = FindMediaSourceByName(GiftName);

    // 构建顺序播放项列表
    // Box逻辑：循环 RandomTimes 次，每次遍历 GiftEnums
    TArray<FSequentialPlayItem> Items;

    for (int32 i = 0; i < RandomTimes; ++i)
    {
        for (int32 j = 0; j < GiftEnums.Num(); ++j)
        {
            FSequentialPlayItem Item;
            Item.MediaSource = MediaSource;  // 可能为 nullptr，顺序播放系统会处理
            Item.GiftEnumValue = GiftEnums[j];
            Item.LogicCallCount = 1;
            Item.LogicDelayTime = (LogicStartOffsets.IsValidIndex(j)) ? LogicStartOffsets[j] : 0.0f;
            Item.NoVideoDuration = Interval;  // 无视频时的等待时间
            Item.bUseCustomUIConfig = false;

            Items.Add(Item);
        }

        
    }

    // 启动顺序播放
    int32 QueueID = StartSequentialPlay(Items, SoundValue, Actor);

    if (QueueID != INDEX_NONE)
    {
        // 保存上下文
        FTriggerQueueContext Context;
        Context.TriggerType = ETriggerType::Box;
        Context.GiftEnums = GiftEnums;
        Context.LogicStartOffsets = LogicStartOffsets;
        Context.Interval = Interval;
        Context.GiftName = GiftName;
        TriggerQueueContextMap.Add(QueueID, Context);

        // 绑定完成委托（只绑定一次，使用 AddUniqueDynamic 避免重复）
        OnSequentialItemCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleTriggerItemCompleted);

        UE_LOG(LogTemp, Log, TEXT("[Claude][BoxTrigger] Started queue %d with %d items (RandomTimes=%d, GiftEnums=%d)"),
            QueueID, Items.Num(), RandomTimes, GiftEnums.Num());
    }

    return QueueID;
}


int32 UGiftVideoPool::PlaySpecialTriggerSequence(
    const TArray<uint8>& GiftEnums,
    const TArray<float>& LogicStartOffsets,
    float Interval,
    int32 TriggerTime,
    const FString& GiftName,
    float SoundValue,
    AActor* Actor)
{
    if (GiftEnums.Num() == 0 || TriggerTime <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Claude][SpecialTrigger] Invalid parameters: GiftEnums=%d, TriggerTime=%d"),
            GiftEnums.Num(), TriggerTime);
        return INDEX_NONE;
    }

    // 查找视频源
    UFileMediaSource* MediaSource = FindMediaSourceByName(GiftName);

    // 构建顺序播放项列表
    // Special逻辑：循环 TriggerTime 次，每次遍历所有 GiftEnums
    TArray<FSequentialPlayItem> Items;

    for (int32 i = 0; i < TriggerTime; ++i)
    {
        for (int32 j = 0; j < GiftEnums.Num(); ++j)
        {
            FSequentialPlayItem Item;
            Item.MediaSource = MediaSource;
            Item.GiftEnumValue = GiftEnums[j];
            Item.LogicCallCount = 1;
            Item.LogicDelayTime = (LogicStartOffsets.IsValidIndex(j)) ? LogicStartOffsets[j] : 0.0f;
            Item.NoVideoDuration = Interval;
            Item.bUseCustomUIConfig = false;

            Items.Add(Item);
        }
    }

    // 启动顺序播放
    int32 QueueID = StartSequentialPlay(Items, SoundValue, Actor);

    if (QueueID != INDEX_NONE)
    {
        // 保存上下文
        FTriggerQueueContext Context;
        Context.TriggerType = ETriggerType::Special;
        Context.GiftEnums = GiftEnums;
        Context.LogicStartOffsets = LogicStartOffsets;
        Context.Interval = Interval;
        Context.GiftName = GiftName;
        TriggerQueueContextMap.Add(QueueID, Context);

        // 绑定完成委托
        OnSequentialItemCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleTriggerItemCompleted);

        UE_LOG(LogTemp, Log, TEXT("[Claude][SpecialTrigger] Started queue %d with %d items (TriggerTime=%d, GiftEnums=%d)"),
            QueueID, Items.Num(), TriggerTime, GiftEnums.Num());
    }

    return QueueID;
}


void UGiftVideoPool::HandleTriggerItemCompleted(int32 QueueID, int32 ItemIndex, int32 SlotIndex)
{
    // 检查是否是我们管理的队列
    FTriggerQueueContext* Context = TriggerQueueContextMap.Find(QueueID);
    if (!Context)
    {
        // 不是 Box/Special 触发的队列，忽略
        return;
    }

    // 查找队列获取当前项信息
    FSequentialPlayQueue* Queue = FindQueue(QueueID);

    // 计算当前是第几轮、第几个礼物
    int32 GiftCount = Context->GiftEnums.Num();
    int32 RoundIndex = (GiftCount > 0) ? (ItemIndex / GiftCount) : 0;
    int32 GiftIndex = (GiftCount > 0) ? (ItemIndex % GiftCount) : 0;

    uint8 CurrentGiftEnum = Context->GiftEnums.IsValidIndex(GiftIndex) ? Context->GiftEnums[GiftIndex] : 0;
    float CurrentLogicOffset = Context->LogicStartOffsets.IsValidIndex(GiftIndex) ? Context->LogicStartOffsets[GiftIndex] : 0.0f;

    UE_LOG(LogTemp, Log, TEXT("[Claude][Trigger] Queue %d Item %d completed (Type=%s, GiftEnum=%d, Offset=%.2f)"),
        QueueID, ItemIndex,
        Context->TriggerType == ETriggerType::Box ? TEXT("Box") : TEXT("Special"),
        CurrentGiftEnum, CurrentLogicOffset);

    // 根据类型广播不同的委托
    if (Context->TriggerType == ETriggerType::Box)
    {
        // 广播 Box 逻辑触发委托
        // 参数: GiftEnum, ActivateTime(第几次激活，从1开始), LogicStartTimeOffset, VideoTimeInterval
        OnBoxGiftLogicTrigger.Broadcast(
            CurrentGiftEnum,
            ItemIndex + 1,  // ActivateTime 从 1 开始
            CurrentLogicOffset,
            Context->Interval
        );
    }
    else // ETriggerType::Special
    {
        // 广播 Special 逻辑触发委托
        OnSpecialGiftLogicTrigger.Broadcast(
            CurrentGiftEnum,
            ItemIndex + 1,
            CurrentLogicOffset,
            Context->Interval
        );
    }

    // 检查队列是否已完成（如果 Queue 为空说明已被清理）
    if (!Queue || !Queue->bIsPlaying)
    {
        // 队列已完成，清理上下文
        TriggerQueueContextMap.Remove(QueueID);
        UE_LOG(LogTemp, Log, TEXT("[Claude][Trigger] Queue %d completed, context removed"), QueueID);
    }
}


UFileMediaSource* UGiftVideoPool::FindMediaSourceByName(const FString& VideoName)
{
    if (VideoName.IsEmpty())
    {
        return nullptr;
    }

    // 构建资产路径
    FString AssetPath = FString::Printf(TEXT("/Game/AutoMedia/FileMediaSource/%s.%s"), *VideoName, *VideoName);

    UFileMediaSource* MediaSource = LoadObject<UFileMediaSource>(nullptr, *AssetPath);

    if (!MediaSource)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Claude][Trigger] Failed to find MediaSource: %s"), *AssetPath);
    }

    return MediaSource;
}


// ========== [Claude] 单次礼物队列系统实现（按礼物类型分队列） ==========

void UGiftVideoPool::EnqueueSingleGift(
    const FSingleGiftRequest& Request,
    int32 PlayCount,
    float Interval,
    float SoundValue,
    AActor* Actor)
{
    if (PlayCount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Claude][SingleGift] Invalid PlayCount: %d"), PlayCount);
        return;
    }

    const uint8 GiftType = Request.GiftEnumValue;

    // [Claude] 构建播放项列表
    TArray<FSequentialPlayItem> NewItems;
    for (int32 i = 0; i < PlayCount; ++i)
    {
        FSequentialPlayItem Item;
        Item.MediaSource = Request.MediaSource;
        Item.GiftEnumValue = GiftType;
        Item.UIConfig = Request.UIConfig;
        Item.bUseCustomUIConfig = Request.bUseCustomUIConfig;
        Item.LogicCallCount = Request.LogicCallCount;
        Item.LogicDelayTime = Request.LogicDelayTime;
        Item.NoVideoDuration = Interval;
        Item.IntervalFromBar = Interval;

        NewItems.Add(Item);
    }

    // [Claude] 检查该礼物类型是否已有活跃队列
    int32* ExistingQueueIDPtr = GiftTypeToQueueMap.Find(GiftType);

    if (ExistingQueueIDPtr)
    {
        // [Claude] 该礼物类型已有队列，尝试追加
        FSequentialPlayQueue* ExistingQueue = FindQueue(*ExistingQueueIDPtr);

        if (ExistingQueue && ExistingQueue->bIsPlaying)
        {
            // [Claude] 队列正在播放，追加到末尾
            ExistingQueue->Items.Append(NewItems);

            UE_LOG(LogTemp, Log, TEXT("[Claude][SingleGift] GiftType=%d: Appended %d items to queue %d (total: %d)"),
                GiftType, NewItems.Num(), *ExistingQueueIDPtr, ExistingQueue->Items.Num());
            return;
        }
        else
        {
            // [Claude] 队列已结束但映射还在，清理旧映射
            QueueToGiftTypeMap.Remove(*ExistingQueueIDPtr);
            SingleGiftQueueConfigs.Remove(*ExistingQueueIDPtr);
            GiftTypeToQueueMap.Remove(GiftType);
        }
    }

    // [Claude] 创建新队列
    int32 NewQueueID = StartSequentialPlay(NewItems, SoundValue, Actor);

    if (NewQueueID != INDEX_NONE)
    {
        // [Claude] 建立双向映射
        GiftTypeToQueueMap.Add(GiftType, NewQueueID);
        QueueToGiftTypeMap.Add(NewQueueID, GiftType);

        // [Claude] 保存配置
        FSingleGiftQueueConfig Config;
        Config.Interval = Interval;
        Config.SoundValue = SoundValue;
        Config.OwnerActor = Actor;
        SingleGiftQueueConfigs.Add(NewQueueID, Config);

        // [Claude] 绑定委托（使用 AddUniqueDynamic 避免重复绑定）
        OnSequentialPlayCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleSingleGiftQueueCompleted);
        OnSequentialItemCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleSingleGiftItemCompleted);

        UE_LOG(LogTemp, Log, TEXT("[Claude][SingleGift] GiftType=%d: Created new queue %d with %d items"),
            GiftType, NewQueueID, NewItems.Num());
    }
}


void UGiftVideoPool::ClearSingleGiftQueue(uint8 GiftEnumValue)
{
    int32* QueueIDPtr = GiftTypeToQueueMap.Find(GiftEnumValue);
    if (QueueIDPtr)
    {
        int32 QueueID = *QueueIDPtr;

        // [Claude] 停止队列
        StopSequentialPlay(QueueID);

        // [Claude] 清理映射
        QueueToGiftTypeMap.Remove(QueueID);
        SingleGiftQueueConfigs.Remove(QueueID);
        GiftTypeToQueueMap.Remove(GiftEnumValue);

        UE_LOG(LogTemp, Log, TEXT("[Claude][SingleGift] GiftType=%d: Queue %d cleared"), GiftEnumValue, QueueID);
    }
}


void UGiftVideoPool::ClearAllSingleGiftQueues()
{
    // [Claude] 收集所有队列ID（避免在遍历时修改Map）
    TArray<int32> QueueIDs;
    GiftTypeToQueueMap.GenerateValueArray(QueueIDs);

    for (int32 QueueID : QueueIDs)
    {
        StopSequentialPlay(QueueID);
    }

    // [Claude] 清空所有映射
    GiftTypeToQueueMap.Empty();
    QueueToGiftTypeMap.Empty();
    SingleGiftQueueConfigs.Empty();

    UE_LOG(LogTemp, Log, TEXT("[Claude][SingleGift] All queues cleared"));
}


int32 UGiftVideoPool::GetSingleGiftQueueLength(uint8 GiftEnumValue) const
{
    const int32* QueueIDPtr = GiftTypeToQueueMap.Find(GiftEnumValue);
    if (!QueueIDPtr)
    {
        return 0;
    }

    // [Claude] 需要非const访问 FindQueue
    UGiftVideoPool* MutableThis = const_cast<UGiftVideoPool*>(this);
    FSequentialPlayQueue* Queue = MutableThis->FindQueue(*QueueIDPtr);

    if (Queue && Queue->bIsPlaying)
    {
        return Queue->Items.Num() - Queue->CurrentIndex;
    }

    return 0;
}


int32 UGiftVideoPool::GetActiveSingleGiftQueueCount() const
{
    return GiftTypeToQueueMap.Num();
}


void UGiftVideoPool::HandleSingleGiftQueueCompleted(int32 QueueID)
{
    // [Claude] 检查是否是单次礼物队列
    uint8* GiftTypePtr = QueueToGiftTypeMap.Find(QueueID);
    if (!GiftTypePtr)
    {
        // [Claude] 不是单次礼物队列，忽略
        return;
    }

    uint8 GiftType = *GiftTypePtr;

    UE_LOG(LogTemp, Log, TEXT("[Claude][SingleGift] GiftType=%d: Queue %d completed"), GiftType, QueueID);

    // [Claude] 清理映射
    GiftTypeToQueueMap.Remove(GiftType);
    QueueToGiftTypeMap.Remove(QueueID);
    SingleGiftQueueConfigs.Remove(QueueID);

    // [Claude] 广播完成委托，传递礼物类型
    OnSingleGiftQueueCompleted.Broadcast(GiftType);
}


void UGiftVideoPool::HandleSingleGiftItemCompleted(int32 QueueID, int32 ItemIndex, int32 SlotIndex)
{
    // [Claude] 检查是否是单次礼物队列
    uint8* GiftTypePtr = QueueToGiftTypeMap.Find(QueueID);
    if (!GiftTypePtr)
    {
        // [Claude] 不是单次礼物队列，忽略
        return;
    }

    uint8 GiftType = *GiftTypePtr;

    UE_LOG(LogTemp, Log, TEXT("[Claude][SingleGift] GiftType=%d: Item %d completed in slot %d"),
        GiftType, ItemIndex, SlotIndex);

    // [Claude] 广播单项完成委托，传递礼物类型
    OnSingleGiftItemCompleted.Broadcast(GiftType, ItemIndex, SlotIndex);
}




// ========== [Claude] Box/Special Trigger 排队系统实现 ==========

TArray<FSequentialPlayItem> UGiftVideoPool::BuildTriggerItems(
    const TArray<uint8>& GiftEnums,
    const TArray<float>& LogicStartOffsets,
    float Interval,
    int32 LoopCount,
    const FString& AnimName)
{
    TArray<FSequentialPlayItem> Items;

    // [Claude] 查找视频源
    UFileMediaSource* MediaSource = FindMediaSourceByName(AnimName);

    // [Claude] 构建播放项：循环 LoopCount 次，每次遍历 GiftEnums
    for (int32 i = 0; i < LoopCount; ++i)
    {
        for (int32 j = 0; j < GiftEnums.Num(); ++j)
        {
            FSequentialPlayItem Item;
            Item.MediaSource = MediaSource;
            Item.GiftEnumValue = GiftEnums[j];
            Item.LogicCallCount = 1;
            Item.LogicDelayTime = (LogicStartOffsets.IsValidIndex(j)) ? LogicStartOffsets[j] : 0.0f;
            Item.NoVideoDuration = Interval;
            Item.bUseCustomUIConfig = false;
            Item.IntervalFromBar = Interval;
            Item.bWaitForVideoEnd = false;

            Items.Add(Item);
        }
    }

    return Items;
}


void UGiftVideoPool::EnqueueBoxTrigger(
    const TArray<uint8>& GiftEnums,
    const TArray<float>& LogicStartOffsets,
    float Interval,
    int32 RandomTimes,
    const FString& GiftName,
    float SoundValue,
    AActor* Actor,
    int32 Multiplier)
{
    if (GiftEnums.Num() == 0 || RandomTimes <= 0 || Multiplier <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Claude][BoxTrigger] Invalid parameters: GiftEnums=%d, RandomTimes=%d, Multiplier=%d"),
            GiftEnums.Num(), RandomTimes, Multiplier);
        return;
    }

    // [Claude] 构建播放项列表（倍率 × 原始循环次数）
    TArray<FSequentialPlayItem> NewItems = BuildTriggerItems(
        GiftEnums,
        LogicStartOffsets,
        Interval,
        RandomTimes * Multiplier,  // 倍率影响循环次数
        GiftName
    );

    // [Claude] 检查该类型是否已有活跃队列
    int32* ExistingQueueIDPtr = BoxTriggerToQueueMap.Find(GiftName);

    if (ExistingQueueIDPtr)
    {
        // [Claude] 已有队列，尝试追加
        FSequentialPlayQueue* ExistingQueue = FindQueue(*ExistingQueueIDPtr);

        if (ExistingQueue && ExistingQueue->bIsPlaying)
        {
            // [Claude] 队列正在播放，追加到末尾
            ExistingQueue->Items.Append(NewItems);

            UE_LOG(LogTemp, Log, TEXT("[Claude][BoxTrigger] AnimName=%s: Appended %d items to queue %d (total: %d)"),
                *GiftName, NewItems.Num(), *ExistingQueueIDPtr, ExistingQueue->Items.Num());
            return;
        }
        else
        {
            // [Claude] 队列已结束但映射还在，清理旧映射
            QueueToBoxTriggerMap.Remove(*ExistingQueueIDPtr);
            BoxTriggerToQueueMap.Remove(GiftName);
            // [Claude] 清理旧的上下文
            TriggerQueueContextMap.Remove(*ExistingQueueIDPtr);
        }
    }

    // [Claude] 创建新队列
    int32 NewQueueID = StartSequentialPlay(NewItems, SoundValue, Actor);

    if (NewQueueID != INDEX_NONE)
    {
        // [Claude] 建立双向映射
        BoxTriggerToQueueMap.Add(GiftName, NewQueueID);
        QueueToBoxTriggerMap.Add(NewQueueID, GiftName);

        // [Claude] 保存上下文（用于触发逻辑）
        FTriggerQueueContext Context;
        Context.TriggerType = ETriggerType::Box;
        Context.GiftEnums = GiftEnums;
        Context.LogicStartOffsets = LogicStartOffsets;
        Context.Interval = Interval;
        Context.GiftName = GiftName;
        TriggerQueueContextMap.Add(NewQueueID, Context);

        // [Claude] 绑定完成委托
        OnSequentialPlayCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleBoxTriggerQueueCompleted);
        OnSequentialItemCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleTriggerItemCompleted);

        UE_LOG(LogTemp, Log, TEXT("[Claude][BoxTrigger] AnimName=%s: Created new queue %d with %d items (Multiplier=%d)"),
            *GiftName, NewQueueID, NewItems.Num(), Multiplier);
    }
}


void UGiftVideoPool::EnqueueSpecialTrigger(
    const TArray<uint8>& GiftEnums,
    const TArray<float>& LogicStartOffsets,
    float Interval,
    int32 TriggerTime,
    const FString& GiftName,
    float SoundValue,
    AActor* Actor,
    int32 Multiplier)
{
    if (GiftEnums.Num() == 0 || TriggerTime <= 0 || Multiplier <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Claude][SpecialTrigger] Invalid parameters: GiftEnums=%d, TriggerTime=%d, Multiplier=%d"),
            GiftEnums.Num(), TriggerTime, Multiplier);
        return;
    }

    // [Claude] 构建播放项列表（倍率 × 原始触发次数）
    TArray<FSequentialPlayItem> NewItems = BuildTriggerItems(
        GiftEnums,
        LogicStartOffsets,
        Interval,
        TriggerTime * Multiplier,  // 倍率影响触发次数
        GiftName
    );

    // [Claude] 检查该类型是否已有活跃队列
    int32* ExistingQueueIDPtr = SpecialTriggerToQueueMap.Find(GiftName);

    if (ExistingQueueIDPtr)
    {
        // [Claude] 已有队列，尝试追加
        FSequentialPlayQueue* ExistingQueue = FindQueue(*ExistingQueueIDPtr);

        if (ExistingQueue && ExistingQueue->bIsPlaying)
        {
            // [Claude] 队列正在播放，追加到末尾
            ExistingQueue->Items.Append(NewItems);

            UE_LOG(LogTemp, Log, TEXT("[Claude][SpecialTrigger] AnimName=%s: Appended %d items to queue %d (total: %d)"),
                *GiftName, NewItems.Num(), *ExistingQueueIDPtr, ExistingQueue->Items.Num());
            return;
        }
        else
        {
            // [Claude] 队列已结束但映射还在，清理旧映射
            QueueToSpecialTriggerMap.Remove(*ExistingQueueIDPtr);
            SpecialTriggerToQueueMap.Remove(GiftName);
            // [Claude] 清理旧的上下文
            TriggerQueueContextMap.Remove(*ExistingQueueIDPtr);
        }
    }

    // [Claude] 创建新队列
    int32 NewQueueID = StartSequentialPlay(NewItems, SoundValue, Actor);

    if (NewQueueID != INDEX_NONE)
    {
        // [Claude] 建立双向映射
        SpecialTriggerToQueueMap.Add(GiftName, NewQueueID);
        QueueToSpecialTriggerMap.Add(NewQueueID, GiftName);

        // [Claude] 保存上下文（用于触发逻辑）
        FTriggerQueueContext Context;
        Context.TriggerType = ETriggerType::Special;
        Context.GiftEnums = GiftEnums;
        Context.LogicStartOffsets = LogicStartOffsets;
        Context.Interval = Interval;
        Context.GiftName = GiftName;
        TriggerQueueContextMap.Add(NewQueueID, Context);

        // [Claude] 绑定完成委托
        OnSequentialPlayCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleSpecialTriggerQueueCompleted);
        OnSequentialItemCompleted.AddUniqueDynamic(this, &UGiftVideoPool::HandleTriggerItemCompleted);

        UE_LOG(LogTemp, Log, TEXT("[Claude][SpecialTrigger] AnimName=%s: Created new queue %d with %d items (Multiplier=%d)"),
            *GiftName, NewQueueID, NewItems.Num(), Multiplier);
    }
}


void UGiftVideoPool::ClearBoxTriggerQueue(const FString& AnimName)
{
    int32* QueueIDPtr = BoxTriggerToQueueMap.Find(AnimName);
    if (QueueIDPtr)
    {
        int32 QueueID = *QueueIDPtr;

        // [Claude] 停止队列
        StopSequentialPlay(QueueID);

        // [Claude] 清理映射
        QueueToBoxTriggerMap.Remove(QueueID);
        BoxTriggerToQueueMap.Remove(AnimName);
        TriggerQueueContextMap.Remove(QueueID);

        UE_LOG(LogTemp, Log, TEXT("[Claude][BoxTrigger] AnimName=%s: Queue %d cleared"), *AnimName, QueueID);
    }
}


void UGiftVideoPool::ClearSpecialTriggerQueue(const FString& AnimName)
{
    int32* QueueIDPtr = SpecialTriggerToQueueMap.Find(AnimName);
    if (QueueIDPtr)
    {
        int32 QueueID = *QueueIDPtr;

        // [Claude] 停止队列
        StopSequentialPlay(QueueID);

        // [Claude] 清理映射
        QueueToSpecialTriggerMap.Remove(QueueID);
        SpecialTriggerToQueueMap.Remove(AnimName);
        TriggerQueueContextMap.Remove(QueueID);

        UE_LOG(LogTemp, Log, TEXT("[Claude][SpecialTrigger] AnimName=%s: Queue %d cleared"), *AnimName, QueueID);
    }
}


void UGiftVideoPool::ClearAllTriggerQueues()
{
    // [Claude] 清理所有 Box 触发队列
    TArray<int32> BoxQueueIDs;
    BoxTriggerToQueueMap.GenerateValueArray(BoxQueueIDs);
    for (int32 QueueID : BoxQueueIDs)
    {
        StopSequentialPlay(QueueID);
    }
    BoxTriggerToQueueMap.Empty();
    QueueToBoxTriggerMap.Empty();

    // [Claude] 清理所有 Special 触发队列
    TArray<int32> SpecialQueueIDs;
    SpecialTriggerToQueueMap.GenerateValueArray(SpecialQueueIDs);
    for (int32 QueueID : SpecialQueueIDs)
    {
        StopSequentialPlay(QueueID);
    }
    SpecialTriggerToQueueMap.Empty();
    QueueToSpecialTriggerMap.Empty();

    // [Claude] 清理所有上下文（包括原有的非排队版本的上下文）
    TriggerQueueContextMap.Empty();

    UE_LOG(LogTemp, Log, TEXT("[Claude][Trigger] All trigger queues cleared"));
}


void UGiftVideoPool::HandleBoxTriggerQueueCompleted(int32 QueueID)
{
    // [Claude] 检查是否是 Box 触发队列
    FString* AnimNamePtr = QueueToBoxTriggerMap.Find(QueueID);
    if (!AnimNamePtr)
    {
        // [Claude] 不是 Box 触发队列，忽略
        return;
    }

    FString AnimName = *AnimNamePtr;

    UE_LOG(LogTemp, Log, TEXT("[Claude][BoxTrigger] AnimName=%s: Queue %d completed"), *AnimName, QueueID);

    // [Claude] 清理映射
    BoxTriggerToQueueMap.Remove(AnimName);
    QueueToBoxTriggerMap.Remove(QueueID);
    TriggerQueueContextMap.Remove(QueueID);
}


void UGiftVideoPool::HandleSpecialTriggerQueueCompleted(int32 QueueID)
{
    // [Claude] 检查是否是 Special 触发队列
    FString* AnimNamePtr = QueueToSpecialTriggerMap.Find(QueueID);
    if (!AnimNamePtr)
    {
        // [Claude] 不是 Special 触发队列，忽略
        return;
    }

    FString AnimName = *AnimNamePtr;

    UE_LOG(LogTemp, Log, TEXT("[Claude][SpecialTrigger] AnimName=%s: Queue %d completed"), *AnimName, QueueID);

    // [Claude] 清理映射
    SpecialTriggerToQueueMap.Remove(AnimName);
    QueueToSpecialTriggerMap.Remove(QueueID);
    TriggerQueueContextMap.Remove(QueueID);
}