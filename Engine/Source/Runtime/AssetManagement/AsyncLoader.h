#pragma once
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <functional>
#include "Enums.h"

class UResourceBase;

/**
 * @brief 에셋 로딩 상태
 */
enum class EAssetLoadState : uint8
{
	NotLoaded,
	Queued,
	Loading,
	Loaded,
	Failed
};

/**
 * @brief 로딩 우선순위
 */
enum class EAssetLoadPriority : uint8
{
	Low = 0,
	Normal = 1,
	High = 2,
	Critical = 3
};

/**
 * @brief 로드 완료된 리소스 정보 (메인 스레드에서 처리 대기)
 * @details 워커 스레드에서 로드 완료 후, 메인 스레드에서 ResourceManager 삽입 및 콜백 실행
 *
 * @param FilePath 파일 경로
 * @param ResourceType 리소스 타입
 * @param Resource 로드된 리소스 (아직 ResourceManager에 등록되지 않음)
 * @param Callback 로드 완료 콜백
 * @param bSuccess 로드 성공 여부
 */
struct FCompletedLoadResult
{
	FString FilePath;
	EResourceType ResourceType;
	UResourceBase* Resource = nullptr;
	std::function<void(UResourceBase*)> Callback;
	bool bSuccess = false;
};

/**
 * @brief 비동기 로딩 요청
 *
 * @param FilePath 로드할 파일 경로
 * @param ResourceType 리소스 타입
 * @param Priority 로딩 우선순위
 * @param Callback 로드 완료 시 호출될 콜백 (메인 스레드에서 실행)
 */
struct FAsyncLoadRequest
{
	FString FilePath;
	EResourceType ResourceType;
	EAssetLoadPriority Priority = EAssetLoadPriority::Normal;
	std::function<void(UResourceBase*)> Callback;

	bool operator<(const FAsyncLoadRequest& Other) const
	{
		return static_cast<uint8>(Priority) < static_cast<uint8>(Other.Priority);
	}
};

/**
 * @brief 스트리밍 핸들, 로딩 상태 추적
 */
struct FStreamableHandle
{
	std::atomic<EAssetLoadState> LoadState{EAssetLoadState::NotLoaded};
	FString FilePath;
	UResourceBase* LoadedResource = nullptr;

	// 콜백 리스트로 관리 (여러 요청의 콜백을 모두 보존)
	TArray<std::function<void(UResourceBase*)>> Callbacks;

	bool IsLoading() const { return LoadState == EAssetLoadState::Loading || LoadState == EAssetLoadState::Queued; }
	bool IsLoaded() const { return LoadState == EAssetLoadState::Loaded; }
	bool HasFailed() const { return LoadState == EAssetLoadState::Failed; }

	void AddCallback(std::function<void(UResourceBase*)> InCallback)
	{
		if (InCallback)
		{
			Callbacks.Add(std::move(InCallback));
		}
	}

	void ClearCallbacks()
	{
		Callbacks.clear();
	}

	void ReleaseHandle()
	{
		LoadedResource = nullptr;
		Callbacks.clear();
	}
};

/**
 * @brief 멀티스레드 비동기 에셋 로더
 * @details 워커 스레드에서 파일 로드, 메인 스레드에서 ResourceManager 삽입
 *
 * Work Flow:
 * AsyncLoad 호출 시 요청이 RequestQueue에 추가
 * 워커 스레드가 RequestQueue에서 꺼내 로드 수행
 * 로드 완료된 리소스는 CompletedQueue에 추가 (ResourceManager에 아직 삽입 안됨)
 * 메인 스레드 Tick에서 ProcessCompletedResources 호출
 * CompletedQueue의 리소스를 ResourceManager에 삽입하고 콜백 실행
 */
class FAsyncLoader
{
public:
	static FAsyncLoader& Get();

	void Initialize(ID3D11Device* InDevice);
	void Shutdown();

	std::shared_ptr<FStreamableHandle> RequestAsyncLoad(
		const FString& FilePath,
		EResourceType ResourceType,
		std::function<void(UResourceBase*)> Callback = nullptr,
		EAssetLoadPriority Priority = EAssetLoadPriority::Normal
	);

	void ProcessCompletedResources();

	// 씬 전환 시 호출, 모든 대기 중인 콜백 제거 (로드는 계속 진행)
	void ClearAllCallbacks();

	// 다이얼로그 등 모달 UI 전에 호출, Worker 스레드 일시 정지하는 용도
	void Pause();
	void Resume();
	bool IsPaused() const { return bPaused.load(); }

	EAssetLoadState GetLoadState(const FString& FilePath) const;
	bool IsLoading() const;
	float GetLoadProgress() const;
	int32 GetPendingCount() const { return PendingCount.load(); }
	int32 GetCompletedCount() const { return CompletedCount.load(); }
	int32 GetTotalRequestedCount() const { return TotalRequestedCount.load(); }
	TArray<FString> GetCurrentlyLoadingAssets() const;

	// 로딩 세션 카운터 리셋 (모든 로드 완료 후 호출하여 다음 로딩 세션을 위해 카운터 초기화)
	void ResetSessionCounters();

private:
	FAsyncLoader() = default;
	~FAsyncLoader();

	FAsyncLoader(const FAsyncLoader&) = delete;
	FAsyncLoader& operator=(const FAsyncLoader&) = delete;

	void WorkerThreadFunc();
	UResourceBase* LoadResourceOnWorker(const FString& FilePath, EResourceType ResourceType);

	ID3D11Device* Device = nullptr;

	std::thread WorkerThread;
	std::atomic<bool> bShutdownRequested{false};
	std::atomic<bool> bWorkerRunning{false};
	std::atomic<bool> bPaused{false};
	std::condition_variable PauseCV;
	std::mutex PauseMutex;

	std::priority_queue<FAsyncLoadRequest> RequestQueue;
	mutable std::mutex RequestMutex;
	std::condition_variable RequestCV;

	TArray<FCompletedLoadResult> CompletedQueue;
	mutable std::mutex CompletedMutex;

	TMap<FString, std::shared_ptr<FStreamableHandle>> HandleMap;
	mutable std::mutex HandleMutex;

	std::atomic<int32> PendingCount{0};
	std::atomic<int32> CompletedCount{0};
	std::atomic<int32> TotalRequestedCount{0};

	FString CurrentLoadingAsset;
	mutable std::mutex CurrentAssetMutex;
};
