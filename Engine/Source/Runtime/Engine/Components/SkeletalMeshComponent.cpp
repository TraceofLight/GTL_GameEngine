#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "ClothComponent.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/Engine/Animation/AnimStateMachine.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAssetUtils.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodyInstance.h"
#include "Source/Runtime/Engine/PhysicsEngine/ConstraintInstance.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsConstraintSetup.h"
#include "SceneView.h"
#include "MeshBatchElement.h"

USkeletalMeshComponent::USkeletalMeshComponent()
    : AnimInstance(nullptr)
    , TestTime(0.0f)
    , bIsInitialized(false)
{
    // Enable component tick for animation updates
    bCanEverTick = true;

    // 테스트용 기본 메시 설정 제거 (메모리 누수 방지)
    // SetSkeletalMesh(GDataDir + "/Test.fbx");
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    // Physics cleanup
    OnDestroyPhysicsState();

    // AnimInstance 정리 (메모리 누수 방지)
    if (AnimInstance)
    {
        ObjectFactory::DeleteObject(AnimInstance);
        AnimInstance = nullptr;
    }

    // InternalClothComponent 정리 (메모리 누수 방지)
    DestroyInternalClothComponent();
}

void USkeletalMeshComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// AnimInstance 초기화 (AnimBlueprint와 AnimToPlay는 상호 배타적)
	if (AnimBlueprint)
	{
		// AnimBlueprint 모드: AnimStateMachine 기반 AnimInstance 생성
		if (!AnimInstance)
		{
			UAnimInstance* NewAnimInstance = NewObject<UAnimInstance>();
			SetAnimInstance(NewAnimInstance);
		}

		if (AnimInstance)
		{
			AnimInstance->SetStateMachine(AnimBlueprint);
		}
	}
	else if (AnimationData.AnimToPlay)
	{
		// SingleNode 모드: AnimationData로부터 AnimInstance 자동 생성
		if (!AnimInstance)
		{
			UAnimSingleNodeInstance* SingleNodeInstance = NewObject<UAnimSingleNodeInstance>();
			SetAnimInstance(SingleNodeInstance);
		}

		// AnimationData 설정 적용
		if (UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(AnimInstance))
		{
			AnimationData.Initialize(SingleNodeInstance);
		}
	}
}

void USkeletalMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    // Collect all as base
    const int32 BeginIndex = OutMeshBatchElements.Num();
    Super::CollectMeshBatches(OutMeshBatchElements, View);

    // If we have cloth sections and an internal cloth component, remove those sections from this component's rendering
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
    {
        return;
    }

    if (!HasClothSections() || InternalClothComponent == nullptr)
    {
        return;
    }

    const auto& GroupInfos = SkeletalMesh->GetSkeletalMeshData()->GroupInfos;
    // Build a quick lookup for cloth groups by (StartIndex,IndexCount)
    struct Key { uint32 a; uint32 b; };
    TSet<uint64> ClothKeys;
    for (const auto& G : GroupInfos)
    {
        if (G.bEnableCloth && G.IndexCount > 0)
        {
            uint64 k = (uint64(G.StartIndex) << 32) | uint64(G.IndexCount);
            ClothKeys.Add(k);
        }
    }

    for (int32 i = OutMeshBatchElements.Num() - 1; i >= BeginIndex; --i)
    {
        const FMeshBatchElement& E = OutMeshBatchElements[i];
        uint64 k = (uint64(E.StartIndex) << 32) | uint64(E.IndexCount);
        if (ClothKeys.Contains(k))
        {
            OutMeshBatchElements.RemoveAt(i);
        }
    }
}

void USkeletalMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AnimInstance)
	{
		AnimInstance->NativeBeginPlay();
	}

	// Constraint 생성 (OnCreatePhysicsState에서 지연됨)
	// PVD가 Body들을 완전히 등록한 후에 Joint를 생성해야 PVD Assert 방지
	if (bPendingConstraintCreation)
	{
		CreateConstraints();
		bPendingConstraintCreation = false;
	}

	// bSimulatePhysics가 true면 래그돌 자동 활성화
	if (bSimulatePhysics && !Bodies.IsEmpty())
	{
		SetAllBodiesSimulatePhysics(true);
	}
}

/**
 * @brief Animation 인스턴스 설정
 */
void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InAnimInstance)
{
    // 기존 AnimInstance 삭제 (메모리 누수 방지)
    if (AnimInstance && AnimInstance != InAnimInstance)
    {
        ObjectFactory::DeleteObject(AnimInstance);
        AnimInstance = nullptr;
    }

    AnimInstance = InAnimInstance;
    if (AnimInstance)
    {
        AnimInstance->Initialize(this);
    }
}

/**
 * @brief AnimationMode 설정 (상호 배타적 처리)
 */
void USkeletalMeshComponent::SetAnimationMode(EAnimationMode NewMode)
{
    if (AnimationMode == NewMode)
    {
        return;
    }

    AnimationMode = NewMode;

    // 상호 배타적 처리
    if (NewMode == EAnimationMode::AnimationBlueprint)
    {
        // AnimationBlueprint 모드로 변경 시 AnimationData 초기화
        AnimationData = FSingleAnimationPlayData();
    }
    else if (NewMode == EAnimationMode::AnimationSingleNode)
    {
        // AnimationSingleNode 모드로 변경 시 AnimBlueprint null
        AnimBlueprint = nullptr;
    }
}

/**
 * @brief Animation Notify 처리 (AnimInstance에서 호출)
 * @param Notify 트리거된 Notify 이벤트
 */
void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    // 소유 액터에게 Notify 전달
    AActor* OwnerActor = GetOwner();
    if (OwnerActor)
    {
        OwnerActor->HandleAnimNotify(Notify);
    }
}

void USkeletalMeshComponent::TriggerAnimNotify(const FString& NotifyName, float TriggerTime, float Duration)
{
    FAnimNotifyEvent Event;
    Event.NotifyName = FName(NotifyName);
    Event.TriggerTime = TriggerTime;
    Event.Duration = Duration;
    OnAnimNotify.Broadcast(Event);
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    // Per-bone physics를 사용할 때는 base BodyInstance sync를 건너뛰어야 함
    // (PrimitiveComponent::TickComponent가 빈 BodyInstance를 sync하면 안 됨)
    if (bSimulatePhysics && !Bodies.IsEmpty())
    {
        // Super::TickComponent를 호출하지 않고 필요한 것만 직접 처리
        USceneComponent::TickComponent(DeltaTime);  // SceneComponent의 tick만 호출

        // Per-bone physics sync
        SyncBonesFromPhysics();

        // Cloth 시뮬레이션 (Physics 중에도 실행)
        if (InternalClothComponent)
        {
            InternalClothComponent->SetWorldTransform(GetWorldTransform());
            InternalClothComponent->TickComponent(DeltaTime);
        }

        return;  // 물리 시뮬레이션 중에는 애니메이션 업데이트 스킵
    }

    USceneComponent::TickComponent(DeltaTime);

    // bSimulatePhysics 캐시 동기화
    if (bSimulatePhysics != bSimulatePhysics_Cached)
    {
        bSimulatePhysics_Cached = bSimulatePhysics;
        SetAllBodiesSimulatePhysics(bSimulatePhysics);
    }

	// Ragdoll 모드일 때는 물리 시뮬레이션 결과를 본에 동기화
	if (bRagdollActive)
	{
		SyncPhysicsToBones();
		return;  // Ragdoll 모드에서는 애니메이션 업데이트 스킵
	}

    // Animation 인스턴스 업데이트
    // 기즈모로 본을 편집 중일 때도 애니메이션은 계속 진행됨
    // 편집 중인 본만 AnimInstance가 덮어쓰지 않도록 처리
    if (AnimInstance)
    {
        // BlendSpace2D 노드 우선 체크 (더 우선순위가 높음)
        FAnimNode_BlendSpace2D* BlendSpace2DNode = AnimInstance->GetBlendSpace2DNode();
        if (BlendSpace2DNode && BlendSpace2DNode->GetBlendSpace())
        {
            // BlendSpace2D 노드 업데이트
            BlendSpace2DNode->Update(DeltaTime);

            // BlendSpace2D에서 계산된 포즈를 가져와 적용
            FPoseContext PoseContext;

            // Skeleton으로 PoseContext 초기화
            if (SkeletalMesh && SkeletalMesh->GetSkeleton())
            {
                PoseContext.Initialize(SkeletalMesh->GetSkeleton());
            }

            BlendSpace2DNode->Evaluate(PoseContext);

            // 계산된 포즈 적용
            if (PoseContext.IsValid())
            {
                PoseContext.EnsureComponentSpaceValid();
                SetPose(PoseContext.LocalSpacePose, PoseContext.ComponentSpacePose);
            }
        }
        // BlendSpace2D가 없으면 State Machine 체크
        else
        {
            FAnimNode_StateMachine* StateMachineNode = AnimInstance->GetStateMachineNode();
            if (StateMachineNode && StateMachineNode->GetStateMachine())
            {
                // State Machine 노드 업데이트
                StateMachineNode->Update(DeltaTime);

                // State Machine에서 계산된 포즈를 가져와 적용
                FPoseContext PoseContext;

                // Skeleton으로 PoseContext 초기화 (CRITICAL FIX)
                if (SkeletalMesh && SkeletalMesh->GetSkeleton())
                {
                    PoseContext.Initialize(SkeletalMesh->GetSkeleton());
                }

                StateMachineNode->Evaluate(PoseContext);

                // 계산된 포즈 적용
                if (PoseContext.IsValid())
                {
                    PoseContext.EnsureComponentSpaceValid();
                    SetPose(PoseContext.LocalSpacePose, PoseContext.ComponentSpacePose);
                }
            }
            else
            {
                // State Machine도 없으면 기본 AnimInstance 업데이트
                // (SingleNodeInstance의 경우 UpdateAnimation + Evaluate 필요)
                AnimInstance->UpdateAnimation(DeltaTime);
                AnimInstance->EvaluateAnimation();
            }
        }
    }

    // 키네마틱 업데이트: Bodies가 있고, 물리 시뮬레이션이 아닐 때 본 트랜스폼을 물리 바디에 푸시
    if (!Bodies.IsEmpty() && !bSimulatePhysics && !bRagdollActive)
    {
        UpdateKinematicBonesToPhysics();
    }

    // Cloth 시뮬레이션 (AnimInstance 업데이트 이후)
    if (InternalClothComponent)
    {
        // ClothComponent의 Transform을 SkeletalMeshComponent와 동기화
        InternalClothComponent->SetWorldTransform(GetWorldTransform());

        // Cloth 시뮬레이션 실행
        InternalClothComponent->TickComponent(DeltaTime);
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    // 비동기 로드 시작 - 완료 시 OnSkeletalMeshLoaded 호출됨
    Super::SetSkeletalMesh(PathFileName);
}

void USkeletalMeshComponent::OnSkeletalMeshLoaded()
{
    Super::OnSkeletalMeshLoaded();

    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
        TempFinalSkinningNormalMatrices.Empty();
        return;
    }

    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    CurrentLocalSpacePose.SetNum(NumBones);
    CurrentComponentSpacePose.SetNum(NumBones);
    TempFinalSkinningMatrices.SetNum(NumBones);
    TempFinalSkinningNormalMatrices.SetNum(NumBones);

    for (int32 i = 0; i < NumBones; ++i)
    {
        const FBone& ThisBone = Skeleton.Bones[i];
        const int32 ParentIndex = ThisBone.ParentIndex;
        FMatrix LocalBindMatrix;

        if (ParentIndex == -1) // 루트 본
        {
            LocalBindMatrix = ThisBone.BindPose;
        }
        else // 자식 본
        {
            const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
            LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
        }
        // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
        CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix);
    }

    ForceRecomputePose();

    // Cloth Section 감지 및 자동 생성
    if (HasClothSections())
    {
        CreateInternalClothComponent();
        UE_LOG("SkeletalMeshComponent: Cloth sections detected. ClothComponent created automatically.\n");
    }
    else
    {
        // Cloth Section이 없으면 기존 ClothComponent 제거
        DestroyInternalClothComponent();
    }
}

/**
 * @brief 본 이름으로 본 인덱스 찾기
 * @param BoneName 찾을 본 이름
 * @return 본 인덱스 (찾지 못하면 -1)
 */
int32 USkeletalMeshComponent::GetBoneIndex(const FName& BoneName) const
{
	if (!SkeletalMesh)
	{
		return -1;
	}

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (!Skeleton)
	{
		return -1;
	}

	auto It = Skeleton->BoneNameToIndex.find(BoneName.ToString());
	if (It != Skeleton->BoneNameToIndex.end())
	{
		return It->second;
	}

	return -1;
}

/**
 * @brief 특정 뼈의 부모 기준 로컬 트랜스폼을 설정
 * @param BoneIndex 수정할 뼈의 인덱스
 * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
 */
void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

/**
 * @brief 본 로컬 트랜스폼을 설정 (ForceRecomputePose 호출 안 함 - 배치 업데이트용)
 * @param BoneIndex 수정할 뼈의 인덱스
 * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
 */
void USkeletalMeshComponent::SetBoneLocalTransformDirect(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
    }
}

/**
 * @brief 본 트랜스폼 갱신 (AnimInstance에서 배치 업데이트 후 호출)
 */
void USkeletalMeshComponent::RefreshBoneTransforms()
{
    ForceRecomputePose();
}

/**
 * @brief Pose 전체 설정 (BlendSpace2D 등에서 사용)
 */
void USkeletalMeshComponent::SetPose(const TArray<FTransform>& InLocalSpacePose, const TArray<FTransform>& InComponentSpacePose)
{
	CurrentLocalSpacePose = InLocalSpacePose;
	CurrentComponentSpacePose = InComponentSpacePose;
	RefreshBoneTransforms();
}

/**
 * @brief Reference Pose(T-Pose)로 리셋
 */
void USkeletalMeshComponent::ResetToReferencePose()
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
        return;

    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    if (CurrentLocalSpacePose.Num() != NumBones)
        return;

    for (int32 i = 0; i < NumBones; ++i)
    {
        const FBone& ThisBone = Skeleton.Bones[i];
        const int32 ParentIndex = ThisBone.ParentIndex;
        FMatrix LocalBindMatrix;

        if (ParentIndex == -1) // 루트 본
        {
            LocalBindMatrix = ThisBone.BindPose;
        }
        else // 자식 본
        {
            const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
            LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
        }
        // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
        CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix);
    }

    ForceRecomputePose();
}

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    // 부모 본의 월드 트랜스폼 계산
    FTransform ParentWorldTransform;
    if (ParentIndex >= 0)
    {
        // 일반 본: 부모 본의 월드 트랜스폼 사용
        ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    }
    else
    {
        // 루트 본: Component의 월드 트랜스폼을 부모로 사용
        ParentWorldTransform = GetWorldTransform();
    }

    // World → Local 변환
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
}

/**
 * @brief 특정 뼈의 현재 로컬 트랜스폼을 반환
 */
FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

/**
 * @brief 기즈모를 렌더링하기 위해 특정 뼈의 월드 트랜스폼을 계산
 */
FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        return GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

/**
 * @brief CurrentLocalSpacePose의 변경사항을 ComponentSpace -> FinalMatrices 계산까지 모두 수행
 */
void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData()) { return; }

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();
    // ComponentSpace -> Final Skinning Matrices 계산
    UpdateFinalSkinningMatrices();
    UpdateSkinningMatrices(TempFinalSkinningMatrices, TempFinalSkinningNormalMatrices);
}

/**
 * @brief CurrentLocalSpacePose를 기반으로 CurrentComponentSpacePose 채우기
 */
void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

/**
 * @brief CurrentComponentSpacePose를 기반으로 TempFinalSkinningMatrices 채우기
 */
void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
        TempFinalSkinningNormalMatrices[BoneIndex] = TempFinalSkinningMatrices[BoneIndex].Inverse().Transpose();
    }
}

// ===== Phase 4: 애니메이션 편의 메서드 구현 =====

/**
 * @brief 애니메이션 재생 (간편 메서드)
 */
void USkeletalMeshComponent::PlayAnimation(UAnimSequence* AnimToPlay, bool bLooping)
{
    if (!AnimToPlay)
    {
        return;
    }

    // AnimSingleNodeInstance 생성 (없으면)
    UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(AnimInstance);
    if (!SingleNodeInstance)
    {
        SingleNodeInstance = NewObject<UAnimSingleNodeInstance>();
        SetAnimInstance(SingleNodeInstance);
    }

    // 애니메이션 설정 및 재생
    SingleNodeInstance->SetAnimationAsset(AnimToPlay);
    SingleNodeInstance->Play(bLooping);
}

/**
 * @brief 애니메이션 정지
 */
void USkeletalMeshComponent::StopAnimation()
{
    if (AnimInstance)
    {
        AnimInstance->StopAnimation();
    }
}

/**
 * @brief BlendSpace2D 설정 (AnimInstance를 통해)
 */
void USkeletalMeshComponent::SetBlendSpace2D(UBlendSpace2D* InBlendSpace)
{
    // AnimInstance가 없으면 생성
    if (!AnimInstance)
    {
        AnimInstance = NewObject<UAnimInstance>();
        AnimInstance->Initialize(this);
        UE_LOG("[SkeletalMeshComponent] AnimInstance created for BlendSpace2D");
    }

    if (AnimInstance)
    {
        AnimInstance->SetBlendSpace2D(InBlendSpace);
    }
}

/**
 * @brief 컴포넌트 복제 시 AnimInstance 상태도 복제
 */
void USkeletalMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // Tick 활성화 (복제 시 유지되지 않으므로 명시적으로 설정)
    bCanEverTick = true;

    // AnimInstance가 있으면 삭제 (BeginPlay에서 재생성)
    if (AnimInstance)
    {
        ObjectFactory::DeleteObject(AnimInstance);
        AnimInstance = nullptr;
    }

    // AnimationMode에 따라 AnimInstance는 BeginPlay에서 재생성됨
    // AnimationData와 AnimBlueprint는 얕은 복사로 이미 복사되었음

    // PIE 복제 시 Pose 배열 초기화 (TArray는 얕은 복사로 빈 배열이 됨)
    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);
        TempFinalSkinningNormalMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1)
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix);
        }

        ForceRecomputePose();
    }
}

/**
 * @brief 컴포넌트 직렬화 시 AnimInstance 상태도 저장/로드
 */
void USkeletalMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // AnimBlueprint 로드 (먼저)
        FString AnimBlueprintPath;
        FJsonSerializer::ReadString(InOutHandle, "AnimBlueprintPath", AnimBlueprintPath, "");
        if (!AnimBlueprintPath.empty())
        {
            AnimBlueprint = RESOURCE.Load<UAnimStateMachine>(AnimBlueprintPath);
        }

        // AnimationData 로드 (먼저)
        FString AnimToPlayPath;
        FJsonSerializer::ReadString(InOutHandle, "AnimToPlayPath", AnimToPlayPath, "");
        FJsonSerializer::ReadBool(InOutHandle, "bSavedLooping", AnimationData.bSavedLooping, true);
        FJsonSerializer::ReadBool(InOutHandle, "bSavedPlaying", AnimationData.bSavedPlaying, true);
        FJsonSerializer::ReadFloat(InOutHandle, "SavedPosition", AnimationData.SavedPosition, 0.0f);
        FJsonSerializer::ReadFloat(InOutHandle, "SavedPlayRate", AnimationData.SavedPlayRate, 1.0f);

        if (!AnimToPlayPath.empty())
        {
            AnimationData.AnimToPlay = RESOURCE.Load<UAnimSequence>(AnimToPlayPath);
        }

        // AnimationMode 로드 (마지막) - 직접 설정 (Setter 사용 안 함)
        int32 ModeValue = static_cast<int32>(EAnimationMode::AnimationSingleNode);
        FJsonSerializer::ReadInt32(InOutHandle, "AnimationMode", ModeValue, static_cast<int32>(EAnimationMode::AnimationSingleNode));
        AnimationMode = static_cast<EAnimationMode>(ModeValue);

        // AnimInstance는 BeginPlay에서 AnimationMode에 따라 생성됨
        // (UAnimInstance vs UAnimSingleNodeInstance)
        // Serialize에서는 애니메이션 데이터만 로드하고 AnimInstance는 생성하지 않음
    }
    else
    {
        // AnimationMode 저장
        InOutHandle["AnimationMode"] = static_cast<uint8>(AnimationMode);

        // AnimBlueprint 저장 (AnimationBlueprint 모드용)
        if (AnimBlueprint)
        {
            InOutHandle["AnimBlueprintPath"] = AnimBlueprint->GetFilePath();
        }
        else
        {
            InOutHandle["AnimBlueprintPath"] = FString("");
        }

        // AnimationData 저장 (AnimationSingleNode 모드용)
        if (AnimationData.AnimToPlay)
        {
            InOutHandle["AnimToPlayPath"] = AnimationData.AnimToPlay->GetFilePath();
        }
        else
        {
            InOutHandle["AnimToPlayPath"] = FString("");
        }
        InOutHandle["bSavedLooping"] = AnimationData.bSavedLooping;
        InOutHandle["bSavedPlaying"] = AnimationData.bSavedPlaying;
        InOutHandle["SavedPosition"] = AnimationData.SavedPosition;
        InOutHandle["SavedPlayRate"] = AnimationData.SavedPlayRate;

        // AnimInstance는 저장하지 않음 (BeginPlay에서 재생성됨)
    }
}

// ===== Bone Delta 관리 =====

void USkeletalMeshComponent::SetBoneDelta(int32 BoneIndex, const FTransform& Delta)
{
    EditedBoneDeltas[BoneIndex] = Delta;
}

void USkeletalMeshComponent::ClearBoneDelta(int32 BoneIndex)
{
    EditedBoneDeltas.erase(BoneIndex);
}

void USkeletalMeshComponent::ClearAllBoneDeltas()
{
    EditedBoneDeltas.clear();
}

bool USkeletalMeshComponent::HasBoneDelta(int32 BoneIndex) const
{
    return EditedBoneDeltas.find(BoneIndex) != EditedBoneDeltas.end();
}

const FTransform* USkeletalMeshComponent::GetBoneDelta(int32 BoneIndex) const
{
    auto It = EditedBoneDeltas.find(BoneIndex);
    if (It != EditedBoneDeltas.end())
    {
        return &It->second;
    }
    return nullptr;
}

// ===== Physics 관리 =====

void USkeletalMeshComponent::OnCreatePhysicsState()
{
    Super::OnCreatePhysicsState();

    // CRITICAL: AVehicleActor는 PhysicsAsset을 사용하지 않음
    // PhysX Vehicle은 단일 RigidDynamic에 모든 Shape을 붙여야 함
    AActor* OwnerActor = GetOwner();
    if (OwnerActor && OwnerActor->GetClass()->Name &&
        strcmp(OwnerActor->GetClass()->Name, "AVehicleActor") == 0)
    {
        return;
    }

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        UE_LOG("Physics: SkeletalMeshComponent::OnCreatePhysicsState: No skeletal mesh");
        return;
    }

	// 비동기 로드 중이면 CurrentComponentSpacePose가 아직 초기화되지 않았을 수 있음
	if (CurrentComponentSpacePose.IsEmpty())
	{
		UE_LOG("Physics: SkeletalMeshComponent::OnCreatePhysicsState: Pose not initialized yet (async load in progress?)");
		return;
	}

    if (!PHYSICS.GetPhysics())
    {
        UE_LOG("Physics: SkeletalMeshComponent::OnCreatePhysicsState: PhysX not ready");
        return;
    }

    // Get physics asset from skeletal mesh (auto-generated if not set)
    UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();

    if (!PhysicsAsset || PhysicsAsset->BodySetups.IsEmpty())
    {
        UE_LOG("Physics: SkeletalMeshComponent::OnCreatePhysicsState: No PhysicsAsset or empty");
        return;
    }

	UWorld* CompWorld = GetWorld();
	PxScene* CompScene = CompWorld ? CompWorld->GetPhysicsScene() : nullptr;
	if (!CompScene)
	{
		return;
	}

    // Clean up any existing bodies
    OnDestroyPhysicsState();

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    const TArray<FBone>& Bones = Skeleton->Bones;

    for (UBodySetup* BoneSetup : PhysicsAsset->BodySetups)
    {
        if (!BoneSetup) continue;

        int32 BoneIdx = -1;
        for (int32 i = 0; i < Bones.Num(); ++i)
        {
            if (FName(Bones[i].Name) == BoneSetup->BoneName)
            {
                BoneIdx = i;
                break;
            }
        }

        if (BoneIdx < 0)
        {
            UE_LOG("Physics: OnCreatePhysicsState: Could not find bone '%s' for physics body",
                   BoneSetup->BoneName.ToString().c_str());
            // 인덱스 정렬 유지를 위해 nullptr 추가 (FindBodyIndexByBoneName과 Bodies 배열 인덱스 일치 필요)
            Bodies.Add(nullptr);
            continue;
        }

        // CurrentComponentSpacePose가 초기화되지 않았으면 스킵
        if (CurrentComponentSpacePose.IsEmpty() || BoneIdx >= CurrentComponentSpacePose.Num())
        {
            UE_LOG("Physics: OnCreatePhysicsState: CurrentComponentSpacePose not initialized (BoneIdx=%d, Size=%d)",
                   BoneIdx, CurrentComponentSpacePose.Num());
            Bodies.Add(nullptr);
            continue;
        }

        FMatrix BoneWorldMatrix = CurrentComponentSpacePose[BoneIdx].ToMatrix() * GetWorldMatrix();

        FBodyInstance* BoneBody = new FBodyInstance(this);
        BoneBody->BodySetup = BoneSetup;

        // Ragdoll용 Body는 항상 Dynamic으로 생성 (Joint 생성을 위해 필수)
        // Static 액터 간에는 Joint를 생성할 수 없음
        BoneBody->CreateActor(PHYSICS.GetPhysics(), BoneWorldMatrix, true);  // Always dynamic
        BoneBody->CreateShapesFromBodySetup();

        // Self-Collision 방지용 FilterData 설정
        // word2에 이 컴포넌트의 주소를 저장하여 같은 래그돌 내 Body들끼리 충돌 무시
        PxRigidActor* Actor = BoneBody->GetPhysicsActor();
        if (Actor)
        {
            PxFilterData FilterData;
            // Ragdoll bodies use DYNAMIC collision group
            FilterData.word0 = ECollisionGroup::Dynamic;
            FilterData.word1 = ECollisionGroup::GroundAndDynamic;
            FilterData.word2 = static_cast<PxU32>(reinterpret_cast<uintptr_t>(this) & 0xFFFFFFFF);  // Ragdoll Owner ID
            FilterData.word3 = 0;

            const PxU32 NumShapes = Actor->getNbShapes();
            TArray<PxShape*> ShapeArray;
            ShapeArray.SetNum(NumShapes);
            Actor->getShapes(ShapeArray.GetData(), NumShapes);
            for (PxShape* Shape : ShapeArray)
            {
                if (Shape)
                {
                    Shape->setSimulationFilterData(FilterData);
                }
            }
        }

        // bSimulatePhysics가 false면 Kinematic으로 설정 (애니메이션 따라감)
        if (!bSimulatePhysics)
        {
            PxRigidDynamic* DynActor = BoneBody->GetPhysicsActor()->is<PxRigidDynamic>();
            if (DynActor)
            {
                DynActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
            }
        }

        // Add to scene
        if (GetWorld() && GetWorld()->GetPhysicsScene())
        {
            BoneBody->AddToScene(GetWorld()->GetPhysicsScene());
        }

        Bodies.Add(BoneBody);
        BodyToBoneIndex.Add(BoneIdx);  // 캐시: Bodies[i]에 해당하는 본 인덱스 저장
    }

	// Constraint 생성은 BeginPlay로 지연 (PVD가 Body들을 완전히 등록한 후 생성)
	// Body가 2개 이상일 때만 Constraint 생성 필요
	if (Bodies.Num() > 1)
	{
		bPendingConstraintCreation = true;
	}
}

void USkeletalMeshComponent::CreateConstraints()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
	{
		return;
	}

	UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	// PhysicsAsset에 ConstraintSetup이 없으면 자동 생성
	if (PhysicsAsset->ConstraintSetups.IsEmpty() && Bodies.Num() > 1)
	{
		FPhysicsAssetUtils::CreateConstraintsForRagdoll(PhysicsAsset, SkeletalMesh);
	}

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const TArray<FBone>& Bones = Skeleton->Bones;

	// Constraint 인스턴스 생성
	for (UPhysicsConstraintSetup* Setup : PhysicsAsset->ConstraintSetups)
	{
		if (!Setup)
		{
			continue;
		}

		// Body Index 재계산 (런타임에 달라질 수 있음)
		int32 BodyIdx1 = PhysicsAsset->FindBodyIndexByBoneName(Setup->ConstraintBone1);
		int32 BodyIdx2 = PhysicsAsset->FindBodyIndexByBoneName(Setup->ConstraintBone2);

		if (BodyIdx1 == -1 || BodyIdx2 == -1)
		{
			continue;
		}

		if (BodyIdx1 >= Bodies.Num() || BodyIdx2 >= Bodies.Num())
		{
			continue;
		}

		FBodyInstance* BodyA = Bodies[BodyIdx1];
		FBodyInstance* BodyB = Bodies[BodyIdx2];

		if (!BodyA || !BodyB || !BodyA->IsValid() || !BodyB->IsValid())
		{
			continue;
		}

		// ===== 런타임에 Joint 위치 재계산 (CurrentComponentSpacePose 사용) =====
		// Body 생성 시와 동일한 좌표계 사용해야 Joint가 올바른 위치에 생성됨

		// Bone1(Ancestor), Bone2(Child)의 본 인덱스 찾기
		int32 AncestorBoneIdx = -1;
		int32 ChildBoneIdx = -1;
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			if (FName(Bones[i].Name) == Setup->ConstraintBone1)
			{
				AncestorBoneIdx = i;
			}
			if (FName(Bones[i].Name) == Setup->ConstraintBone2)
			{
				ChildBoneIdx = i;
			}
		}

		if (AncestorBoneIdx >= 0 && ChildBoneIdx >= 0 &&
			AncestorBoneIdx < CurrentComponentSpacePose.Num() &&
			ChildBoneIdx < CurrentComponentSpacePose.Num())
		{
			// CurrentComponentSpacePose에서 런타임 Transform 가져오기
			const FTransform& AncestorTransform = CurrentComponentSpacePose[AncestorBoneIdx];
			const FTransform& ChildTransform = CurrentComponentSpacePose[ChildBoneIdx];

			// Child의 Component Space 위치를 Ancestor의 로컬 좌표계로 변환
			FVector ChildWorldPos = ChildTransform.Translation;
			FVector AncestorWorldPos = AncestorTransform.Translation;
			FVector Offset = ChildWorldPos - AncestorWorldPos;
			FVector ChildPosInAncestorLocal = AncestorTransform.Rotation.Inverse().RotateVector(Offset);

			// Setup의 Joint 위치 업데이트 (런타임 포즈 기반)
			Setup->ConstraintPositionInBody1 = ChildPosInAncestorLocal;
			Setup->ConstraintPositionInBody2 = FVector(0, 0, 0);  // Child body의 원점
		}

		// Constraint Instance 생성
		FConstraintInstance* NewConstraint = new FConstraintInstance();
		if (NewConstraint->CreateJoint(PHYSICS.GetPhysics(), BodyA, BodyB, Setup))
		{
			Constraints.Add(NewConstraint);
		}
		else
		{
			delete NewConstraint;
		}
	}
}

void USkeletalMeshComponent::OnDestroyPhysicsState()
{
	// Constraint 먼저 해제 (Body보다 먼저)
	for (FConstraintInstance* Constraint : Constraints)
	{
		if (Constraint)
		{
			Constraint->DestroyJoint();
			delete Constraint;
		}
	}
	Constraints.Empty();

	// Body 해제
    for (FBodyInstance* Body : Bodies)
    {
        if (Body)
        {
            Body->TermBody();
            delete Body;
        }
    }
    Bodies.Empty();
    BodyToBoneIndex.Empty();  // 캐시도 함께 비우기

	bRagdollActive = false;
	bPendingConstraintCreation = false;
}

// ===== Ragdoll 기능 구현 =====

void USkeletalMeshComponent::SetAllBodiesSimulatePhysics(bool bNewSimulate)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
	{
		return;
	}

	if (Bodies.IsEmpty())
	{
		UE_LOG("Physics: SetAllBodiesSimulatePhysics: No physics bodies");
		return;
	}

	bRagdollActive = bNewSimulate;

	for (FBodyInstance* Body : Bodies)
	{
		if (!Body || !Body->IsValid())
		{
			continue;
		}

		PxRigidActor* Actor = Body->GetPhysicsActor();
		if (!Actor)
		{
			continue;
		}

		if (bNewSimulate)
		{
			// Kinematic -> Dynamic 전환
			PxRigidDynamic* DynActor = Actor->is<PxRigidDynamic>();
			if (DynActor)
			{
				DynActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
				DynActor->wakeUp();
			}
		}
		else
		{
			// Dynamic -> Kinematic 전환
			PxRigidDynamic* DynActor = Actor->is<PxRigidDynamic>();
			if (DynActor)
			{
				DynActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
			}
		}
	}
}

void USkeletalMeshComponent::SyncPhysicsToBones()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
	{
		return;
	}

	if (!bRagdollActive || Bodies.IsEmpty())
	{
		return;
	}

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const TArray<FBone>& Bones = Skeleton->Bones;
	const int32 NumBones = Bones.Num();

	UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	// Physics Body가 있는 본 인덱스 추적
	TSet<int32> BonesWithPhysics;

	// 컴포넌트 월드 트랜스폼의 역행렬 (World -> Component space 변환용)
	FMatrix ComponentWorldInv = GetWorldMatrix().Inverse();

	// 각 Body에서 본 트랜스폼 추출
	for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		FBodyInstance* Body = Bodies[BodyIdx];
		if (!Body || !Body->IsValid())
		{
			continue;
		}

		UBodySetup* Setup = Body->BodySetup;
		if (!Setup)
		{
			continue;
		}

		// 본 인덱스 찾기
		int32 BoneIdx = -1;
		for (int32 i = 0; i < NumBones; ++i)
		{
			if (FName(Bones[i].Name) == Setup->BoneName)
			{
				BoneIdx = i;
				break;
			}
		}

		if (BoneIdx == -1)
		{
			continue;
		}

		BonesWithPhysics.insert(BoneIdx);

		// PhysX Actor에서 월드 트랜스폼 가져오기
		PxRigidActor* Actor = Body->GetPhysicsActor();
		if (!Actor)
		{
			continue;
		}

		PxTransform PxWorldTM = Actor->getGlobalPose();

		// PhysX Transform -> FTransform 변환 (스케일은 1로 - Physics는 스케일 없음)
		FVector WorldPos(PxWorldTM.p.x, PxWorldTM.p.y, PxWorldTM.p.z);
		FQuat WorldRot(PxWorldTM.q.x, PxWorldTM.q.y, PxWorldTM.q.z, PxWorldTM.q.w);
		FTransform BoneWorldTransform(WorldPos, WorldRot, FVector(1, 1, 1));

		// World -> Component space 변환
		FMatrix BoneComponentMatrix = BoneWorldTransform.ToMatrix() * ComponentWorldInv;
		FTransform BoneComponentTransform(BoneComponentMatrix);

		// 스케일은 원래 본의 스케일 유지
		BoneComponentTransform.Scale3D = CurrentComponentSpacePose[BoneIdx].Scale3D;

		// Component Space Pose 업데이트
		CurrentComponentSpacePose[BoneIdx] = BoneComponentTransform;
	}

	// Component Space에서 Local Space 역계산 + Physics Body 없는 본은 부모 따라감
	for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
	{
		const int32 ParentIdx = Bones[BoneIdx].ParentIndex;

		// Physics Body가 없는 본은 부모의 변환을 따라가도록 ComponentSpace 재계산
		if (BonesWithPhysics.find(BoneIdx) == BonesWithPhysics.end() && ParentIdx >= 0)
		{
			// 기존 LocalSpace 유지하면서 부모의 새 ComponentSpace 따라가기
			CurrentComponentSpacePose[BoneIdx] = CurrentComponentSpacePose[ParentIdx].GetWorldTransform(CurrentLocalSpacePose[BoneIdx]);
		}

		// Local Space 역계산
		if (ParentIdx < 0)
		{
			// 루트 본: Component Space == Local Space
			CurrentLocalSpacePose[BoneIdx] = CurrentComponentSpacePose[BoneIdx];
		}
		else
		{
			// 자식 본: Local = Parent.Inverse * Component
			FTransform ParentComponent = CurrentComponentSpacePose[ParentIdx];
			CurrentLocalSpacePose[BoneIdx] = ParentComponent.GetRelativeTransform(CurrentComponentSpacePose[BoneIdx]);
		}
	}

	// 스키닝 매트릭스 업데이트
	UpdateFinalSkinningMatrices();
	UpdateSkinningMatrices(TempFinalSkinningMatrices, TempFinalSkinningNormalMatrices);
}

FBodyInstance* USkeletalMeshComponent::GetBodyInstance(int32 BoneIndex) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
	{
		return nullptr;
	}

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
	{
		return nullptr;
	}

	FName BoneName = FName(Skeleton->Bones[BoneIndex].Name);
	return GetBodyInstanceByBoneName(BoneName);
}

FBodyInstance* USkeletalMeshComponent::GetBodyInstanceByBoneName(const FName& BoneName) const
{
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return nullptr;
	}

	int32 BodyIdx = PhysicsAsset->FindBodyIndexByBoneName(BoneName);
	if (BodyIdx == -1 || BodyIdx >= Bodies.Num())
	{
		return nullptr;
	}

	return Bodies[BodyIdx];
}

void USkeletalMeshComponent::SyncBonesFromPhysics()
{
    if (Bodies.IsEmpty() || !SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return;
    }

    UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
    if (!PhysicsAsset)
    {
        return;
    }

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    const TArray<FBone>& Bones = Skeleton->Bones;
    const int32 NumBones = Bones.Num();
    FTransform CompWorldTransform = GetWorldTransform();

    // Physics Body가 있는 본 인덱스 추적
    TSet<int32> BonesWithPhysics;

    // Step 1: Physics Body가 있는 본들 업데이트
    for (int32 i = 0; i < Bodies.Num() && i < PhysicsAsset->BodySetups.Num(); ++i)
    {
        FBodyInstance* Body = Bodies[i];
        UBodySetup* Setup = PhysicsAsset->BodySetups[i];

        if (!Body || !Setup)
        {
            continue;
        }

        // Find bone index
        int32 BoneIdx = -1;
        for (int32 j = 0; j < NumBones; ++j)
        {
            if (FName(Bones[j].Name) == Setup->BoneName)
            {
                BoneIdx = j;
                break;
            }
        }

        if (BoneIdx < 0 || BoneIdx >= CurrentComponentSpacePose.Num())
        {
            continue;
        }

        BonesWithPhysics.insert(BoneIdx);

        // 원래 본의 scale 보존 (physics는 scale을 관리하지 않음)
        FVector OriginalScale = CurrentComponentSpacePose[BoneIdx].Scale3D;

        // Get physics body's world transform
        FTransform BodyWorldTransform = Body->GetWorldTransform();

        // World -> Component space 변환
        FVector WorldPos = BodyWorldTransform.Translation;
        FVector CompPos = CompWorldTransform.Rotation.Inverse().RotateVector(WorldPos - CompWorldTransform.Translation);
        FQuat CompRot = CompWorldTransform.Rotation.Inverse() * BodyWorldTransform.Rotation;

        // Component Space Pose 업데이트
        CurrentComponentSpacePose[BoneIdx] = FTransform(CompPos, CompRot, OriginalScale);
    }

    // Step 2: Physics Body가 없는 본은 부모 따라가기 + Local Space 역계산
    for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
    {
        const int32 ParentIdx = Bones[BoneIdx].ParentIndex;

        // Physics Body가 없는 본은 부모의 변환을 따라가도록 ComponentSpace 재계산
        if (BonesWithPhysics.find(BoneIdx) == BonesWithPhysics.end() && ParentIdx >= 0)
        {
            // 부모의 새 ComponentSpace + 자신의 LocalSpace = 새 ComponentSpace
            CurrentComponentSpacePose[BoneIdx] = CurrentComponentSpacePose[ParentIdx].GetWorldTransform(CurrentLocalSpacePose[BoneIdx]);
        }

        // Local Space 역계산
        if (ParentIdx < 0)
        {
            // 루트 본: Component Space == Local Space
            CurrentLocalSpacePose[BoneIdx] = CurrentComponentSpacePose[BoneIdx];
        }
        else
        {
            // 자식 본: Local = Parent.Inverse * Component
            CurrentLocalSpacePose[BoneIdx] = CurrentComponentSpacePose[ParentIdx].GetRelativeTransform(CurrentComponentSpacePose[BoneIdx]);
        }
    }

    // Skinning matrices 업데이트
    UpdateFinalSkinningMatrices();
    UpdateSkinningMatrices(TempFinalSkinningMatrices, TempFinalSkinningNormalMatrices);
}

void USkeletalMeshComponent::UpdateKinematicBonesToPhysics()
{
    // Early-out: 필수 데이터 체크
    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return;
    }

    if (Bodies.IsEmpty() || BodyToBoneIndex.IsEmpty())
    {
        return;
    }

    // 컴포넌트 월드 트랜스폼 (루프 밖에서 한 번만 계산)
    FTransform ComponentWorldTransform = GetWorldTransform();

    // 각 Body에 대해 본 트랜스폼 푸시
    for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
    {
        FBodyInstance* Body = Bodies[BodyIdx];
        if (!Body || !Body->IsValid())
        {
            continue;
        }

        // 캐시된 본 인덱스 사용 (O(1) 조회)
        int32 BoneIdx = BodyToBoneIndex[BodyIdx];
        if (BoneIdx < 0 || BoneIdx >= CurrentComponentSpacePose.Num())
        {
            continue;
        }

        // 본 월드 트랜스폼 계산: ComponentSpace -> WorldSpace
        FTransform BoneComponentTransform = CurrentComponentSpacePose[BoneIdx];
        FTransform BoneWorldTransform = ComponentWorldTransform.GetWorldTransform(BoneComponentTransform);

        // PhysX Actor에 트랜스폼 설정
        PxRigidDynamic* DynActor = Body->GetPhysicsActor()->is<PxRigidDynamic>();
        if (DynActor)
        {
            FVector Pos = BoneWorldTransform.Translation;
            FQuat Rot = BoneWorldTransform.Rotation;

            // 쿼터니언 정규화 (PhysX 요구사항)
            Rot.Normalize();

            // NaN/Inf 체크
            if (std::isfinite(Pos.X) && std::isfinite(Pos.Y) && std::isfinite(Pos.Z) &&
                std::isfinite(Rot.X) && std::isfinite(Rot.Y) && std::isfinite(Rot.Z) && std::isfinite(Rot.W))
            {
                PxTransform PxNewPose(
                    PxVec3(Pos.X, Pos.Y, Pos.Z),
                    PxQuat(Rot.X, Rot.Y, Rot.Z, Rot.W)
                );

                // setKinematicTarget: 다음 시뮬레이션 스텝에서 이동하며 dynamic 객체와 충돌 처리
                // (setGlobalPose는 텔레포트로, 다른 객체를 밀어내지 않고 통과함)
                DynActor->setKinematicTarget(PxNewPose);
            }
        }
    }
}

// ===== Cloth Section 감지 및 ClothComponent 관리 =====

/**
 * @brief 현재 SkeletalMesh에 Cloth Section이 있는지 확인
 */
bool USkeletalMeshComponent::HasClothSections() const
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
    {
        return false;
    }

    const auto& GroupInfos = SkeletalMesh->GetSkeletalMeshData()->GroupInfos;
    for (const auto& Group : GroupInfos)
    {
        if (Group.bEnableCloth)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief 내부 ClothComponent 생성 및 초기화
 */
void USkeletalMeshComponent::CreateInternalClothComponent()
{
	// AActor에 새로운 컴포넌트 추가
	InternalClothComponent = static_cast<UClothComponent*>(Owner->AddNewComponent(UClothComponent::StaticClass(), this));
	if (!InternalClothComponent)
	{
		return;
	}
	InternalClothComponent->SetSkeletalMesh(SkeletalMesh->GetPathFileName());

}

/**
 * @brief 내부 ClothComponent 정리
 * @note 소멸자에서는 직접 호출하지 않음 (Owner 파괴 중 Cast 크래시 방지)
 */
void USkeletalMeshComponent::DestroyInternalClothComponent()
{
    if (InternalClothComponent)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->RemoveOwnedComponent(InternalClothComponent);
		}
		InternalClothComponent = nullptr;
	}
}
