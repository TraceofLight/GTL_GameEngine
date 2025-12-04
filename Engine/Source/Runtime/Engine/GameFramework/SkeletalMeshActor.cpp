#include "pch.h"
#include "SkeletalMeshActor.h"
#include "World.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/GameFramework/FAudioDevice.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/Engine/Audio/Sound.h"

ASkeletalMeshActor::ASkeletalMeshActor()
{
    ObjectName = "Skeletal Mesh Actor";

    // 스킨드 메시 렌더용 컴포넌트 생성 및 루트로 설정
    // - 프리뷰 장면에서 메시를 표시하는 실제 렌더링 컴포넌트
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
    RootComponent = SkeletalMeshComponent;

    // AnimInstance는 애니메이션 탭에서 애니메이션을 선택할 때 생성됨
}

ASkeletalMeshActor::~ASkeletalMeshActor() = default;

void ASkeletalMeshActor::BeginPlay()
{
    Super::BeginPlay();

    // AnimNotify Delegate 구독 (UE 표준: BeginPlay에서 초기화)
    RegisterAnimNotifyDelegate();
}

void ASkeletalMeshActor::EndPlay()
{
    // BeginPlay에서 등록한 델리게이트를 대칭적으로 해제 (LuaScriptComponent 패턴)
    if (SkeletalMeshComponent && AnimNotifyDelegateHandle != 0)
    {
        SkeletalMeshComponent->OnAnimNotify.Remove(AnimNotifyDelegateHandle);
        AnimNotifyDelegateHandle = 0;
    }

    Super::EndPlay();
}

void ASkeletalMeshActor::RegisterAnimNotifyDelegate()
{
    if (!SkeletalMeshComponent)
    {
        return;
    }

    // 중복 구독 방지
    if (AnimNotifyDelegateHandle != 0)
    {
        return;
    }

    AnimNotifyDelegateHandle = SkeletalMeshComponent->OnAnimNotify.Add([this](const FAnimNotifyEvent& Event)
    {
        FString SoundPath = Event.NotifyName.ToString();
        USound* Sound = UResourceManager::GetInstance().Load<USound>(SoundPath);
        if (Sound)
        {
            FAudioDevice::PlaySound3D(Sound, GetActorLocation(), 1.0f, false);
        }
    });
}

void ASkeletalMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

FAABB ASkeletalMeshActor::GetBounds() const
{
    // Be robust to component replacement: query current root
    if (auto* Current = Cast<USkeletalMeshComponent>(RootComponent))
    {
        return Current->GetWorldAABB();
    }
    return FAABB();
}

void ASkeletalMeshActor::SetSkeletalMeshComponent(USkeletalMeshComponent* InComp)
{
    SkeletalMeshComponent = InComp;
}

void ASkeletalMeshActor::SetSkeletalMesh(const FString& PathFileName)
{
    if (SkeletalMeshComponent)
    {
        SkeletalMeshComponent->SetSkeletalMesh(PathFileName);
    }
}

void ASkeletalMeshActor::EnsureViewerComponents()
{
    // Only create viewer components if they don't exist and we're in a preview world
    if (BoneLineComponent)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || !World->IsPreviewWorld())
    {
        return;
    }

    // Create bone line component for skeleton visualization
    if (!BoneLineComponent)
    {
        BoneLineComponent = NewObject<ULineComponent>();
        if (BoneLineComponent && RootComponent)
        {
            BoneLineComponent->ObjectName = "BoneLines";
            BoneLineComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
            BoneLineComponent->SetAlwaysOnTop(true);
            AddOwnedComponent(BoneLineComponent);
            BoneLineComponent->RegisterComponent(World);
        }
    }
}

void ASkeletalMeshActor::RebuildBoneLines(int32 SelectedBoneIndex, bool bUpdateAllBones, bool bForceHighlightRefresh)
{
    // Ensure viewer components exist before using them
    EnsureViewerComponents();

    if (!BoneLineComponent || !SkeletalMeshComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0)
    {
        return;
    }

    // Initialize cache once per mesh
    if (!bBoneLinesInitialized || BoneLinesCache.Num() != BoneCount)
    {
        BoneLineComponent->ClearLines();
        BuildBoneLinesCache();
        bBoneLinesInitialized = true;
        CachedSelected = -1;
    }

    // Update selection highlight when changed or forced
    if (CachedSelected != SelectedBoneIndex || bForceHighlightRefresh)
    {
        UpdateBoneSelectionHighlight(SelectedBoneIndex);
        CachedSelected = SelectedBoneIndex;
    }

    // Update transforms
    if (bUpdateAllBones)
    {
        // 모든 본 업데이트 (애니메이션 재생 중)
        for (int32 i = 0; i < BoneCount; ++i)
        {
            UpdateBoneSubtreeTransforms(i);
        }
    }
    else if (SelectedBoneIndex >= 0 && SelectedBoneIndex < BoneCount)
    {
        // 선택된 본 서브트리만 업데이트 (편집 모드)
        UpdateBoneSubtreeTransforms(SelectedBoneIndex);
    }
}

void ASkeletalMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // Find skeletal mesh component (always exists)
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<USkeletalMeshComponent>(Component))
        {
            SkeletalMeshComponent = Comp;
            break;
        }
    }

    // Find viewer components (may not exist if not in preview world)
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<ULineComponent>(Component))
        {
            BoneLineComponent = Comp;
            break;
        }
    }
}

void ASkeletalMeshActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RootComponent);
    }
}

void ASkeletalMeshActor::BuildBoneLinesCache()
{
    if (!SkeletalMeshComponent || !BoneLineComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());

    BoneLinesCache.Empty();
    BoneLinesCache.resize(BoneCount);
    BoneChildren.Empty();
    BoneChildren.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const int32 p = Bones[i].ParentIndex;
        if (p >= 0 && p < BoneCount)
        {
            BoneChildren[p].Add(i);
        }
    }

    // Initial centers from current bone transforms (object space)
    // Use GetBoneWorldTransform to properly accumulate parent transforms
    const FMatrix WorldInv = GetWorldMatrix().InverseAffine();
    TArray<FVector> JointPos;
    JointPos.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FMatrix W = SkeletalMeshComponent->GetBoneWorldTransform(i).ToMatrix();
        const FMatrix O = W * WorldInv;
        JointPos[i] = FVector(O.M[3][0], O.M[3][1], O.M[3][2]);
    }

    const int NumSegments = CachedSegments;

    for (int32 i = 0; i < BoneCount; ++i)
    {
        FBoneDebugLines& BL = BoneLinesCache[i];
        const FVector Center = JointPos[i];
        const int32 parent = Bones[i].ParentIndex;

        // 사각 피라미드로 본 연결 표시
        if (parent >= 0)
        {
            const FVector ParentPos = JointPos[parent];
            const FVector ChildPos = Center;
            const FVector BoneDir = (ChildPos - ParentPos).GetNormalized();

            // 수직 벡터 계산 (피라미드 베이스용)
            FVector Up = FVector(0, 0, 1);
            if (std::abs(FVector::Dot(Up, BoneDir)) > 0.99f)
            {
                Up = FVector(0, 1, 0); // 본이 Z축과 평행하면 Y축 사용
            }

            FVector Right = FVector::Cross(BoneDir, Up).GetNormalized();
            FVector Forward = FVector::Cross(Right, BoneDir).GetNormalized();

            // 본 길이 기반 피라미드 크기 조정
            const float BoneLength = (ChildPos - ParentPos).Size();
            const float PyramidSize = std::min(BoneBaseRadius, BoneLength * 0.15f);

            // 사각형 베이스 4개 코너 계산 (부모 조인트 위치에 배치)
            const FVector Corner0 = ParentPos + Right * PyramidSize + Forward * PyramidSize;
            const FVector Corner1 = ParentPos - Right * PyramidSize + Forward * PyramidSize;
            const FVector Corner2 = ParentPos - Right * PyramidSize - Forward * PyramidSize;
            const FVector Corner3 = ParentPos + Right * PyramidSize - Forward * PyramidSize;

            // 기본 색상: 검은색
            const FVector4 BaseColor(0.0f, 0.0f, 0.0f, 1.0f);

            // 피라미드 생성: 4개 베이스 라인 + 4개 엣지 라인 = 총 8개 라인
            BL.ConeEdges.reserve(4); // 엣지 라인 (코너 → 자식 조인트)
            BL.ConeBase.reserve(4);  // 베이스 라인 (사각형)

            // 베이스 사각형 4개 라인
            BL.ConeBase.Add(BoneLineComponent->AddLine(Corner0, Corner1, BaseColor));
            BL.ConeBase.Add(BoneLineComponent->AddLine(Corner1, Corner2, BaseColor));
            BL.ConeBase.Add(BoneLineComponent->AddLine(Corner2, Corner3, BaseColor));
            BL.ConeBase.Add(BoneLineComponent->AddLine(Corner3, Corner0, BaseColor));

            // 엣지 라인 4개 (각 코너에서 자식 조인트로)
            BL.ConeEdges.Add(BoneLineComponent->AddLine(Corner0, ChildPos, BaseColor));
            BL.ConeEdges.Add(BoneLineComponent->AddLine(Corner1, ChildPos, BaseColor));
            BL.ConeEdges.Add(BoneLineComponent->AddLine(Corner2, ChildPos, BaseColor));
            BL.ConeEdges.Add(BoneLineComponent->AddLine(Corner3, ChildPos, BaseColor));
        }

        // Joint 시각화
        BL.Rings.reserve(NumSegments * 3);

        const FVector4 JointRingColor(0.0f, 0.0f, 0.0f, 1.0f); // 검은색

        for (int k = 0; k < NumSegments; ++k)
        {
            const float a0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
            const float a1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;

            // XY 평면 링 (Z=0)
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0), 0.0f),
                Center + FVector(BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1), 0.0f),
                JointRingColor));

            // XZ 평면 링 (Y=0)
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(BoneJointRadius * std::cos(a0), 0.0f, BoneJointRadius * std::sin(a0)),
                Center + FVector(BoneJointRadius * std::cos(a1), 0.0f, BoneJointRadius * std::sin(a1)),
                JointRingColor));

            // YZ 평면 링 (X=0)
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(0.0f, BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0)),
                Center + FVector(0.0f, BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1)),
                JointRingColor));
        }
    }
}

void ASkeletalMeshActor::UpdateBoneSelectionHighlight(int32 SelectedBoneIndex)
{
    if (!SkeletalMeshComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());

    // 색상 스킴
    const FVector4 BaseColor(0.0f, 0.0f, 0.0f, 1.0f);       // 기본: 검은색
    const FVector4 OrangeColor(1.0f, 0.5f, 0.0f, 1.0f);     // 선택된 본 → 부모: 주황색
    const FVector4 GreenColor(0.0f, 1.0f, 0.0f, 1.0f);      // 선택된 본의 직접 자식: 초록색
    const FVector4 WhiteColor(1.0f, 1.0f, 1.0f, 1.0f);      // 선택된 본의 자손들: 흰색

    // 선택된 본의 모든 자손 인덱스를 수집 (직접 자식 제외)
    std::set<int32> DescendantIndices;
    std::set<int32> DirectChildIndices;

    if (SelectedBoneIndex >= 0)
    {
        // 직접 자식 찾기
        for (int32 i = 0; i < BoneCount; ++i)
        {
            if (Bones[i].ParentIndex == SelectedBoneIndex)
            {
                DirectChildIndices.insert(i);
            }
        }

        // BFS로 모든 자손 수집 (직접 자식의 자손들)
        TArray<int32> Queue;
        for (int32 DirectChild : DirectChildIndices)
        {
            // 직접 자식의 자식들부터 큐에 추가
            for (int32 i = 0; i < BoneCount; ++i)
            {
                if (Bones[i].ParentIndex == DirectChild)
                {
                    Queue.Add(i);
                }
            }
        }

        while (!Queue.IsEmpty())
        {
            int32 Current = Queue.Last();
            Queue.Pop();

            DescendantIndices.insert(Current);

            // 이 본의 자식들도 큐에 추가
            for (int32 i = 0; i < BoneCount; ++i)
            {
                if (Bones[i].ParentIndex == Current)
                {
                    Queue.Add(i);
                }
            }
        }
    }

    for (int32 i = 0; i < BoneCount; ++i)
    {
        FBoneDebugLines& BL = BoneLinesCache[i];
        const int32 parent = Bones[i].ParentIndex;

        // 이 본의 피라미드 색상 결정
        FVector4 PyramidColor = BaseColor;

        if (SelectedBoneIndex >= 0)
        {
            // 선택된 본의 부모와의 연결 → 주황색
            if (i == SelectedBoneIndex && parent >= 0)
            {
                PyramidColor = OrangeColor;
            }
            // 선택된 본의 직접 자식 → 초록색
            else if (DirectChildIndices.count(i) > 0)
            {
                PyramidColor = GreenColor;
            }
            // 선택된 본의 자손들 (직접 자식 제외) → 흰색
            else if (DescendantIndices.count(i) > 0)
            {
                PyramidColor = WhiteColor;
            }
        }

        // 피라미드 엣지 라인 색상 업데이트
        for (ULine* L : BL.ConeEdges)
        {
            if (L) L->SetColor(PyramidColor);
        }

        // 피라미드 베이스 라인 색상 업데이트
        for (ULine* L : BL.ConeBase)
        {
            if (L) L->SetColor(PyramidColor);
        }

        // Joint 구체 링 색상 업데이트 (선택된 본, 직접 자식, 또는 자손이면 흰색, 그 외 검은색)
        FVector4 JointColor = BaseColor;
        if (SelectedBoneIndex >= 0 && (i == SelectedBoneIndex || DirectChildIndices.count(i) > 0 || DescendantIndices.count(i) > 0))
        {
            JointColor = WhiteColor;
        }
        for (ULine* L : BL.Rings)
        {
            if (L)
            {
                L->SetColor(JointColor);
            }
        }
    }
}

void ASkeletalMeshActor::UpdateBoneSubtreeTransforms(int32 BoneIndex)
{
    if (!SkeletalMeshComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneIndex < 0 || BoneIndex >= BoneCount)
    {
        return;
    }

    TArray<int32> Stack; Stack.Add(BoneIndex);
    TArray<int32> ToUpdate;
    while (!Stack.IsEmpty())
    {
        int32 b = Stack.Last(); Stack.Pop();
        ToUpdate.Add(b);
        for (int32 c : BoneChildren[b]) Stack.Add(c);
    }

    const FMatrix WorldInv = GetWorldMatrix().InverseAffine();
    TArray<FVector> Centers; Centers.resize(BoneCount);

    for (int32 b : ToUpdate)
    {
        const FMatrix W = SkeletalMeshComponent->GetBoneWorldTransform(b).ToMatrix();
        const FMatrix O = W * WorldInv;
        Centers[b] = FVector(O.M[3][0], O.M[3][1], O.M[3][2]);
    }

    const int NumSegments = CachedSegments;

    for (int32 b : ToUpdate)
    {
        FBoneDebugLines& BL = BoneLinesCache[b];
        const int32 parent = Bones[b].ParentIndex;

        // Update cone geometry
        if (parent >= 0 && !BL.ConeEdges.IsEmpty())
        {
            const FMatrix ParentW = SkeletalMeshComponent->GetBoneWorldTransform(parent).ToMatrix();
            const FMatrix ParentO = ParentW * WorldInv;
            const FVector ParentPos(ParentO.M[3][0], ParentO.M[3][1], ParentO.M[3][2]);
            const FVector ChildPos = Centers[b];

            const FVector BoneDir = (ChildPos - ParentPos).GetNormalized();

            // Calculate perpendicular vectors for the cone base
            FVector Up = FVector(0, 0, 1);
            if (std::abs(FVector::Dot(Up, BoneDir)) > 0.99f)
            {
                Up = FVector(0, 1, 0);
            }

            FVector Right = FVector::Cross(BoneDir, Up).GetNormalized();
            FVector Forward = FVector::Cross(Right, BoneDir).GetNormalized();

            // Scale cone radius based on bone length
            const float BoneLength = (ChildPos - ParentPos).Size();
            const float PyramidSize = std::min(BoneBaseRadius, BoneLength * 0.15f);

            // 사각 피라미드 4개 코너 재계산
            const FVector Corner0 = ParentPos + Right * PyramidSize + Forward * PyramidSize;
            const FVector Corner1 = ParentPos - Right * PyramidSize + Forward * PyramidSize;
            const FVector Corner2 = ParentPos - Right * PyramidSize - Forward * PyramidSize;
            const FVector Corner3 = ParentPos + Right * PyramidSize - Forward * PyramidSize;

            // 베이스 사각형 업데이트 (4개 라인)
            if (BL.ConeBase.Num() >= 4)
            {
                if (BL.ConeBase[0]) BL.ConeBase[0]->SetLine(Corner0, Corner1);
                if (BL.ConeBase[1]) BL.ConeBase[1]->SetLine(Corner1, Corner2);
                if (BL.ConeBase[2]) BL.ConeBase[2]->SetLine(Corner2, Corner3);
                if (BL.ConeBase[3]) BL.ConeBase[3]->SetLine(Corner3, Corner0);
            }

            // 엣지 라인 업데이트 (4개 라인)
            if (BL.ConeEdges.Num() >= 4)
            {
                if (BL.ConeEdges[0]) BL.ConeEdges[0]->SetLine(Corner0, ChildPos);
                if (BL.ConeEdges[1]) BL.ConeEdges[1]->SetLine(Corner1, ChildPos);
                if (BL.ConeEdges[2]) BL.ConeEdges[2]->SetLine(Corner2, ChildPos);
                if (BL.ConeEdges[3]) BL.ConeEdges[3]->SetLine(Corner3, ChildPos);
            }
        }

        // Joint 구체 링 업데이트
        const FVector Center = Centers[b];

        for (int k = 0; k < NumSegments; ++k)
        {
            const float a0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
            const float a1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;
            const int base = k * 3;
            if (BL.Rings.IsEmpty() || base + 2 >= BL.Rings.Num()) break;

            // XY 평면 링
            BL.Rings[base + 0]->SetLine(
                Center + FVector(BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0), 0.0f),
                Center + FVector(BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1), 0.0f));

            // XZ 평면 링
            BL.Rings[base + 1]->SetLine(
                Center + FVector(BoneJointRadius * std::cos(a0), 0.0f, BoneJointRadius * std::sin(a0)),
                Center + FVector(BoneJointRadius * std::cos(a1), 0.0f, BoneJointRadius * std::sin(a1)));

            // YZ 평면 링
            BL.Rings[base + 2]->SetLine(
                Center + FVector(0.0f, BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0)),
                Center + FVector(0.0f, BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1)));
        }
    }
}

int32 ASkeletalMeshActor::PickBone(const FRay& Ray, float& OutDistance) const
{
    if (!SkeletalMeshComponent)
    {
        return -1;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return -1;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return -1;
    }

    const auto& Bones = Data->Skeleton.Bones;
    if (Bones.empty())
    {
        return -1;
    }

    int32 ClosestBoneIndex = -1;
    float ClosestDistance = FLT_MAX;

    // Test each bone with a bounding sphere
    for (int32 i = 0; i < (int32)Bones.size(); ++i)
    {
        // Get bone world transform
        FTransform BoneWorldTransform = SkeletalMeshComponent->GetBoneWorldTransform(i);
        FVector BoneWorldPos = BoneWorldTransform.Translation;

        // Use BoneJointRadius as picking radius (can be adjusted)
        float PickRadius = BoneJointRadius * 2.0f; // Slightly larger for easier picking

        // Test ray-sphere intersection
        float HitDistance;
        if (IntersectRaySphere(Ray, BoneWorldPos, PickRadius, HitDistance))
        {
            if (HitDistance < ClosestDistance)
            {
                ClosestDistance = HitDistance;
                ClosestBoneIndex = i;
            }
        }
    }

    if (ClosestBoneIndex >= 0)
    {
        OutDistance = ClosestDistance;
    }

    return ClosestBoneIndex;
}
