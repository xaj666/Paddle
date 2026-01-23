#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Engine/DeveloperSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SoftObjectPath.h"

#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Framework/Application/NavigationConfig.h" 

#include "VideoLoader.generated.h"



class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class UImage;
class UMediaSoundComponent;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVideoEndedSignature, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVideoReadyToShow, int32, SlotIndex, UUserWidget*, VideoUI);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBatchPlayCompleted);
// 逻辑触发委托 - 用于通知蓝图执行 GiftFunc
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGiftLogicTrigger, uint8, GiftEnumValue, int32, ActivateTime, float, DelayTime);

// 顺序播放完成委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSequentialPlayCompleted, int32, QueueID);
// 顺序播放单个视频完成委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSequentialItemCompleted, int32, QueueID, int32, ItemIndex, int32, SlotIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHaSaGiPlayCompleted);


// ========== [Claude] Box/Special Trigger 顺序播放系统 ==========

// Box触发逻辑完成委托 - 蓝图中绑定此委托来调用 GiftFunCall
// 参数: GiftEnum, ActivateTime, LogicStartTimeOffset, VideoTimeInterval
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnBoxGiftLogicTrigger, uint8, GiftEnumValue, int32, ActivateTime, float, LogicStartTimeOffset, float, VideoTimeInterval);

// Special触发逻辑完成委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSpecialGiftLogicTrigger, uint8, GiftEnumValue, int32, ActivateTime, float, LogicStartTimeOffset, float, VideoTimeInterval);

// 单次礼物队列完成委托 - [Claude] 增加 GiftEnumValue 参数用于区分是哪种礼物的队列完成
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSingleGiftQueueCompleted, uint8, GiftEnumValue);
// 单次礼物单项完成委托 - [Claude] 增加 GiftEnumValue 参数
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSingleGiftItemCompleted, uint8, GiftEnumValue, int32, ItemIndex, int32, SlotIndex);

UCLASS()
class UVideoLoader : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    void ScanMoviesFolderAndCreateFMS();
    void LoadSnapshot(FMediaSnapshot& Out);
    void SaveSnapshot(const FMediaSnapshot& In);

    void UpdateVideoDataTable(const TSet<FString>& CurrentRelSet);
    class UDataTable* LoadVideoDataTable();
};

USTRUCT(BlueprintType)
struct FGiftUIDisplayConfig
{
    GENERATED_BODY()

    // 位置偏移
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FVector2D Translation = FVector2D::ZeroVector;

    // 缩放
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FVector2D Scale = FVector2D(1.0f, 1.0f);

    // 旋转角度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    float Rotation = 0.0f;
};

USTRUCT()
struct FMediaSnapshot
{
    GENERATED_BODY()
    UPROPERTY() TArray<FString> RelPaths;
};

USTRUCT()
struct FSlot
{
    GENERATED_BODY()

    UPROPERTY()
    UMediaPlayer* Player = nullptr;
    UPROPERTY()
    UMediaTexture* Texture = nullptr;
    UPROPERTY()
    UMaterial* Material = nullptr;
    UPROPERTY()
    UUserWidget* UI = nullptr;
    UPROPERTY()
    UMediaSoundComponent* SoundComp = nullptr;
    UPROPERTY()
    UImage* VideoImage = nullptr;


    // 当前使用的 UI 配置
    FGiftUIDisplayConfig CurrentUIConfig;
    bool bHasCustomConfig = false;

    bool  bBusy = false;
    int32 PoolIndex = INDEX_NONE;
};

// 批次播放请求结构
USTRUCT(BlueprintType)
struct FBatchPlayRequest
{
    GENERATED_BODY()

    UPROPERTY()
    UFileMediaSource* MediaSource = nullptr;

    UPROPERTY()
    TArray<int32> SlotIndices;  // 预分配的槽位

    UPROPERTY()
    int32 PlayCount = 1;  // 播放次数

    UPROPERTY()
    float Interval = 0.5f;  // 播放间隔（秒）

    UPROPERTY()
    int32 CurrentIndex = 0;  // 当前播放到第几个

    UPROPERTY()
    AActor* OwnerActor = nullptr;

    FTimerHandle IntervalTimerHandle;
};



// 视频播放配置结构体
USTRUCT(BlueprintType)
struct FVideoPlayConfig
{
    GENERATED_BODY()

    // 视频源
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    TArray<UFileMediaSource*> MediaSources;

    // 礼物类型枚举 (对应蓝图中的 Gift_Enum)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    TArray<uint8> GiftEnumValue;

    // 播放次数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    int32 PlayCount = 1;

    // 视频播放间隔（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    float VideoInterval = 0.5f;

    // 逻辑触发相对于视频播放的时间偏移（秒）
    // 正数：逻辑延迟触发；负数：逻辑提前触发
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    float LogicTimeOffset = 0.0f;

    // 每次视频播放时逻辑调用次数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    int32 LogicCallCount = 1;

    // 逻辑调用的附加延迟时间
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    float LogicDelayTime = 0.0f;

    // 新增：UI 显示配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    TArray<FGiftUIDisplayConfig> UIDisplayConfig;

    // 是否使用自定义 UI 配置（false 则使用默认）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    bool bUseCustomUIConfig = false;
};

// 单次逻辑调用请求
USTRUCT()
struct FLogicCallRequest
{
    GENERATED_BODY()

    UPROPERTY()
    uint8 GiftEnumValue = 0;

    UPROPERTY()
    int32 ActivateTime = 1;

    UPROPERTY()
    float DelayTime = 0.0f;

    FTimerHandle TimerHandle;
};

// 顺序播放请求项
USTRUCT(BlueprintType)
struct FSequentialPlayItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UFileMediaSource* MediaSource = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 GiftEnumValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGiftUIDisplayConfig UIConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseCustomUIConfig = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LogicCallCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LogicDelayTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoVideoDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntervalFromBar = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bWaitForVideoEnd = false;
};

// 顺序播放队列
USTRUCT()
struct FSequentialPlayQueue
{
    GENERATED_BODY()

    UPROPERTY()
    int32 QueueID = INDEX_NONE;

    UPROPERTY()
    TArray<FSequentialPlayItem> Items;

    UPROPERTY()
    int32 CurrentIndex = 0;

    UPROPERTY()
    int32 CurrentSlotIndex = INDEX_NONE;

    UPROPERTY()
    float SoundValue = 1.0f;

    UPROPERTY()
    AActor* OwnerActor = nullptr;

    bool bIsPlaying = false;

    FTimerHandle NoVideoTimerHandle;
    FTimerHandle HaSaGiTimerHandle;
    FTimerHandle IntervalTimerHandle;
};


USTRUCT(BlueprintType)
struct FSingleGiftRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UFileMediaSource* MediaSource = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 GiftEnumValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGiftUIDisplayConfig UIConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseCustomUIConfig = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LogicCallCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LogicDelayTime = 0.0f;
};

UCLASS()
class UGiftVideoPool : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnVideoEndedSignature OnVideoEndedinBP;

    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnVideoReadyToShow OnVideoReadyToShow;

    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnBatchPlayCompleted OnBatchPlayCompleted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> VideoUIClass;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 播放单个视频 */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    int32 PlayGiftVideo(UFileMediaSource* MediaSource, AActor* Actor);

    /** 批次播放视频 */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    void PlayGiftVideoBatch(UFileMediaSource* MediaSource, AActor* Actor, int32 PlayCount, float Interval);

    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    UMediaTexture* GetSlotTexture(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    UUserWidget* GetVideoUIBySlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Utils")
    UMediaPlayer* GetMediaPlayerBySlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Utils")
    void ResumeVideo(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Utils")
    void PauseVideo(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Utils")
    void PlayFromStart(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Utils")
    UAudioComponent* GetSlotAudioComponent(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Utils")
    void ActivateSoundCompBySlotIndex(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Video")
    void BindUIWithPlayerID(int32 PlayerID, UUserWidget* TargetUI);



    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    void PlayGiftVideoWithLogic(const FVideoPlayConfig& Config, float SoundValue, AActor* Actor);


    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    void PlayRandomGiftVideoWithLogic(const FVideoPlayConfig& Config, float SoundValue, AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    void TriggerGiftLogicBatch(
        uint8 GiftEnumValue,
        int32 TriggerCount,
        float TriggerInterval = 0.5f,
        int32 LogicCallCountPerTrigger = 1,
        float LogicDelayTime = 0.0f,
        float InitialDelay = 0.0f
    );


    // 顺序播放完成委托
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnSequentialPlayCompleted OnSequentialPlayCompleted;

    // 顺序播放单个视频完成委托
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnSequentialItemCompleted OnSequentialItemCompleted;

    /**
     * 开始顺序播放视频队列
     * @param Items 要顺序播放的视频列表
     * @param SoundValue 音量
     * @param Actor Owner Actor
     * @return 队列ID，用于追踪此队列
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    int32 StartSequentialPlay(const TArray<FSequentialPlayItem>& Items, float SoundValue, AActor* Actor);

    /**
     * 停止指定的顺序播放队列
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    void StopSequentialPlay(int32 QueueID);

    /**
     * 停止所有顺序播放队列
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    void StopAllSequentialPlays();

    /**
     * 获取顺序播放队列的当前进度
     * @return 当前播放到第几个（从0开始），-1表示队列不存在
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    int32 GetSequentialPlayProgress(int32 QueueID);


    // 逻辑触发委托 - 绑定到蓝图中调用 GiftFunc
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnGiftLogicTrigger OnGiftLogicTrigger;

    private:
        // 顺序播放队列列表
        UPROPERTY()
        TArray<FSequentialPlayQueue> SequentialQueues;

        // 队列ID计数器
        int32 NextQueueID = 0;

        // Slot到队列的映射（用于识别结束的视频属于哪个队列）
        TMap<int32, int32> SlotToQueueMap;

        // 播放队列中的下一个视频
        void PlayNextInQueue(int32 QueueID);

        // 处理顺序播放的视频结束
        void HandleSequentialMediaEnd(int32 SlotIndex);

        // 查找队列
        FSequentialPlayQueue* FindQueue(int32 QueueID);

        void OnHaSaGiPlayEnd();

        UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
        FOnHaSaGiPlayCompleted OnHaSaGiPlayCompleted;

private:
    static constexpr int32 POOL_SIZE = 16;

    UPROPERTY()
    TArray<FSlot> VideoSlots;

    void InitPool();
    FSlot* FindFreeSlot();

    // 创建新的 Slot
    FSlot* CreateNewSlot(AActor* Actor);

    // 批次播放相关
    UPROPERTY()
    TArray<FBatchPlayRequest> BatchRequests;

    void OnBatchIntervalTick(int32 BatchIndex);
    void StartSlotPlayback(int32 SlotIndex);

private:
    // 应用 UI 配置
    void ApplyUIConfig(int32 SlotIndex, const FGiftUIDisplayConfig& Config);
    // 重置 UI 到默认状态
    void ResetUIToDefault(int32 SlotIndex);

    // 默认 UI 配置
    FGiftUIDisplayConfig DefaultUIConfig;


    // 逻辑调用队列
    UPROPERTY()
    TArray<FLogicCallRequest> PendingLogicCalls;

    // 处理逻辑触发
    void ScheduleLogicCall(uint8 GiftEnumValue, int32 ActivateTime, float DelayTime, float TimeOffset);
    void ExecuteLogicCall(int32 RequestIndex);
    void CleanupLogicTimers();



public:
    UPROPERTY()
    TMap<UMediaPlayer*, UMediaEndProxy*> PlayerToProxyMap;

    TMap<int32, TWeakObjectPtr<UUserWidget>> IDToWidgetMap;

    void CreateProxyForPlayer(UMediaPlayer* Player, int32 PlayerID);
    void HandleMediaOpened(int32 PlayerID);
    void HandleMediaEnd(int32 PlayerID);
    void CleanupProxies();






public:
    // ========== [Claude] Box/Special Trigger 顺序播放接口 ==========

    // Box触发逻辑委托 - 蓝图绑定此委托执行 GiftFunCall
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate|BoxTrigger")
    FOnBoxGiftLogicTrigger OnBoxGiftLogicTrigger;

    // Special触发逻辑委托 - 蓝图绑定此委托执行 GiftFunCall
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate|SpecialTrigger")
    FOnSpecialGiftLogicTrigger OnSpecialGiftLogicTrigger;

    /**
     * [Claude] BoxTrigger 的 C++ 实现
     * 使用顺序播放系统，避免高并发时 Timer 冲突
     * @param GiftEnums 礼物枚举数组
     * @param LogicStartOffsets 每个礼物对应的逻辑启动偏移时间数组
     * @param Interval 视频播放间隔
     * @param RandomTimes 随机次数（循环次数）
     * @param AnimName 动画/视频名称
     * @param SoundValue 音量
     * @param Actor Owner Actor
     * @return 队列ID
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
    int32 PlayBoxTriggerSequence(
        const TArray<uint8>& GiftEnums,
        const TArray<float>& LogicStartOffsets,
        float Interval,
        int32 RandomTimes,
        const FString& AnimName,
        float SoundValue,
        AActor* Actor
    );

    /**
     * [Claude] SpecialTrigger 的 C++ 实现
     * 使用顺序播放系统，避免高并发时 Timer 冲突
     * @param GiftEnums 礼物枚举数组
     * @param LogicStartOffsets 每个礼物对应的逻辑启动偏移时间数组
     * @param Interval 视频播放间隔
     * @param TriggerTime 触发次数
     * @param AnimName 动画/视频名称
     * @param SoundValue 音量
     * @param Actor Owner Actor
     * @return 队列ID
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
    int32 PlaySpecialTriggerSequence(
        const TArray<uint8>& GiftEnums,
        const TArray<float>& LogicStartOffsets,
        float Interval,
        int32 TriggerTime,
        const FString& AnimName,
        float SoundValue,
        AActor* Actor
    );

private:
    // ========== [Claude] Box/Special Trigger 内部处理 ==========

    // 触发类型枚举，用于区分 Box 和 Special
    enum class ETriggerType : uint8
    {
        Box,
        Special
    };

    // 触发队列上下文，存储每个队列的附加信息
    struct FTriggerQueueContext
    {
        ETriggerType TriggerType = ETriggerType::Box;
        TArray<uint8> GiftEnums;
        TArray<float> LogicStartOffsets;
        float Interval = 0.5f;
        FString GiftName;
    };

    // 队列ID到上下文的映射
    TMap<int32, FTriggerQueueContext> TriggerQueueContextMap;

    // 内部处理函数：当顺序播放项完成时调用
    UFUNCTION()
    void HandleTriggerItemCompleted(int32 QueueID, int32 ItemIndex, int32 SlotIndex);

    // 查找视频 MediaSource（通过名称从 DataTable 查找）
    UFileMediaSource* FindMediaSourceByName(const FString& VideoName);





    public:
        // ========== [Claude] 单次礼物队列系统（按礼物类型分队列） ==========

        // 单次礼物队列完成委托
        UPROPERTY(BlueprintAssignable, Category = "Video Delegate|SingleGift")
        FOnSingleGiftQueueCompleted OnSingleGiftQueueCompleted;

        // 单次礼物单项完成委托
        UPROPERTY(BlueprintAssignable, Category = "Video Delegate|SingleGift")
        FOnSingleGiftItemCompleted OnSingleGiftItemCompleted;

        /**
         * [Claude] 将单次礼物加入对应类型的队列
         * 同种礼物快速多次调用时会排队，不同种礼物可同时播放
         * @param Request 礼物请求配置（包含 GiftEnumValue 用于区分队列）
         * @param PlayCount 播放次数
         * @param Interval 播放间隔（秒）
         * @param SoundValue 音量
         * @param Actor Owner Actor
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        void EnqueueSingleGift(
            const FSingleGiftRequest& Request,
            int32 PlayCount,
            float Interval,
            float SoundValue,
            AActor* Actor
        );

        /**
         * [Claude] 清空指定礼物类型的队列
         * @param GiftEnumValue 礼物类型枚举值
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        void ClearSingleGiftQueue(uint8 GiftEnumValue);

        /**
         * [Claude] 清空所有礼物类型的队列
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        void ClearAllSingleGiftQueues();

        /**
         * [Claude] 获取指定礼物类型的队列剩余长度
         * @param GiftEnumValue 礼物类型枚举值
         * @return 队列中剩余待播放的数量
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        int32 GetSingleGiftQueueLength(uint8 GiftEnumValue) const;

        /**
         * [Claude] 获取当前活跃的礼物队列数量
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        int32 GetActiveSingleGiftQueueCount() const;

private:
    // ========== [Claude] 单次礼物队列内部（按 GiftEnumValue 分队列） ==========

    // [Claude] 礼物类型 -> 队列ID 的映射（每种礼物有独立队列）
    TMap<uint8, int32> GiftTypeToQueueMap;

    // [Claude] 队列ID -> 礼物类型 的反向映射（用于回调时查找礼物类型）
    TMap<int32, uint8> QueueToGiftTypeMap;

    // [Claude] 队列ID -> 配置信息的映射
    struct FSingleGiftQueueConfig
    {
        float Interval = 0.5f;
        float SoundValue = 1.0f;
        TWeakObjectPtr<AActor> OwnerActor;
    };
    TMap<int32, FSingleGiftQueueConfig> SingleGiftQueueConfigs;

    // 处理单次礼物队列完成
    UFUNCTION()
    void HandleSingleGiftQueueCompleted(int32 QueueID);

    // 处理单次礼物单项完成
    UFUNCTION()
    void HandleSingleGiftItemCompleted(int32 QueueID, int32 ItemIndex, int32 SlotIndex);


    public:
        /**
         * [Claude] BoxTrigger 排队版本
         * 同种 AnimName 会自动追加到队列，支持倍率
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void EnqueueBoxTrigger(
            const TArray<uint8>& GiftEnums,
            const TArray<float>& LogicStartOffsets,
            float Interval,
            int32 RandomTimes,
            const FString& AnimName,
            float SoundValue,
            AActor* Actor,
            int32 Multiplier = 1
        );

        /**
         * [Claude] SpecialTrigger 排队版本
         * 同种 AnimName 会自动追加到队列，支持倍率
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void EnqueueSpecialTrigger(
            const TArray<uint8>& GiftEnums,
            const TArray<float>& LogicStartOffsets,
            float Interval,
            int32 TriggerTime,
            const FString& AnimName,
            float SoundValue,
            AActor* Actor,
            int32 Multiplier = 1
        );

        /**
         * [Claude] 清空指定 Box 触发队列
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void ClearBoxTriggerQueue(const FString& AnimName);

        /**
         * [Claude] 清空指定 Special 触发队列
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void ClearSpecialTriggerQueue(const FString& AnimName);

        /**
         * [Claude] 清空所有 Trigger 队列
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void ClearAllTriggerQueues();


    private:
        // ========== [Claude] Box/Special Trigger 排队系统数据结构 ==========

        // [Claude] Box触发的独立映射（AnimName -> QueueID）
        TMap<FString, int32> BoxTriggerToQueueMap;
        TMap<int32, FString> QueueToBoxTriggerMap;  // 反向映射

        // [Claude] Special触发的独立映射
        TMap<FString, int32> SpecialTriggerToQueueMap;
        TMap<int32, FString> QueueToSpecialTriggerMap;  // 反向映射

        // [Claude] 辅助函数：构建触发项列表
        TArray<FSequentialPlayItem> BuildTriggerItems(
            const TArray<uint8>& GiftEnums,
            const TArray<float>& LogicStartOffsets,
            float Interval,
            int32 LoopCount,
            const FString& AnimName);

        // [Claude] 处理排队版本的完成回调
        UFUNCTION()
        void HandleBoxTriggerQueueCompleted(int32 QueueID);

        UFUNCTION()
        void HandleSpecialTriggerQueueCompleted(int32 QueueID);


};


class UImage;
class UMediaTexture;
class UMaterialInstanceDynamic;

UCLASS()
class UChormaProcessor : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    void SetVideoToImage(UImage* TargetImage, UMediaTexture* MediaTex);

private:
    UPROPERTY()
    class UMaterialInterface* ChromaKeyMat = nullptr;
};


UCLASS()
class UVideoSubsystemLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Video", meta = (WorldContext = "WorldContextObject"))
    static UGiftVideoPool* GetGiftVideoPool(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category = "Video", meta = (WorldContext = "WorldContextObject"))
    static UChormaProcessor* GetChormaProcessor(const UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
    static void SetTabNavigationEnabled(bool bEnabled);
};


UCLASS()
class UMediaEndProxy : public UObject
{
    GENERATED_BODY()

public:
    void InitProxy(int32 InPlayerID, UGiftVideoPool* InOwnerPool);

    UFUNCTION()
    void OnProxyMediaOpened(FString OpenedUrl);
    UFUNCTION()
    void OnProxyMediaEnd();

    // [Claude] 重置防重入状态，在开始新播放时调用
    void ResetEndState();

private:
    UPROPERTY()
    TWeakObjectPtr<UGiftVideoPool> OwnerPool;

    int32 PlayerID = INDEX_NONE;

    // [Claude] 防止 OnEndReached 重复触发
    double LastEndTime = 0.0;
    static constexpr double END_DEBOUNCE_THRESHOLD = 0.1;  // 100ms 内的重复调用视为同一次
};