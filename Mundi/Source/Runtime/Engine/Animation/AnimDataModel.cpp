#include "pch.h"
#include "AnimDataModel.h"

IMPLEMENT_CLASS(UAnimDataModel)

UAnimDataModel::UAnimDataModel()
	: Skeleton(nullptr)
	, PlayLength(0.0f)
	, NumberOfFrames(0)
	, NumberOfKeys(0)
{
}

UAnimDataModel::~UAnimDataModel()
{
	// Skeleton 복사본 삭제
	if (Skeleton)
	{
		delete Skeleton;
		Skeleton = nullptr;
	}
}

void UAnimDataModel::SetSkeleton(const FSkeleton& InSkeleton)
{
	// 기존 Skeleton 삭제
	if (Skeleton)
	{
		delete Skeleton;
	}

	// 새로운 Skeleton 복사본 생성
	Skeleton = new FSkeleton(InSkeleton);

	// 캐시 초기화
	if (Skeleton && !Skeleton->bCacheInitialized)
	{
		Skeleton->InitializeCachedData();
	}
}
