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
// �߼�����ί�� - ����֪ͨ��ͼִ�� GiftFunc
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGiftLogicTrigger, uint8, GiftEnumValue, int32, ActivateTime, float, DelayTime);

// ˳�򲥷����ί��
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSequentialPlayCompleted, int32, QueueID);
// ˳�򲥷ŵ�����Ƶ���ί��
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSequentialItemCompleted, int32, QueueID, int32, ItemIndex, int32, SlotIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHaSaGiPlayCompleted);


// ========== [Claude] Box/Special Trigger ˳�򲥷�ϵͳ ==========

// Box�����߼����ί�� - ��ͼ�а󶨴�ί�������� GiftFunCall
// ����: GiftEnum, ActivateTime, LogicStartTimeOffset, VideoTimeInterval
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnBoxGiftLogicTrigger, uint8, GiftEnumValue, int32, ActivateTime, float, LogicStartTimeOffset, float, VideoTimeInterval);

// Special�����߼����ί��
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSpecialGiftLogicTrigger, uint8, GiftEnumValue, int32, ActivateTime, float, LogicStartTimeOffset, float, VideoTimeInterval);

// ��������������ί�� - [Claude] ���� GiftEnumValue ����������������������Ķ������
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSingleGiftQueueCompleted, uint8, GiftEnumValue);
// �������ﵥ�����ί�� - [Claude] ���� GiftEnumValue ����
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

    // λ��ƫ��
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FVector2D Translation = FVector2D::ZeroVector;

    // ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FVector2D Scale = FVector2D(1.0f, 1.0f);

    // ��ת�Ƕ�
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


    // ��ǰʹ�õ� UI ����
    FGiftUIDisplayConfig CurrentUIConfig;
    bool bHasCustomConfig = false;

    bool  bBusy = false;
    int32 PoolIndex = INDEX_NONE;
};

// ���β�������ṹ
USTRUCT(BlueprintType)
struct FBatchPlayRequest
{
    GENERATED_BODY()

    UPROPERTY()
    UFileMediaSource* MediaSource = nullptr;

    UPROPERTY()
    TArray<int32> SlotIndices;  // Ԥ����Ĳ�λ

    UPROPERTY()
    int32 PlayCount = 1;  // ���Ŵ���

    UPROPERTY()
    float Interval = 0.5f;  // ���ż�����룩

    UPROPERTY()
    int32 CurrentIndex = 0;  // ��ǰ���ŵ��ڼ���

    UPROPERTY()
    AActor* OwnerActor = nullptr;

    FTimerHandle IntervalTimerHandle;
};



// ��Ƶ�������ýṹ��
USTRUCT(BlueprintType)
struct FVideoPlayConfig
{
    GENERATED_BODY()

    // ��ƵԴ
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    TArray<UFileMediaSource*> MediaSources;

    // ��������ö�� (��Ӧ��ͼ�е� Gift_Enum)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    TArray<uint8> GiftEnumValue;

    // ���Ŵ���
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    int32 PlayCount = 1;

    // ��Ƶ���ż�����룩
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    float VideoInterval = 0.5f;

    // �߼������������Ƶ���ŵ�ʱ��ƫ�ƣ��룩
    // �������߼��ӳٴ������������߼���ǰ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    float LogicTimeOffset = 0.0f;

    // ÿ����Ƶ����ʱ�߼����ô���
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    int32 LogicCallCount = 1;

    // �߼����õĸ����ӳ�ʱ��
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    float LogicDelayTime = 0.0f;

    // ������UI ��ʾ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    TArray<FGiftUIDisplayConfig> UIDisplayConfig;

    // �Ƿ�ʹ���Զ��� UI ���ã�false ��ʹ��Ĭ�ϣ�
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    bool bUseCustomUIConfig = false;
};

// �����߼���������
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

// ˳�򲥷�������
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

// ˳�򲥷Ŷ���
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

    /** ���ŵ�����Ƶ */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo")
    int32 PlayGiftVideo(UFileMediaSource* MediaSource, AActor* Actor);

    /** ���β�����Ƶ */
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


    // ˳�򲥷����ί��
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnSequentialPlayCompleted OnSequentialPlayCompleted;

    // ˳�򲥷ŵ�����Ƶ���ί��
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnSequentialItemCompleted OnSequentialItemCompleted;

    /**
     * ��ʼ˳�򲥷���Ƶ����
     * @param Items Ҫ˳�򲥷ŵ���Ƶ�б�
     * @param SoundValue ����
     * @param Actor Owner Actor
     * @return ����ID������׷�ٴ˶���
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    int32 StartSequentialPlay(const TArray<FSequentialPlayItem>& Items, float SoundValue, AActor* Actor);

    /**
     * ָֹͣ����˳�򲥷Ŷ���
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    void StopSequentialPlay(int32 QueueID);

    /**
     * ֹͣ����˳�򲥷Ŷ���
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    void StopAllSequentialPlays();

    /**
     * ��ȡ˳�򲥷Ŷ��еĵ�ǰ����
     * @return ��ǰ���ŵ��ڼ�������0��ʼ����-1��ʾ���в�����
     */
    UFUNCTION(BlueprintCallable, Category = "GiftVideo|Sequential")
    int32 GetSequentialPlayProgress(int32 QueueID);


    // �߼�����ί�� - �󶨵���ͼ�е��� GiftFunc
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate")
    FOnGiftLogicTrigger OnGiftLogicTrigger;

    private:
        // ˳�򲥷Ŷ����б�
        UPROPERTY()
        TArray<FSequentialPlayQueue> SequentialQueues;

        // ����ID������
        int32 NextQueueID = 0;

        // Slot�����е�ӳ�䣨����ʶ���������Ƶ�����ĸ����У�
        TMap<int32, int32> SlotToQueueMap;

        // ���Ŷ����е���һ����Ƶ
        void PlayNextInQueue(int32 QueueID);

        // ����˳�򲥷ŵ���Ƶ����
        void HandleSequentialMediaEnd(int32 SlotIndex);

        // ���Ҷ���
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

    // �����µ� Slot
    FSlot* CreateNewSlot(AActor* Actor);

    // ���β������
    UPROPERTY()
    TArray<FBatchPlayRequest> BatchRequests;

    void OnBatchIntervalTick(int32 BatchIndex);
    void StartSlotPlayback(int32 SlotIndex);

private:
    // Ӧ�� UI ����
    void ApplyUIConfig(int32 SlotIndex, const FGiftUIDisplayConfig& Config);
    // ���� UI ��Ĭ��״̬
    void ResetUIToDefault(int32 SlotIndex);

    // Ĭ�� UI ����
    FGiftUIDisplayConfig DefaultUIConfig;


    // �߼����ö���
    UPROPERTY()
    TArray<FLogicCallRequest> PendingLogicCalls;

    // �����߼�����
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
    // ========== [Claude] Box/Special Trigger ˳�򲥷Žӿ� ==========

    // Box�����߼�ί�� - ��ͼ�󶨴�ί��ִ�� GiftFunCall
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate|BoxTrigger")
    FOnBoxGiftLogicTrigger OnBoxGiftLogicTrigger;

    // Special�����߼�ί�� - ��ͼ�󶨴�ί��ִ�� GiftFunCall
    UPROPERTY(BlueprintAssignable, Category = "Video Delegate|SpecialTrigger")
    FOnSpecialGiftLogicTrigger OnSpecialGiftLogicTrigger;

    /**
     * [Claude] BoxTrigger �� C++ ʵ��
     * ʹ��˳�򲥷�ϵͳ������߲���ʱ Timer ��ͻ
     * @param GiftEnums ����ö������
     * @param LogicStartOffsets ÿ�������Ӧ���߼�����ƫ��ʱ������
     * @param Interval ��Ƶ���ż��
     * @param RandomTimes ���������ѭ��������
     * @param AnimName ����/��Ƶ����
     * @param SoundValue ����
     * @param Actor Owner Actor
     * @return ����ID
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
     * [Claude] SpecialTrigger �� C++ ʵ��
     * ʹ��˳�򲥷�ϵͳ������߲���ʱ Timer ��ͻ
     * @param GiftEnums ����ö������
     * @param LogicStartOffsets ÿ�������Ӧ���߼�����ƫ��ʱ������
     * @param Interval ��Ƶ���ż��
     * @param TriggerTime ��������
     * @param AnimName ����/��Ƶ����
     * @param SoundValue ����
     * @param Actor Owner Actor
     * @return ����ID
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
    // ========== [Claude] Box/Special Trigger �ڲ����� ==========

    // ��������ö�٣��������� Box �� Special
    enum class ETriggerType : uint8
    {
        Box,
        Special
    };

    // �������������ģ��洢ÿ�����еĸ�����Ϣ
    struct FTriggerQueueContext
    {
        ETriggerType TriggerType = ETriggerType::Box;
        TArray<uint8> GiftEnums;
        TArray<float> LogicStartOffsets;
        float Interval = 0.5f;
        FString GiftName;
    };

    // ����ID�������ĵ�ӳ��
    TMap<int32, FTriggerQueueContext> TriggerQueueContextMap;

    // �ڲ�������������˳�򲥷������ʱ����
    UFUNCTION()
    void HandleTriggerItemCompleted(int32 QueueID, int32 ItemIndex, int32 SlotIndex);

    // ������Ƶ MediaSource��ͨ�����ƴ� DataTable ���ң�
    UFileMediaSource* FindMediaSourceByName(const FString& VideoName);





    public:
        // ========== [Claude] �����������ϵͳ�����������ͷֶ��У� ==========

        // ��������������ί��
        UPROPERTY(BlueprintAssignable, Category = "Video Delegate|SingleGift")
        FOnSingleGiftQueueCompleted OnSingleGiftQueueCompleted;

        // �������ﵥ�����ί��
        UPROPERTY(BlueprintAssignable, Category = "Video Delegate|SingleGift")
        FOnSingleGiftItemCompleted OnSingleGiftItemCompleted;

        /**
         * [Claude] ��������������Ӧ���͵Ķ���
         * ͬ��������ٶ�ε���ʱ���Ŷӣ���ͬ�������ͬʱ����
         * @param Request �����������ã����� GiftEnumValue �������ֶ��У�
         * @param PlayCount ���Ŵ���
         * @param Interval ���ż�����룩
         * @param SoundValue ����
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
         * [Claude] ���ָ���������͵Ķ���
         * @param GiftEnumValue ��������ö��ֵ
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        void ClearSingleGiftQueue(uint8 GiftEnumValue);

        /**
         * [Claude] ��������������͵Ķ���
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        void ClearAllSingleGiftQueues();

        /**
         * [Claude] ��ȡָ���������͵Ķ���ʣ�೤��
         * @param GiftEnumValue ��������ö��ֵ
         * @return ������ʣ������ŵ�����
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        int32 GetSingleGiftQueueLength(uint8 GiftEnumValue) const;

        /**
         * [Claude] ��ȡ��ǰ��Ծ�������������
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|SingleGift")
        int32 GetActiveSingleGiftQueueCount() const;

private:
    // ========== [Claude] ������������ڲ����� GiftEnumValue �ֶ��У� ==========

    // [Claude] �������� -> ����ID ��ӳ�䣨ÿ�������ж������У�
    TMap<uint8, int32> GiftTypeToQueueMap;

    // [Claude] ����ID -> �������� �ķ���ӳ�䣨���ڻص�ʱ�����������ͣ�
    TMap<int32, uint8> QueueToGiftTypeMap;

    // [Claude] ����ID -> ������Ϣ��ӳ��
    struct FSingleGiftQueueConfig
    {
        float Interval = 0.5f;
        float SoundValue = 1.0f;
        TWeakObjectPtr<AActor> OwnerActor;
    };
    TMap<int32, FSingleGiftQueueConfig> SingleGiftQueueConfigs;

    // ������������������
    UFUNCTION()
    void HandleSingleGiftQueueCompleted(int32 QueueID);

    // �����������ﵥ�����
    UFUNCTION()
    void HandleSingleGiftItemCompleted(int32 QueueID, int32 ItemIndex, int32 SlotIndex);


    public:
        /**
         * [Claude] BoxTrigger �ŶӰ汾
         * ͬ�� AnimName ���Զ�׷�ӵ����У�֧�ֱ���
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void EnqueueBoxTrigger(
            const TArray<FString>& CallNames,
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
         * [Claude] SpecialTrigger �ŶӰ汾
         * ͬ�� AnimName ���Զ�׷�ӵ����У�֧�ֱ���
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void EnqueueSpecialTrigger(
            const TArray<FString>& CallNames,
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
         * [Claude] ���ָ�� Box ��������
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void ClearBoxTriggerQueue(const FString& AnimName);

        /**
         * [Claude] ���ָ�� Special ��������
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void ClearSpecialTriggerQueue(const FString& AnimName);

        /**
         * [Claude] ������� Trigger ����
         */
        UFUNCTION(BlueprintCallable, Category = "GiftVideo|Trigger")
        void ClearAllTriggerQueues();


    private:
        // ========== [Claude] Box/Special Trigger �Ŷ�ϵͳ���ݽṹ ==========

        // [Claude] Box�����Ķ���ӳ�䣨AnimName -> QueueID��
        TMap<FString, int32> BoxTriggerToQueueMap;
        TMap<int32, FString> QueueToBoxTriggerMap;  // ����ӳ��

        // [Claude] Special�����Ķ���ӳ��
        TMap<FString, int32> SpecialTriggerToQueueMap;
        TMap<int32, FString> QueueToSpecialTriggerMap;  // ����ӳ��

        // [Claude] ���������������������б�
        TArray<FSequentialPlayItem> BuildTriggerItems(
            const TArray<FString>& CallNames,
            const TArray<uint8>& GiftEnums,
            const TArray<float>& LogicStartOffsets,
            float Interval,
            int32 LoopCount,
            const FString& AnimName,
            bool isSpecial
            );

        // [Claude] �����ŶӰ汾����ɻص�
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

    // [Claude] ���÷�����״̬���ڿ�ʼ�²���ʱ����
    void ResetEndState();

private:
    UPROPERTY()
    TWeakObjectPtr<UGiftVideoPool> OwnerPool;

    int32 PlayerID = INDEX_NONE;

    // [Claude] ��ֹ OnEndReached �ظ�����
    double LastEndTime = 0.0;
    static constexpr double END_DEBOUNCE_THRESHOLD = 0.1;  // 100ms �ڵ��ظ�������Ϊͬһ��
};