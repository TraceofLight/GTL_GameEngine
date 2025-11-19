#include "pch.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Runtime/Core/Misc/WindowsBinReader.h"
#include "Source/Runtime/Core/Misc/Archive.h"

IMPLEMENT_CLASS(UAnimSequence)

UAnimSequence::UAnimSequence()
{
}

UAnimSequence::~UAnimSequence()
{
	// 부모 클래스의 소멸자가 DataModel을 정리하므로 여기서는 추가 작업 불필요
}

void UAnimSequence::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	// 파일 확장자 확인: .anim 파일이면 LoadFromAnimFile 호출
	if (InFilePath.ends_with(".anim"))
	{
		LoadFromAnimFile(InFilePath);
		return;
	}

	// 경로 형식: "FBX파일경로#AnimStackName"
	// '#'를 기준으로 FBX 파일 경로와 AnimStack 이름 분리
	size_t HashPos = InFilePath.find('#');
	if (HashPos == FString::npos)
	{
		UE_LOG("AnimSequence: Load: Invalid path format - %s", InFilePath.c_str());
		return;
	}

	FString FbxFilePath = InFilePath.substr(0, HashPos);
	FString AnimStackName = InFilePath.substr(HashPos + 1);

	// FBX에서 Skeleton 추출
	FSkeleton* FbxSkeleton = UFbxLoader::GetInstance().ExtractSkeletonFromFbx(FbxFilePath);
	if (!FbxSkeleton)
	{
		UE_LOG("AnimSequence: Load: Failed to extract skeleton from %s", FbxFilePath.c_str());
		return;
	}

	// 모든 AnimStack 로드
	TArray<UAnimSequence*> AllAnimSequences = UFbxLoader::GetInstance().LoadAllFbxAnimations(FbxFilePath, *FbxSkeleton);

	// 이름이 일치하는 AnimSequence 찾기
	UAnimSequence* FoundAnimSequence = nullptr;
	for (UAnimSequence* AnimSeq : AllAnimSequences)
	{
		if (AnimSeq && AnimSeq->GetName() == AnimStackName)
		{
			FoundAnimSequence = AnimSeq;
			break;
		}
	}

	if (!FoundAnimSequence)
	{
		UE_LOG("AnimSequence: Load: AnimStack '%s' not found in %s", AnimStackName.c_str(), FbxFilePath.c_str());

		// 생성된 AnimSequence들 정리
		for (UAnimSequence* AnimSeq : AllAnimSequences)
		{
			if (AnimSeq)
			{
				ObjectFactory::DeleteObject(AnimSeq);
			}
		}
		delete FbxSkeleton;
		return;
	}

	// DataModel 소유권 이전 (안전한 소유권 이전 패턴 사용)
	TransferDataModelFrom(FoundAnimSequence);
	Name = FoundAnimSequence->GetName();

	// 나머지 AnimSequence들 정리
	for (UAnimSequence* AnimSeq : AllAnimSequences)
	{
		if (AnimSeq)
		{
			ObjectFactory::DeleteObject(AnimSeq);
		}
	}

	// FbxSkeleton 삭제
	// 각 AnimSequence의 DataModel이 Skeleton의 복사본을 소유하므로
	// 원본 FbxSkeleton은 여기서 안전하게 삭제 가능
	delete FbxSkeleton;

	// FilePath와 LastModifiedTime 설정
	// FilePath는 ResourceManager가 이미 설정하지만, 확실하게 하기 위해 다시 설정
	// (ResourceManager::Load()의 213줄에서 SetFilePath 호출)

	// LastModifiedTime 설정 (Hot Reload 지원)
	std::filesystem::path FbxPath(FbxFilePath);
	if (std::filesystem::exists(FbxPath))
	{
		SetLastModifiedTime(std::filesystem::last_write_time(FbxPath));
	}
}

/**
 * @brief .anim 파일에서 AnimSequence 로드
 * @param AnimFilePath .anim 파일 경로
 */
void UAnimSequence::LoadFromAnimFile(const FString& AnimFilePath)
{
	FWindowsBinReader Reader(AnimFilePath);
	if (!Reader.IsOpen())
	{
		UE_LOG("AnimSequence: LoadFromAnimFile: Failed to open %s", AnimFilePath.c_str());
		return;
	}

	// Name 로드
	FString AnimName;
	Serialization::ReadString(Reader, AnimName);
	SetName(AnimName);

	// Notifies 로드
	uint32 NotifyCount = 0;
	Reader << NotifyCount;
	Notifies.resize(NotifyCount);
	for (uint32 i = 0; i < NotifyCount; ++i)
	{
		Reader << Notifies[i];
	}

	// DataModel 로드
	if (!DataModel)
	{
		DataModel = NewObject<UAnimDataModel>();
	}
	Reader << *DataModel;

	Reader.Close();

	// FilePath 설정 (ResourceManager 호환)
	SetFilePath(AnimFilePath);

	// LastModifiedTime 설정
	std::filesystem::path FilePath(AnimFilePath);
	if (std::filesystem::exists(FilePath))
	{
		SetLastModifiedTime(std::filesystem::last_write_time(FilePath));
	}
}

bool UAnimSequence::GetBoneTransformAtTime(const FString& BoneName, float Time, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const
{
	if (!DataModel)
	{
		return false;
	}

	// 본 트랙 찾기
	const FBoneAnimationTrack* Track = DataModel->GetBoneTrackByName(BoneName);
	if (!Track)
	{
		return false;
	}

	// 시간을 프레임으로 변환
	float FrameRate = DataModel->GetFrameRate().AsDecimal();
	float FrameFloat = Time * FrameRate;
	int32 Frame0 = static_cast<int32>(FrameFloat);
	int32 Frame1 = Frame0 + 1;
	float Alpha = FrameFloat - static_cast<float>(Frame0);

	// 프레임 범위 체크
	int32 MaxFrame = DataModel->GetNumberOfFrames() - 1;
	Frame0 = FMath::Clamp(Frame0, 0, MaxFrame);
	Frame1 = FMath::Clamp(Frame1, 0, MaxFrame);

	// 보간
	OutPosition = InterpolatePosition(Track->InternalTrack.PosKeys, Alpha, Frame0, Frame1);
	OutRotation = InterpolateRotation(Track->InternalTrack.RotKeys, Alpha, Frame0, Frame1);
	OutScale = InterpolateScale(Track->InternalTrack.ScaleKeys, Alpha, Frame0, Frame1);

	return true;
}

bool UAnimSequence::GetBoneTransformAtFrame(const FString& BoneName, int32 Frame, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const
{
	if (!DataModel)
	{
		return false;
	}

	// 본 트랙 찾기
	const FBoneAnimationTrack* Track = DataModel->GetBoneTrackByName(BoneName);
	if (!Track)
	{
		return false;
	}

	// 프레임 범위 체크
	int32 MaxFrame = DataModel->GetNumberOfFrames() - 1;
	Frame = FMath::Clamp(Frame, 0, MaxFrame);

	// 키프레임 데이터 가져오기
	if (Frame < Track->InternalTrack.PosKeys.Num())
	{
		OutPosition = Track->InternalTrack.PosKeys[Frame];
	}
	else
	{
		OutPosition = FVector::Zero();
	}

	if (Frame < Track->InternalTrack.RotKeys.Num())
	{
		OutRotation = Track->InternalTrack.RotKeys[Frame];
	}
	else
	{
		OutRotation = FQuat::Identity();
	}

	if (Frame < Track->InternalTrack.ScaleKeys.Num())
	{
		OutScale = Track->InternalTrack.ScaleKeys[Frame];
	}
	else
	{
		OutScale = FVector::One();
	}

	return true;
}

FVector UAnimSequence::InterpolatePosition(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const
{
	if (Keys.Num() == 0)
	{
		return FVector::Zero();
	}

	if (Frame0 >= Keys.Num())
	{
		return Keys[Keys.Num() - 1];
	}

	if (Frame1 >= Keys.Num() || Frame0 == Frame1)
	{
		return Keys[Frame0];
	}

	return FVector::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FQuat UAnimSequence::InterpolateRotation(const TArray<FQuat>& Keys, float Alpha, int32 Frame0, int32 Frame1) const
{
	if (Keys.Num() == 0)
	{
		return FQuat::Identity();
	}

	if (Frame0 >= Keys.Num())
	{
		return Keys[Keys.Num() - 1];
	}

	if (Frame1 >= Keys.Num() || Frame0 == Frame1)
	{
		return Keys[Frame0];
	}

	return FQuat::Slerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FVector UAnimSequence::InterpolateScale(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const
{
	if (Keys.Num() == 0)
	{
		return FVector::One();
	}

	if (Frame0 >= Keys.Num())
	{
		return Keys[Keys.Num() - 1];
	}

	if (Frame1 >= Keys.Num() || Frame0 == Frame1)
	{
		return Keys[Frame0];
	}

	return FVector::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

// ========================================
// Sync Markers 구현
// ========================================

/**
 * @brief Sync Marker 추가
 */
void UAnimSequence::AddSyncMarker(const FString& MarkerName, float Time)
{
	SyncMarkers.Add(FAnimSyncMarker(MarkerName, Time));

	// 시간순으로 정렬
	std::sort(SyncMarkers.begin(), SyncMarkers.end(),
		[](const FAnimSyncMarker& A, const FAnimSyncMarker& B)
		{
			return A.Time < B.Time;
		});
}

/**
 * @brief Sync Marker 제거
 */
void UAnimSequence::RemoveSyncMarker(int32 Index)
{
	if (Index >= 0 && Index < SyncMarkers.Num())
	{
		SyncMarkers.RemoveAt(Index);
	}
}

/**
 * @brief 특정 이름의 Sync Marker 찾기
 */
const FAnimSyncMarker* UAnimSequence::FindSyncMarker(const FString& MarkerName) const
{
	for (const FAnimSyncMarker& Marker : SyncMarkers)
	{
		if (Marker.MarkerName == MarkerName)
		{
			return &Marker;
		}
	}
	return nullptr;
}

/**
 * @brief 주어진 시간 범위 내의 Sync Marker 찾기
 */
void UAnimSequence::FindSyncMarkersInRange(float StartTime, float EndTime, TArray<FAnimSyncMarker>& OutMarkers) const
{
	OutMarkers.Empty();

	for (const FAnimSyncMarker& Marker : SyncMarkers)
	{
		if (Marker.Time >= StartTime && Marker.Time <= EndTime)
		{
			OutMarkers.Add(Marker);
		}
	}
}
