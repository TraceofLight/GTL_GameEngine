#pragma once
#include "Object.h"
#include "fbxsdk.h"

class UAnimSequence;
class UAnimDataModel;

class UFbxLoader : public UObject
{
public:

	DECLARE_CLASS(UFbxLoader, UObject)
	static UFbxLoader& GetInstance();
	UFbxLoader();

	static void PreLoad();

	USkeletalMesh* LoadFbxMesh(const FString& FilePath);

	FSkeletalMeshData* LoadFbxMeshAsset(const FString& FilePath);

	// 애니메이션 임포트 (단일 AnimStack)
	// UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton& TargetSkeleton, const FString& AnimStackName = "");

	// 모든 AnimStack 임포트 (각 스택을 개별 AnimSequence로 반환)
	TArray<UAnimSequence*> LoadAllFbxAnimations(const FString& FilePath, const FSkeleton& TargetSkeleton);

	// With Skin FBX에서 Skeleton 추출 (호환성 체크용)
	FSkeleton* ExtractSkeletonFromFbx(const FString& FilePath);


protected:
	~UFbxLoader() override;
private:
	UFbxLoader(const UFbxLoader&) = delete;
	UFbxLoader& operator=(const UFbxLoader&) = delete;


	void LoadMeshFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TMap<FbxSurfaceMaterial*, int32>& MaterialToIndex);

	void LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex);

	void LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData);

	void LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TArray<int32> MaterialSlotToIndex, int32 DefaultMaterialIndex = 0);

	void ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& MaterialInfo);

	FString ParseTexturePath(FbxProperty& Property);

	FbxString GetAttributeTypeName(FbxNodeAttribute* InAttribute);

	void EnsureSingleRootBone(FSkeletalMeshData& MeshData);

	// 애니메이션 임포트 헬퍼 메서드
	void ExtractBoneAnimation(
		FbxNode* BoneNode,
		FbxAnimLayer* AnimLayer,
		const FbxTimeSpan& TimeSpan,
		double FrameRate,
		struct FBoneAnimationTrack& OutTrack
	);

	FVector SamplePosition(FbxNode* Node, FbxAnimLayer* Layer, FbxTime Time);
	FQuat SampleRotation(FbxNode* Node, FbxAnimLayer* Layer, FbxTime Time);
	FVector SampleScale(FbxNode* Node, FbxAnimLayer* Layer, FbxTime Time);

	FbxNode* FindNodeByName(FbxNode* RootNode, const char* NodeName);

	// bin파일 저장용
	TArray<FMaterialInfo> MaterialInfos;
	FbxManager* SdkManager = nullptr;
	/** 현재 로드 중인 FBX 파일의 상위 디렉토리 (UTF-8) */
	FString CurrentFbxBaseDir;
};
