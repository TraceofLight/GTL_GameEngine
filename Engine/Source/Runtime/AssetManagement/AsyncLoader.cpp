#include "pch.h"
#include "AsyncLoader.h"
#include "ResourceManager.h"
#include "StaticMesh.h"
#include "SkeletalMesh.h"
#include "Texture.h"
#include "Material.h"
#include "../Engine/Audio/Sound.h"
#include "../Engine/Animation/AnimSequence.h"

FAsyncLoader& FAsyncLoader::Get()
{
	static FAsyncLoader Instance;
	return Instance;
}

FAsyncLoader::~FAsyncLoader()
{
	Shutdown();
}

void FAsyncLoader::Initialize(ID3D11Device* InDevice, int32 NumWorkers)
{
	Device = InDevice;

	// 워커 수 결정: 0이면 (CPU 코어 수 - 1), 최소 1개
	if (NumWorkers <= 0)
	{
		NumWorkers = static_cast<int32>(std::thread::hardware_concurrency());
		if (NumWorkers > 1)
		{
			--NumWorkers; // 메인 스레드를 위해 1개 예약
		}
	}
	NumWorkers = std::max(1, NumWorkers);

	bShutdownRequested = false;
	ActiveWorkerCount = 0;

	// 현재 로딩 중인 에셋 배열 초기화
	{
		std::lock_guard<std::mutex> Lock(CurrentAssetMutex);
		CurrentLoadingAssets.resize(NumWorkers);
	}

	// 워커 스레드 풀 생성
	WorkerThreads.reserve(NumWorkers);
	for (int32 i = 0; i < NumWorkers; ++i)
	{
		WorkerThreads.emplace_back(&FAsyncLoader::WorkerThreadFunc, this, i);
	}

	UE_LOG("AsyncLoader: Initialized with %d worker threads", NumWorkers);
}

void FAsyncLoader::Shutdown()
{
	bShutdownRequested = true;
	RequestCV.notify_all();
	PauseCV.notify_all();

	// 모든 워커 스레드 종료 대기
	for (auto& Worker : WorkerThreads)
	{
		if (Worker.joinable())
		{
			Worker.join();
		}
	}
	WorkerThreads.clear();

	{
		std::lock_guard<std::mutex> Lock(RequestMutex);
		while (!RequestQueue.empty())
		{
			RequestQueue.pop();
		}
	}

	{
		std::lock_guard<std::mutex> Lock(CompletedMutex);
		CompletedQueue.clear();
	}

	{
		std::lock_guard<std::mutex> Lock(HandleMutex);
		HandleMap.clear();
	}

	{
		std::lock_guard<std::mutex> Lock(CurrentAssetMutex);
		CurrentLoadingAssets.clear();
	}

	UE_LOG("AsyncLoader: Shutdown complete");
}

void FAsyncLoader::WorkerThreadFunc(int32 WorkerIndex)
{
	++ActiveWorkerCount;

	while (!bShutdownRequested)
	{
		FAsyncLoadRequest Request;
		bool bHasRequest = false;

		{
			std::unique_lock<std::mutex> Lock(RequestMutex);
			RequestCV.wait(Lock, [this]()
			{
				// 일시 정지 상태면 대기, 셧다운이면 종료, 요청 있으면 처리
				return bShutdownRequested || (!bPaused && !RequestQueue.empty());
			});

			if (bShutdownRequested)
			{
				break;
			}

			// 일시 정지 상태면 다시 대기
			if (bPaused)
			{
				continue;
			}

			if (!RequestQueue.empty())
			{
				Request = RequestQueue.top();
				RequestQueue.pop();
				bHasRequest = true;
			}
		}

		if (!bHasRequest)
		{
			continue;
		}

		// 현재 워커가 로딩 중인 에셋 기록
		{
			std::lock_guard<std::mutex> Lock(CurrentAssetMutex);
			if (WorkerIndex < static_cast<int32>(CurrentLoadingAssets.size()))
			{
				CurrentLoadingAssets[WorkerIndex] = Request.FilePath;
			}
		}

		{
			std::lock_guard<std::mutex> Lock(HandleMutex);
			auto* Handle = HandleMap.Find(Request.FilePath);
			if (Handle && *Handle)
			{
				(*Handle)->LoadState = EAssetLoadState::Loading;
			}
		}

		UResourceBase* LoadedResource = LoadResourceOnWorker(Request.FilePath, Request.ResourceType);

		FCompletedLoadResult Result;
		Result.FilePath = Request.FilePath;
		Result.ResourceType = Request.ResourceType;
		Result.Resource = LoadedResource;
		Result.Callback = Request.Callback;
		Result.bSuccess = (LoadedResource != nullptr);

		{
			std::lock_guard<std::mutex> Lock(CompletedMutex);
			CompletedQueue.push_back(Result);
		}

		{
			std::lock_guard<std::mutex> Lock(HandleMutex);
			auto* Handle = HandleMap.Find(Request.FilePath);
			if (Handle && *Handle)
			{
				(*Handle)->LoadState = Result.bSuccess ? EAssetLoadState::Loaded : EAssetLoadState::Failed;
				(*Handle)->LoadedResource = LoadedResource;
			}
		}

		--PendingCount;
		++CompletedCount;

		// 현재 워커의 로딩 상태 클리어
		{
			std::lock_guard<std::mutex> Lock(CurrentAssetMutex);
			if (WorkerIndex < static_cast<int32>(CurrentLoadingAssets.size()))
			{
				CurrentLoadingAssets[WorkerIndex].clear();
			}
		}

		// 일시 정지 대기 중인 메인 스레드에 알림
		PauseCV.notify_all();
	}

	--ActiveWorkerCount;
}

UResourceBase* FAsyncLoader::LoadResourceOnWorker(const FString& FilePath, EResourceType ResourceType)
{
	// Worker 스레드에서는 new T()로 직접 생성 (GUObjectArray 등록 없음)
	// 메인 스레드의 ProcessCompletedResources에서 AddToGUObjectArray 호출
	try
	{
		switch (ResourceType)
		{
		case EResourceType::StaticMesh:
		{
			UStaticMesh* Mesh = new UStaticMesh();
			Mesh->Load(FilePath, Device);
			Mesh->SetFilePath(FilePath);
			return Mesh;
		}

		case EResourceType::SkeletalMesh:
		{
			USkeletalMesh* Mesh = new USkeletalMesh();
			Mesh->Load(FilePath, Device);
			Mesh->SetFilePath(FilePath);
			return Mesh;
		}

		case EResourceType::Texture:
		{
			UTexture* Texture = new UTexture();
			Texture->Load(FilePath, Device);
			Texture->SetFilePath(FilePath);
			return Texture;
		}

		case EResourceType::Material:
		{
			UMaterial* Material = new UMaterial();
			Material->Load(FilePath, Device);
			Material->SetFilePath(FilePath);
			return Material;
		}

		case EResourceType::Sound:
		{
			USound* Sound = new USound();
			Sound->Load(FilePath, Device);
			Sound->SetFilePath(FilePath);
			return Sound;
		}

		case EResourceType::Animation:
		{
			UAnimSequence* Anim = new UAnimSequence();
			Anim->Load(FilePath, Device);
			Anim->SetFilePath(FilePath);
			return Anim;
		}

		default:
			UE_LOG("AsyncLoader: Unsupported resource type for %s", FilePath.c_str());
			return nullptr;
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG("AsyncLoader: Failed to load %s - %s", FilePath.c_str(), e.what());
		return nullptr;
	}
	catch (...)
	{
		UE_LOG("AsyncLoader: Unknown exception loading %s", FilePath.c_str());
		return nullptr;
	}
}

std::shared_ptr<FStreamableHandle> FAsyncLoader::RequestAsyncLoad(
	const FString& FilePath,
	EResourceType ResourceType,
	std::function<void(UResourceBase*)> Callback,
	EAssetLoadPriority Priority)
{
	FString NormalizedPath = NormalizePath(FilePath);

	{
		std::lock_guard<std::mutex> Lock(HandleMutex);
		auto* ExistingHandle = HandleMap.Find(NormalizedPath);
		if (ExistingHandle && *ExistingHandle)
		{
			auto& Handle = *ExistingHandle;
			EAssetLoadState State = Handle->LoadState.load();

			// Queued, Loading, Loaded 상태 모두 콜백 리스트에 추가
			// Loaded 상태여도 ProcessCompletedResources가 아직 안 불렸을 수 있음
			if (State == EAssetLoadState::Queued || State == EAssetLoadState::Loading || State == EAssetLoadState::Loaded)
			{
				Handle->AddCallback(Callback);
				return Handle;
			}
		}
	}

	auto Handle = std::make_shared<FStreamableHandle>();
	Handle->FilePath = NormalizedPath;
	Handle->LoadState = EAssetLoadState::Queued;
	Handle->AddCallback(Callback);

	{
		std::lock_guard<std::mutex> Lock(HandleMutex);
		HandleMap[NormalizedPath] = Handle;
	}

	{
		std::lock_guard<std::mutex> Lock(RequestMutex);
		FAsyncLoadRequest Request;
		Request.FilePath = NormalizedPath;
		Request.ResourceType = ResourceType;
		Request.Priority = Priority;
		Request.Callback = Callback;
		RequestQueue.push(Request);
		++PendingCount;
		++TotalRequestedCount;
	}

	RequestCV.notify_all();  // 유휴 워커들을 깨워서 요청 처리

	return Handle;
}

void FAsyncLoader::ProcessCompletedResources()
{
	TArray<FCompletedLoadResult> ToProcess;

	{
		std::lock_guard<std::mutex> Lock(CompletedMutex);
		ToProcess = std::move(CompletedQueue);
		CompletedQueue.clear();
	}

	if (ToProcess.IsEmpty())
	{
		return;
	}

	auto& RM = UResourceManager::GetInstance();

	for (auto& Result : ToProcess)
	{
		if (Result.bSuccess && Result.Resource)
		{
			// 메인 스레드에서 GUObjectArray에 등록 (ObjectName, InternalIndex 설정)
			switch (Result.ResourceType)
			{
			case EResourceType::StaticMesh:
				AddToGUObjectArray(UStaticMesh::StaticClass(), Result.Resource);
				break;
			case EResourceType::SkeletalMesh:
				AddToGUObjectArray(USkeletalMesh::StaticClass(), Result.Resource);
				break;
			case EResourceType::Texture:
				AddToGUObjectArray(UTexture::StaticClass(), Result.Resource);
				break;
			case EResourceType::Material:
				AddToGUObjectArray(UMaterial::StaticClass(), Result.Resource);
				break;
			case EResourceType::Sound:
				AddToGUObjectArray(USound::StaticClass(), Result.Resource);
				break;
			case EResourceType::Animation:
				AddToGUObjectArray(UAnimSequence::StaticClass(), Result.Resource);
				break;
			default:
				break;
			}

			// ResourceManager에 등록
			bool bAdded = false;
			switch (Result.ResourceType)
			{
			case EResourceType::StaticMesh:
				bAdded = RM.Add<UStaticMesh>(Result.FilePath, Result.Resource);
				break;
			case EResourceType::SkeletalMesh:
				bAdded = RM.Add<USkeletalMesh>(Result.FilePath, Result.Resource);
				break;
			case EResourceType::Texture:
				bAdded = RM.Add<UTexture>(Result.FilePath, Result.Resource);
				break;
			case EResourceType::Material:
				bAdded = RM.Add<UMaterial>(Result.FilePath, Result.Resource);
				break;
			case EResourceType::Sound:
				bAdded = RM.Add<USound>(Result.FilePath, Result.Resource);
				break;
			case EResourceType::Animation:
				bAdded = RM.Add<UAnimSequence>(Result.FilePath, Result.Resource);
				break;
			default:
				break;
			}

			if (!bAdded)
			{
				UE_LOG("AsyncLoader: Resource already exists, skipping: %s", Result.FilePath.c_str());
			}
		}

		// 콜백 리스트 전체 실행 후 정리
		{
			std::lock_guard<std::mutex> Lock(HandleMutex);
			auto* Handle = HandleMap.Find(Result.FilePath);
			if (Handle && *Handle)
			{
				for (auto& Callback : (*Handle)->Callbacks)
				{
					if (Callback)
					{
						Callback(Result.Resource);
					}
				}
				(*Handle)->ClearCallbacks();
				// 처리 완료 후 핸들 제거 (다음 요청은 ResourceManager에서 즉시 처리됨)
				HandleMap.Remove(Result.FilePath);
			}
		}
	}
}

void FAsyncLoader::Pause()
{
	bPaused = true;
	// 모든 워커가 현재 작업을 완료할 때까지 대기
	std::unique_lock<std::mutex> Lock(PauseMutex);
	PauseCV.wait(Lock, [this]()
	{
		std::lock_guard<std::mutex> AssetLock(CurrentAssetMutex);
		for (const auto& Asset : CurrentLoadingAssets)
		{
			if (!Asset.empty())
			{
				return false; // 아직 작업 중인 워커가 있음
			}
		}
		return true; // 모든 워커가 유휴 상태
	});
	UE_LOG("AsyncLoader: Paused (all %d workers idle)", static_cast<int32>(WorkerThreads.size()));
}

void FAsyncLoader::Resume()
{
	bPaused = false;
	PauseCV.notify_all();
	RequestCV.notify_all();  // 모든 워커 깨우기
	UE_LOG("AsyncLoader: Resumed");
}

void FAsyncLoader::ClearAllCallbacks()
{
	// HandleMap의 모든 콜백 리스트 제거
	{
		std::lock_guard<std::mutex> Lock(HandleMutex);
		for (auto& Pair : HandleMap)
		{
			if (Pair.second)
			{
				Pair.second->ClearCallbacks();
			}
		}
	}

	UE_LOG("AsyncLoader: Cleared all callbacks for scene transition");
}

EAssetLoadState FAsyncLoader::GetLoadState(const FString& FilePath) const
{
	FString NormalizedPath = NormalizePath(FilePath);

	std::lock_guard<std::mutex> Lock(HandleMutex);
	auto* Handle = HandleMap.Find(NormalizedPath);
	if (Handle && *Handle)
	{
		return (*Handle)->LoadState.load();
	}

	return EAssetLoadState::NotLoaded;
}

bool FAsyncLoader::IsLoading() const
{
	return PendingCount.load() > 0 || !CompletedQueue.empty();
}

float FAsyncLoader::GetLoadProgress() const
{
	int32 Total = TotalRequestedCount.load();
	if (Total == 0)
	{
		return 1.0f;
	}

	int32 Completed = CompletedCount.load();
	return static_cast<float>(Completed) / static_cast<float>(Total);
}

TArray<FString> FAsyncLoader::GetCurrentlyLoadingAssets() const
{
	TArray<FString> Result;

	{
		std::lock_guard<std::mutex> Lock(CurrentAssetMutex);
		for (const auto& Asset : CurrentLoadingAssets)
		{
			if (!Asset.empty())
			{
				Result.push_back(Asset);
			}
		}
	}

	return Result;
}

void FAsyncLoader::ResetSessionCounters()
{
	// 로딩이 진행 중이면 리셋하지 않음
	if (PendingCount.load() > 0)
	{
		return;
	}

	TotalRequestedCount = 0;
	CompletedCount = 0;
}
