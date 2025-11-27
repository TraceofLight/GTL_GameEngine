#include "pch.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "Source/Editor/FBXLoader.h"
#include "ResourceManager.h"

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
	// .anim 파일인 경우 직접 로드
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
		UE_LOG("AnimSequence: Load: Invalid path format, expected 'FbxPath#AnimStackName' or '.anim' file, got: %s", InFilePath.c_str());
		return;
	}

	FString FbxFilePath = InFilePath.substr(0, HashPos);
	FString AnimStackName = InFilePath.substr(HashPos + 1);

	// FBX에서 Skeleton 추출
	FSkeleton* FbxSkeleton = UFbxLoader::GetInstance().ExtractSkeletonFromFbx(FbxFilePath);
	if (!FbxSkeleton)
	{
		UE_LOG("AnimSequence: Load: Failed to extract skeleton from FBX: %s", FbxFilePath.c_str());
		return;
	}

	// 모든 AnimStack 로드
	TArray<UAnimSequence*> AllAnimSequences = UFbxLoader::GetInstance().LoadAllFbxAnimations(FbxFilePath, *FbxSkeleton);

	// 고유 이름 생성: FBX파일명_AnimStackName (LoadAllFbxAnimations와 동일한 형식)
	std::filesystem::path FbxPath(FbxFilePath);
	FString FbxFileName = FbxPath.stem().string();
	FString ExpectedAnimName = FbxFileName + "_" + AnimStackName;

	// 이름이 일치하는 AnimSequence 찾기
	UAnimSequence* FoundAnimSequence = nullptr;
	for (UAnimSequence* AnimSeq : AllAnimSequences)
	{
		if (AnimSeq && AnimSeq->GetName() == ExpectedAnimName)
		{
			FoundAnimSequence = AnimSeq;
			break;
		}
	}

	if (!FoundAnimSequence)
	{
		UE_LOG("AnimSequence: Load: AnimStack '%s' not found in FBX: %s", AnimStackName.c_str(), FbxFilePath.c_str());

		// 생성된 AnimSequence들 정리 (ResourceManager에서 먼저 제거)
		for (UAnimSequence* AnimSeq : AllAnimSequences)
		{
			if (AnimSeq)
			{
				// ResourceManager에서 제거 (댕글링 포인터 방지)
				const FString& AnimPath = AnimSeq->GetFilePath();
				if (!AnimPath.empty())
				{
					UResourceManager::GetInstance().Unload<UAnimSequence>(AnimPath);
				}
				ObjectFactory::DeleteObject(AnimSeq);
			}
		}
		delete FbxSkeleton;
		return;
	}

	// DataModel 소유권 이전 (안전한 소유권 이전 패턴 사용)
	TransferDataModelFrom(FoundAnimSequence);
	Name = FoundAnimSequence->GetName();

	// 나머지 AnimSequence들 정리 (ResourceManager에서 먼저 제거)
	for (UAnimSequence* AnimSeq : AllAnimSequences)
	{
		if (AnimSeq)
		{
			// ResourceManager에서 제거 (댕글링 포인터 방지)
			const FString& AnimPath = AnimSeq->GetFilePath();
			if (!AnimPath.empty())
			{
				UResourceManager::GetInstance().Unload<UAnimSequence>(AnimPath);
			}
			ObjectFactory::DeleteObject(AnimSeq);
		}
	}

	delete FbxSkeleton;

	// FilePath를 .anim 경로로 설정 (UI에서 .anim만 표시되도록)
	// 형식: FBX경로_AnimStackName.anim
	size_t DotPos = FbxFilePath.find_last_of('.');
	FString AnimFilePath;
	if (DotPos != FString::npos)
	{
		AnimFilePath = FbxFilePath.substr(0, DotPos) + "_" + AnimStackName + ".anim";
	}
	else
	{
		AnimFilePath = FbxFilePath + "_" + AnimStackName + ".anim";
	}
	SetFilePath(AnimFilePath);

	// LastModifiedTime 설정 (Hot Reload 지원)
	// .anim 파일이 있으면 그 시간 사용, 없으면 FBX 시간 사용
	FWideString WAnimFilePath = UTF8ToWide(AnimFilePath);
	std::filesystem::path AnimFilePathObj(WAnimFilePath);
	if (std::filesystem::exists(AnimFilePathObj))
	{
		SetLastModifiedTime(std::filesystem::last_write_time(AnimFilePathObj));
	}
	else if (std::filesystem::exists(FbxPath))
	{
		SetLastModifiedTime(std::filesystem::last_write_time(FbxPath));
	}

	UE_LOG("AnimSequence: Load: Successfully loaded AnimStack '%s', FilePath: %s", AnimStackName.c_str(), AnimFilePath.c_str());
}

void UAnimSequence::LoadFromAnimFile(const FString& AnimFilePath)
{
	try
	{
		// 파일 크기 검증 - 너무 작거나 너무 크면 손상된 것으로 간주
		std::filesystem::path AnimPath(UTF8ToWide(AnimFilePath));
		if (!std::filesystem::exists(AnimPath))
		{
			UE_LOG("AnimSequence: LoadFromAnimFile: File not found: %s", AnimFilePath.c_str());
			return;
		}

		auto FileSize = std::filesystem::file_size(AnimPath);
		if (FileSize < 16 || FileSize > 100 * 1024 * 1024) // 16바이트 ~ 100MB
		{
			UE_LOG("AnimSequence: LoadFromAnimFile: Invalid file size (%llu bytes): %s", FileSize, AnimFilePath.c_str());
			return;
		}

		FWindowsBinReader Reader(AnimFilePath);
		if (!Reader.IsOpen())
		{
			UE_LOG("AnimSequence: LoadFromAnimFile: Failed to open file: %s", AnimFilePath.c_str());
			return;
		}

		// Name 로드
		FString AnimName;
		Serialization::ReadString(Reader, AnimName);

		// Name 유효성 검사
		if (AnimName.empty() || AnimName.size() > 1024)
		{
			UE_LOG("AnimSequence: LoadFromAnimFile: Invalid name in file: %s", AnimFilePath.c_str());
			return;
		}
		Name = AnimName;

		// Notifies 로드
		uint32 NotifyCount = 0;
		Reader << NotifyCount;

		// NotifyCount 유효성 검사 (1000개 이상이면 손상된 것으로 간주)
		if (NotifyCount > 1000)
		{
			UE_LOG("AnimSequence: LoadFromAnimFile: Invalid NotifyCount (%u) in file: %s", NotifyCount, AnimFilePath.c_str());
			Name.clear();
			return;
		}

		Notifies.resize(NotifyCount);
		for (uint32 i = 0; i < NotifyCount; ++i)
		{
			Reader << Notifies[i];
		}

		// SyncMarkers 로드
		uint32 SyncMarkerCount = 0;
		Reader << SyncMarkerCount;

		// SyncMarkerCount 유효성 검사
		if (SyncMarkerCount > 1000)
		{
			UE_LOG("AnimSequence: LoadFromAnimFile: Invalid SyncMarkerCount (%u) in file: %s", SyncMarkerCount, AnimFilePath.c_str());
			Name.clear();
			Notifies.clear();
			return;
		}

		SyncMarkers.clear();
		SyncMarkers.reserve(SyncMarkerCount);
		for (uint32 i = 0; i < SyncMarkerCount; ++i)
		{
			FString MarkerName;
			float MarkerTime = 0.0f;
			Serialization::ReadString(Reader, MarkerName);
			Reader << MarkerTime;
			SyncMarkers.Add(FAnimSyncMarker(MarkerName, MarkerTime));
		}

		// DataModel 로드
		UAnimDataModel* NewDataModel = NewObject<UAnimDataModel>();
		Reader << *NewDataModel;
		SetDataModel(NewDataModel);

		Reader.Close();

		// LastModifiedTime 설정
		std::filesystem::path FilePath(AnimFilePath);
		if (std::filesystem::exists(FilePath))
		{
			SetLastModifiedTime(std::filesystem::last_write_time(FilePath));
		}

		UE_LOG("AnimSequence: LoadFromAnimFile: Successfully loaded '%s' from %s", Name.c_str(), AnimFilePath.c_str());
	}
	catch (const std::exception& e)
	{
		UE_LOG("AnimSequence: LoadFromAnimFile: Exception: %s", e.what());
	}
}

bool UAnimSequence::Save()
{
	FString CurrentPath = GetFilePath();
	FString SavePath;

	// 저장 경로 결정
	if (CurrentPath.ends_with(".anim"))
	{
		// .anim 파일: 기존 경로에 덮어쓰기
		SavePath = CurrentPath;
	}
	else
	{
		// FBX 파일: .anim으로 변환하여 저장
		size_t HashPos = CurrentPath.find('#');
		if (HashPos != FString::npos)
		{
			// "Data/Animation/File.FBX#AnimStack" → "Data/Animation/File.anim"
			FString FbxPath = CurrentPath.substr(0, HashPos);
			size_t DotPos = FbxPath.find_last_of('.');
			if (DotPos != FString::npos)
			{
				SavePath = FbxPath.substr(0, DotPos) + ".anim";
			}
			else
			{
				SavePath = FbxPath + ".anim";
			}
		}
		else
		{
			// Fallback: AnimSequence 이름 사용
			SavePath = "Data/Animation/" + Name + ".anim";
		}
	}

	return SaveToFile(SavePath);
}

bool UAnimSequence::SaveToFile(const FString& SavePath)
{
	if (SavePath.empty())
	{
		UE_LOG("AnimSequence: SaveToFile: Empty save path");
		return false;
	}

	// 디렉토리 생성
	std::filesystem::path FilePathObj(SavePath);
	std::filesystem::path DirPath = FilePathObj.parent_path();
	if (!DirPath.empty() && !std::filesystem::exists(DirPath))
	{
		std::filesystem::create_directories(DirPath);
	}

	try
	{
		FWindowsBinWriter Writer(SavePath);

		// Name 저장
		Serialization::WriteString(Writer, Name);

		// Notifies 저장
		uint32 NotifyCount = static_cast<uint32>(Notifies.Num());
		Writer << NotifyCount;
		for (FAnimNotifyEvent& Notify : Notifies)
		{
			Writer << Notify;
		}

		// SyncMarkers 저장
		uint32 SyncMarkerCount = static_cast<uint32>(SyncMarkers.Num());
		Writer << SyncMarkerCount;
		for (const FAnimSyncMarker& Marker : SyncMarkers)
		{
			Serialization::WriteString(Writer, Marker.MarkerName);
			float Time = Marker.Time;
			Writer << Time;
		}

		// DataModel 저장
		if (DataModel)
		{
			Writer << *DataModel;
		}

		Writer.Close();

		// FilePath 업데이트 (다음 Save는 .anim 경로로)
		SetFilePath(SavePath);

		UE_LOG("AnimSequence: SaveToFile: Saved '%s' to %s", Name.c_str(), SavePath.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG("AnimSequence: SaveToFile: Failed - %s", e.what());
		return false;
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
