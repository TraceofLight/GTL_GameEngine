#include "pch.h"
#include "MixamoChainMapper.h"
#include <algorithm>

// Static 멤버 초기화
TArray<FString> FMixamoChainMapper::CanonicalBoneNames;
TMap<FString, int32> FMixamoChainMapper::CanonicalBoneIndexMap;
bool FMixamoChainMapper::bIsInitialized = false;

void FMixamoChainMapper::InitializeCanonicalBones()
{
	if (bIsInitialized)
		return;

	// X Bot 기준 표준 Mixamo 본 이름 목록
	CanonicalBoneNames = {
		// Root
		"Hips",

		// Spine
		"Spine",
		"Spine1",
		"Spine2",

		// Neck & Head
		"Neck",
		"Head",
		"HeadTop_End",

		// Left Arm
		"LeftShoulder",
		"LeftArm",
		"LeftForeArm",
		"LeftHand",
		"LeftHandThumb1",
		"LeftHandThumb2",
		"LeftHandThumb3",
		"LeftHandThumb4",
		"LeftHandIndex1",
		"LeftHandIndex2",
		"LeftHandIndex3",
		"LeftHandIndex4",
		"LeftHandMiddle1",
		"LeftHandMiddle2",
		"LeftHandMiddle3",
		"LeftHandMiddle4",
		"LeftHandRing1",
		"LeftHandRing2",
		"LeftHandRing3",
		"LeftHandRing4",
		"LeftHandPinky1",
		"LeftHandPinky2",
		"LeftHandPinky3",
		"LeftHandPinky4",

		// Right Arm
		"RightShoulder",
		"RightArm",
		"RightForeArm",
		"RightHand",
		"RightHandThumb1",
		"RightHandThumb2",
		"RightHandThumb3",
		"RightHandThumb4",
		"RightHandIndex1",
		"RightHandIndex2",
		"RightHandIndex3",
		"RightHandIndex4",
		"RightHandMiddle1",
		"RightHandMiddle2",
		"RightHandMiddle3",
		"RightHandMiddle4",
		"RightHandRing1",
		"RightHandRing2",
		"RightHandRing3",
		"RightHandRing4",
		"RightHandPinky1",
		"RightHandPinky2",
		"RightHandPinky3",
		"RightHandPinky4",

		// Left Leg
		"LeftUpLeg",
		"LeftLeg",
		"LeftFoot",
		"LeftToeBase",
		"LeftToe_End",

		// Right Leg
		"RightUpLeg",
		"RightLeg",
		"RightFoot",
		"RightToeBase",
		"RightToe_End"
	};

	// 인덱스 맵 생성 (빠른 검색용)
	CanonicalBoneIndexMap.clear();
	for (int32 i = 0; i < CanonicalBoneNames.Num(); ++i)
	{
		CanonicalBoneIndexMap[CanonicalBoneNames[i]] = i;
	}

	bIsInitialized = true;
}

const TArray<FString>& FMixamoChainMapper::GetCanonicalBoneNames()
{
	InitializeCanonicalBones();
	return CanonicalBoneNames;
}

FString FMixamoChainMapper::RemovePrefix(const FString& BoneName)
{
	// 콜론(:) 찾기
	size_t ColonPos = BoneName.find(':');

	if (ColonPos != FString::npos)
	{
		// 콜론 뒤의 문자열 반환 (접두사 제거)
		return BoneName.substr(ColonPos + 1);
	}

	// 콜론이 없으면 원본 그대로 반환
	return BoneName;
}

bool FMixamoChainMapper::IsMixamoSkeleton(const FSkeleton& Skeleton)
{
	if (Skeleton.Bones.Num() == 0)
		return false;

	// Mixamo 특징적인 본 이름들 체크
	TArray<FString> RequiredBones = {
		"Hips",
		"Spine",
		"Neck",
		"Head",
		"LeftArm",
		"RightArm",
		"LeftUpLeg",
		"RightUpLeg"
	};

	int32 MatchCount = 0;

	for (const FBone& Bone : Skeleton.Bones)
	{
		FString CanonicalName = RemovePrefix(Bone.Name);

		for (const FString& Required : RequiredBones)
		{
			if (CanonicalName == Required)
			{
				++MatchCount;
				break;
			}
		}
	}

	// 필수 본 중 최소 6개 이상 매칭되면 Mixamo로 간주
	return MatchCount >= 6;
}

bool FMixamoChainMapper::CreateMapping(
	const FSkeleton& SourceSkeleton,
	TArray<int32>& OutCanonicalToSource,
	TArray<int32>& OutSourceToCanonical)
{
	InitializeCanonicalBones();

	// 매핑 배열 초기화 (-1로 초기화)
	OutCanonicalToSource.SetNum(CanonicalBoneNames.Num(), -1);
	OutSourceToCanonical.SetNum(SourceSkeleton.Bones.Num(), -1);

	int32 MappedCount = 0;

	// Source 스켈레톤의 각 본을 Canonical에 매핑
	for (int32 SourceIdx = 0; SourceIdx < SourceSkeleton.Bones.Num(); ++SourceIdx)
	{
		const FBone& SourceBone = SourceSkeleton.Bones[SourceIdx];
		FString CanonicalName = RemovePrefix(SourceBone.Name);

		// Canonical 본 이름 목록에서 찾기
		auto It = CanonicalBoneIndexMap.find(CanonicalName);
		if (It != CanonicalBoneIndexMap.end())
		{
			int32 CanonicalIdx = It->second;

			// 양방향 매핑
			OutCanonicalToSource[CanonicalIdx] = SourceIdx;
			OutSourceToCanonical[SourceIdx] = CanonicalIdx;

			++MappedCount;
		}
	}

	UE_LOG("FMixamoChainMapper: Mapped %d / %d bones to Canonical Skeleton",
		MappedCount, SourceSkeleton.Bones.Num());

	return MappedCount > 0;
}

FSkeleton FMixamoChainMapper::ConvertToCanonicalSkeleton(const FSkeleton& SourceSkeleton)
{
	InitializeCanonicalBones();

	FSkeleton CanonicalSkeleton;

	// 매핑 생성
	TArray<int32> CanonicalToSource;
	TArray<int32> SourceToCanonical;
	CreateMapping(SourceSkeleton, CanonicalToSource, SourceToCanonical);

	// Canonical 본 생성
	CanonicalSkeleton.Bones.Reserve(CanonicalBoneNames.Num());

	for (int32 CanonicalIdx = 0; CanonicalIdx < CanonicalBoneNames.Num(); ++CanonicalIdx)
	{
		FBone CanonicalBone;
		CanonicalBone.Name = CanonicalBoneNames[CanonicalIdx];

		int32 SourceIdx = CanonicalToSource[CanonicalIdx];

		if (SourceIdx != -1)
		{
			// Source에 해당 본이 있으면 BindPose 복사
			const FBone& SourceBone = SourceSkeleton.Bones[SourceIdx];
			CanonicalBone.BindPose = SourceBone.BindPose;
			CanonicalBone.InverseBindPose = SourceBone.InverseBindPose;

			// ParentIndex 재매핑
			if (SourceBone.ParentIndex != -1)
			{
				int32 SourceParentIdx = SourceBone.ParentIndex;
				int32 CanonicalParentIdx = SourceToCanonical[SourceParentIdx];
				CanonicalBone.ParentIndex = CanonicalParentIdx;
			}
			else
			{
				CanonicalBone.ParentIndex = -1;
			}
		}
		else
		{
			// Source에 없는 본 (예: 손가락/발가락 끝)
			// Identity BindPose로 초기화
			CanonicalBone.BindPose = FMatrix::Identity();
			CanonicalBone.InverseBindPose = FMatrix::Identity();

			// 부모 찾기 (알려진 패턴 기반)
			CanonicalBone.ParentIndex = FindParentForMissingBone(CanonicalIdx);
		}

		CanonicalSkeleton.Bones.Add(CanonicalBone);
	}

	// BoneNameToIndex 생성
	CanonicalSkeleton.BoneNameToIndex.clear();
	for (int32 i = 0; i < CanonicalSkeleton.Bones.Num(); ++i)
	{
		CanonicalSkeleton.BoneNameToIndex[CanonicalSkeleton.Bones[i].Name] = i;
	}

	return CanonicalSkeleton;
}

int32 FMixamoChainMapper::FindParentForMissingBone(int32 CanonicalIdx)
{
	InitializeCanonicalBones();

	if (CanonicalIdx < 0 || CanonicalIdx >= CanonicalBoneNames.Num())
		return -1;

	const FString& BoneName = CanonicalBoneNames[CanonicalIdx];

	// 손가락/발가락 끝 패턴 처리
	// 예: LeftHandThumb4 -> LeftHandThumb3
	//     LeftToe_End -> LeftToeBase

	// "4"로 끝나는 경우 (손가락 끝)
	if (BoneName.length() > 0 && BoneName.back() == '4')
	{
		FString ParentName = BoneName;
		ParentName.back() = '3'; // 4 -> 3
		return FindCanonicalBoneIndex(ParentName);
	}

	// "_End"로 끝나는 경우
	if (BoneName.length() > 4 && BoneName.substr(BoneName.length() - 4) == "_End")
	{
		// HeadTop_End -> Head
		if (BoneName == "HeadTop_End")
			return FindCanonicalBoneIndex("Head");

		// LeftToe_End -> LeftToeBase
		if (BoneName == "LeftToe_End")
			return FindCanonicalBoneIndex("LeftToeBase");

		// RightToe_End -> RightToeBase
		if (BoneName == "RightToe_End")
			return FindCanonicalBoneIndex("RightToeBase");
	}

	return -1;
}

bool FMixamoChainMapper::IsValidCanonicalIndex(int32 CanonicalIndex)
{
	InitializeCanonicalBones();
	return CanonicalIndex >= 0 && CanonicalIndex < CanonicalBoneNames.Num();
}

int32 FMixamoChainMapper::FindCanonicalBoneIndex(const FString& CanonicalName)
{
	InitializeCanonicalBones();

	auto It = CanonicalBoneIndexMap.find(CanonicalName);
	if (It != CanonicalBoneIndexMap.end())
	{
		return It->second;
	}

	return -1;
}

FSkeleton* FMixamoChainMapper::CreateCanonicalSkeleton()
{
	InitializeCanonicalBones();

	FSkeleton* Skeleton = new FSkeleton();
	Skeleton->Name = "MixamoCanonicalSkeleton";

	// 부모 관계 맵 (본 이름 -> 부모 본 이름)
	TMap<FString, FString> ParentMap = {
		// Root
		{"Hips", ""},

		// Spine
		{"Spine", "Hips"},
		{"Spine1", "Spine"},
		{"Spine2", "Spine1"},

		// Neck & Head
		{"Neck", "Spine2"},
		{"Head", "Neck"},
		{"HeadTop_End", "Head"},

		// Left Arm
		{"LeftShoulder", "Spine2"},
		{"LeftArm", "LeftShoulder"},
		{"LeftForeArm", "LeftArm"},
		{"LeftHand", "LeftForeArm"},
		{"LeftHandThumb1", "LeftHand"},
		{"LeftHandThumb2", "LeftHandThumb1"},
		{"LeftHandThumb3", "LeftHandThumb2"},
		{"LeftHandThumb4", "LeftHandThumb3"},
		{"LeftHandIndex1", "LeftHand"},
		{"LeftHandIndex2", "LeftHandIndex1"},
		{"LeftHandIndex3", "LeftHandIndex2"},
		{"LeftHandIndex4", "LeftHandIndex3"},
		{"LeftHandMiddle1", "LeftHand"},
		{"LeftHandMiddle2", "LeftHandMiddle1"},
		{"LeftHandMiddle3", "LeftHandMiddle2"},
		{"LeftHandMiddle4", "LeftHandMiddle3"},
		{"LeftHandRing1", "LeftHand"},
		{"LeftHandRing2", "LeftHandRing1"},
		{"LeftHandRing3", "LeftHandRing2"},
		{"LeftHandRing4", "LeftHandRing3"},
		{"LeftHandPinky1", "LeftHand"},
		{"LeftHandPinky2", "LeftHandPinky1"},
		{"LeftHandPinky3", "LeftHandPinky2"},
		{"LeftHandPinky4", "LeftHandPinky3"},

		// Right Arm
		{"RightShoulder", "Spine2"},
		{"RightArm", "RightShoulder"},
		{"RightForeArm", "RightArm"},
		{"RightHand", "RightForeArm"},
		{"RightHandThumb1", "RightHand"},
		{"RightHandThumb2", "RightHandThumb1"},
		{"RightHandThumb3", "RightHandThumb2"},
		{"RightHandThumb4", "RightHandThumb3"},
		{"RightHandIndex1", "RightHand"},
		{"RightHandIndex2", "RightHandIndex1"},
		{"RightHandIndex3", "RightHandIndex2"},
		{"RightHandIndex4", "RightHandIndex3"},
		{"RightHandMiddle1", "RightHand"},
		{"RightHandMiddle2", "RightHandMiddle1"},
		{"RightHandMiddle3", "RightHandMiddle2"},
		{"RightHandMiddle4", "RightHandMiddle3"},
		{"RightHandRing1", "RightHand"},
		{"RightHandRing2", "RightHandRing1"},
		{"RightHandRing3", "RightHandRing2"},
		{"RightHandRing4", "RightHandRing3"},
		{"RightHandPinky1", "RightHand"},
		{"RightHandPinky2", "RightHandPinky1"},
		{"RightHandPinky3", "RightHandPinky2"},
		{"RightHandPinky4", "RightHandPinky3"},

		// Left Leg
		{"LeftUpLeg", "Hips"},
		{"LeftLeg", "LeftUpLeg"},
		{"LeftFoot", "LeftLeg"},
		{"LeftToeBase", "LeftFoot"},
		{"LeftToe_End", "LeftToeBase"},

		// Right Leg
		{"RightUpLeg", "Hips"},
		{"RightLeg", "RightUpLeg"},
		{"RightFoot", "RightLeg"},
		{"RightToeBase", "RightFoot"},
		{"RightToe_End", "RightToeBase"}
	};

	// 본 생성
	Skeleton->Bones.Reserve(CanonicalBoneNames.Num());

	for (const FString& BoneName : CanonicalBoneNames)
	{
		FBone Bone;
		Bone.Name = BoneName;

		// 부모 찾기
		auto ParentIt = ParentMap.find(BoneName);
		if (ParentIt != ParentMap.end() && !ParentIt->second.empty())
		{
			FString ParentName = ParentIt->second;
			Bone.ParentIndex = FindCanonicalBoneIndex(ParentName);
		}
		else
		{
			Bone.ParentIndex = -1; // 루트 본
		}

		// Identity BindPose
		Bone.BindPose = FMatrix::Identity();
		Bone.InverseBindPose = FMatrix::Identity();

		Skeleton->Bones.Add(Bone);
	}

	// BoneNameToIndex 생성
	Skeleton->BoneNameToIndex.clear();
	for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
	{
		Skeleton->BoneNameToIndex[Skeleton->Bones[i].Name] = i;
	}

	// 캐시 데이터 초기화
	Skeleton->InitializeCachedData();

	return Skeleton;
}
