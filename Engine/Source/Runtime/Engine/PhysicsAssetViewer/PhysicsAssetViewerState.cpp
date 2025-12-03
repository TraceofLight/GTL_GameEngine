#include "pch.h"
#include "PhysicsAssetViewerState.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsConstraintSetup.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/ClothComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Core/Misc/PrimitiveGeometry.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Renderer.h"

UBodySetup* PhysicsAssetViewerState::GetSelectedBodySetup() const
{
    if (!PhysicsAsset || SelectedBodyIndex < 0)
    {
        return nullptr;
    }

    if (SelectedBodyIndex >= PhysicsAsset->BodySetups.Num())
    {
        return nullptr;
    }

    return PhysicsAsset->BodySetups[SelectedBodyIndex];
}

void PhysicsAssetViewerState::ClearSelection()
{
    SelectedBodyIndex = -1;
    GraphFilterRootBodyIndex = -1;  // 필터 해제 (전체 그래프 표시)
    SelectedConstraintIndex = -1;
    SelectedShapeIndex = -1;
    SelectedShapeType = EAggCollisionShape::Unknown;
    SelectedBoneName = FName();

    // 기즈모 클리어 (World에서 가져옴)
    AGizmoActor* Gizmo = World ? World->GetGizmoActor() : nullptr;
    if (Gizmo)
    {
        Gizmo->ClearConstraintTarget();
        Gizmo->SetbRender(false);
    }
}

void PhysicsAssetViewerState::SelectBody(int32 BodyIndex)
{
    ClearSelection();
    SelectedBodyIndex = BodyIndex;
    GraphFilterRootBodyIndex = BodyIndex;  // Skeleton Tree 클릭 시 그래프 필터링 루트도 변경

    if (PhysicsAsset && BodyIndex >= 0 && BodyIndex < PhysicsAsset->BodySetups.Num())
    {
        UBodySetup* Setup = PhysicsAsset->BodySetups[BodyIndex];
        if (Setup)
        {
            SelectedBoneName = Setup->BoneName;

            // Body의 첫 번째 Shape 위치에 Gizmo 설정
            AGizmoActor* Gizmo = World ? World->GetGizmoActor() : nullptr;
            if (Gizmo && PreviewActor && CurrentMesh)
            {
                USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
                const FSkeleton* Skeleton = CurrentMesh->GetSkeleton();

                if (SkelComp && Skeleton)
                {
                    // 해당 본의 인덱스 찾기
                    int32 BoneIndex = -1;
                    std::string BoneNameStr = Setup->BoneName.ToString();
                    for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
                    {
                        if (Skeleton->Bones[i].Name == BoneNameStr)
                        {
                            BoneIndex = i;
                            break;
                        }
                    }

                    if (BoneIndex >= 0)
                    {
                        // Body의 첫 번째 Shape 타입 및 인덱스 결정
                        EAggCollisionShape::Type ShapeType = EAggCollisionShape::Unknown;
                        int32 ShapeIndex = 0;

                        if (Setup->AggGeom.SphereElems.Num() > 0)
                        {
                            ShapeType = EAggCollisionShape::Sphere;
                        }
                        else if (Setup->AggGeom.BoxElems.Num() > 0)
                        {
                            ShapeType = EAggCollisionShape::Box;
                        }
                        else if (Setup->AggGeom.SphylElems.Num() > 0)
                        {
                            ShapeType = EAggCollisionShape::Capsule;
                        }

                        if (ShapeType != EAggCollisionShape::Unknown)
                        {
                            Gizmo->SetShapeTarget(SkelComp, Setup, BoneIndex, ShapeType, ShapeIndex);
                            Gizmo->SetMode(EGizmoMode::Translate);  // Body는 이동이 주요 용도
                            Gizmo->SetbRender(true);
                        }
                    }
                }
            }
        }
    }

    EditMode = EPhysicsAssetEditMode::Body;
}

void PhysicsAssetViewerState::SelectConstraint(int32 ConstraintIndex)
{
    ClearSelection();
    SelectedConstraintIndex = ConstraintIndex;
    EditMode = EPhysicsAssetEditMode::Constraint;

    // 기즈모 설정 (World에서 가져옴)
    AGizmoActor* Gizmo = World ? World->GetGizmoActor() : nullptr;
    if (Gizmo && PhysicsAsset && PreviewActor && CurrentMesh &&
        ConstraintIndex >= 0 && ConstraintIndex < PhysicsAsset->ConstraintSetups.Num())
    {
        UPhysicsConstraintSetup* Constraint = PhysicsAsset->ConstraintSetups[ConstraintIndex];
        USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
        const FSkeleton* Skeleton = CurrentMesh->GetSkeleton();

        if (Constraint && SkelComp && Skeleton)
        {
            // 두 본의 인덱스 찾기
            int32 BoneIndex1 = -1;
            int32 BoneIndex2 = -1;
            for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
            {
                if (Skeleton->Bones[i].Name == Constraint->ConstraintBone1.ToString())
                    BoneIndex1 = i;
                if (Skeleton->Bones[i].Name == Constraint->ConstraintBone2.ToString())
                    BoneIndex2 = i;
            }

            if (BoneIndex1 >= 0 && BoneIndex2 >= 0)
            {
                Gizmo->SetConstraintTarget(SkelComp, Constraint, BoneIndex1, BoneIndex2);
                Gizmo->SetMode(EGizmoMode::Rotate);  // Constraint는 회전만 의미 있음
                Gizmo->SetbRender(true);
            }
        }
    }
}

void PhysicsAssetViewerState::SelectShape(int32 BodyIndex, EAggCollisionShape::Type ShapeType, int32 ShapeIndex)
{
    char debugMsg[512];
    sprintf_s(debugMsg, "[PAE] SelectShape called: BodyIndex=%d, ShapeType=%d, ShapeIndex=%d\n", BodyIndex, (int)ShapeType, ShapeIndex);
    OutputDebugStringA(debugMsg);

    ClearSelection();
    SelectedBodyIndex = BodyIndex;
    SelectedShapeType = ShapeType;
    SelectedShapeIndex = ShapeIndex;

    if (PhysicsAsset && BodyIndex >= 0 && BodyIndex < PhysicsAsset->BodySetups.Num())
    {
        UBodySetup* Setup = PhysicsAsset->BodySetups[BodyIndex];
        if (Setup)
        {
            SelectedBoneName = Setup->BoneName;

            // 기즈모 설정 (World에서 가져옴)
            AGizmoActor* Gizmo = World ? World->GetGizmoActor() : nullptr;

            sprintf_s(debugMsg, "[PAE] SelectShape: Gizmo=%p, PreviewActor=%p, CurrentMesh=%p\n",
                (void*)Gizmo, (void*)PreviewActor, (void*)CurrentMesh);
            OutputDebugStringA(debugMsg);

            if (Gizmo && PreviewActor && CurrentMesh)
            {
                USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
                const FSkeleton* Skeleton = CurrentMesh->GetSkeleton();

                sprintf_s(debugMsg, "[PAE] SelectShape: SkelComp=%p, Skeleton=%p\n", (void*)SkelComp, (void*)Skeleton);
                OutputDebugStringA(debugMsg);

                if (SkelComp && Skeleton)
                {
                    // 해당 본의 인덱스 찾기
                    int32 BoneIndex = -1;
                    std::string BoneNameStr = Setup->BoneName.ToString();

                    sprintf_s(debugMsg, "[PAE] SelectShape: Looking for BoneName='%s' in %d bones\n",
                        BoneNameStr.c_str(), (int)Skeleton->Bones.size());
                    OutputDebugStringA(debugMsg);

                    for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
                    {
                        if (Skeleton->Bones[i].Name == BoneNameStr)
                        {
                            BoneIndex = i;
                            break;
                        }
                    }

                    // 본 매칭 실패 시 디버그 출력
                    if (BoneIndex < 0)
                    {
                        sprintf_s(debugMsg, "[PAE] SelectShape: BoneName='%s' NOT FOUND in Skeleton!\n", BoneNameStr.c_str());
                        OutputDebugStringA(debugMsg);

                        // 스켈레톤의 본 이름들 출력 (처음 10개)
                        for (int32 i = 0; i < std::min((int32)Skeleton->Bones.size(), 10); ++i)
                        {
                            sprintf_s(debugMsg, "  Skeleton Bone[%d]: '%s'\n", i, Skeleton->Bones[i].Name.c_str());
                            OutputDebugStringA(debugMsg);
                        }
                    }
                    else
                    {
                        sprintf_s(debugMsg, "[PAE] SelectShape: Found BoneIndex=%d, calling SetShapeTarget\n", BoneIndex);
                        OutputDebugStringA(debugMsg);

                        Gizmo->SetShapeTarget(SkelComp, Setup, BoneIndex, ShapeType, ShapeIndex);
                        Gizmo->SetMode(EGizmoMode::Scale);  // Shape는 크기 조정이 주요 용도
                        Gizmo->SetbRender(true);

                        // 기즈모 설정 후 상태 확인
                        sprintf_s(debugMsg, "[PAE] SelectShape: After SetShapeTarget - TargetType=%d, bRender=%d\n",
                            (int)Gizmo->GetTargetType(), Gizmo->GetbRender() ? 1 : 0);
                        OutputDebugStringA(debugMsg);
                    }
                }
                else
                {
                    OutputDebugStringA("[PAE] SelectShape: SkelComp or Skeleton is null!\n");
                }
            }
            else
            {
                OutputDebugStringA("[PAE] SelectShape: Gizmo, PreviewActor, or CurrentMesh is null!\n");
            }
        }
        else
        {
            OutputDebugStringA("[PAE] SelectShape: Setup is null!\n");
        }
    }
    else
    {
        sprintf_s(debugMsg, "[PAE] SelectShape: Invalid BodyIndex or PhysicsAsset (PhysicsAsset=%p, BodyIndex=%d)\n",
            (void*)PhysicsAsset, BodyIndex);
        OutputDebugStringA(debugMsg);
    }

    EditMode = EPhysicsAssetEditMode::Shape;
}

// ============================================================================
// 디버그 드로잉
// ============================================================================

void PhysicsAssetViewerState::DrawPhysicsBodies(URenderer* Renderer) const
{
    if (!bShowBodies || !PhysicsAsset || !Renderer)
        return;

    // SkeletalMeshComponent에서 본 위치 가져오기
    USkeletalMeshComponent* SkelComp = nullptr;
    if (PreviewActor)
    {
        SkelComp = PreviewActor->GetSkeletalMeshComponent();
    }

    const FSkeleton* Skeleton = CurrentMesh ? CurrentMesh->GetSkeleton() : nullptr;

    // 기본 색상 (언리얼 스타일)
    const FVector4 NormalColor(1.0f, 1.0f, 1.0f, 1.0f);      // 흰색 (일반 Body)
    const FVector4 SelectedColor(0.3f, 0.7f, 1.0f, 1.0f);    // 파란색 (선택된 Body)
    const FVector4 ShapeSelectedColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색 (선택된 Shape)

    // 모든 BodySetup 순회
    for (int32 BodyIdx = 0; BodyIdx < PhysicsAsset->BodySetups.Num(); ++BodyIdx)
    {
        UBodySetup* Setup = PhysicsAsset->BodySetups[BodyIdx];
        if (!Setup)
            continue;

        // 본의 월드 트랜스폼 가져오기
        FTransform BoneTransform;
        if (SkelComp && Skeleton)
        {
            // BoneName으로 본 인덱스 찾기
            int32 BoneIndex = -1;
            for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
            {
                if (Skeleton->Bones[i].Name == Setup->BoneName.ToString())
                {
                    BoneIndex = i;
                    break;
                }
            }

            if (BoneIndex >= 0)
            {
                BoneTransform = SkelComp->GetBoneWorldTransform(BoneIndex);
            }
            else
            {
                // 본을 찾지 못하면 컴포넌트의 월드 트랜스폼 사용
                BoneTransform = SkelComp->GetWorldTransform();
            }

            // Body가 선택되었는지 확인
            bool bBodySelected = (EditMode == EPhysicsAssetEditMode::Body && SelectedBodyIndex == BodyIdx);
            bool bShapeMode = (EditMode == EPhysicsAssetEditMode::Shape && SelectedBodyIndex == BodyIdx);

            // Sphere shapes 그리기
            for (int32 i = 0; i < Setup->AggGeom.SphereElems.Num(); ++i)
            {
                FVector4 Color = NormalColor;
                if (bBodySelected)
                    Color = SelectedColor;
                else if (bShapeMode && SelectedShapeType == EAggCollisionShape::Sphere && SelectedShapeIndex == i)
                    Color = ShapeSelectedColor;

                DrawSphere(Renderer, BoneTransform, Setup->AggGeom.SphereElems[i], Color);
            }

            // Box shapes 그리기
            for (int32 i = 0; i < Setup->AggGeom.BoxElems.Num(); ++i)
            {
                FVector4 Color = NormalColor;
                if (bBodySelected)
                    Color = SelectedColor;
                else if (bShapeMode && SelectedShapeType == EAggCollisionShape::Box && SelectedShapeIndex == i)
                    Color = ShapeSelectedColor;

                DrawBox(Renderer, BoneTransform, Setup->AggGeom.BoxElems[i], Color);
            }

            // Capsule shapes 그리기
            for (int32 i = 0; i < Setup->AggGeom.SphylElems.Num(); ++i)
            {
                FVector4 Color = NormalColor;
                if (bBodySelected)
                    Color = SelectedColor;
                else if (bShapeMode && SelectedShapeType == EAggCollisionShape::Capsule && SelectedShapeIndex == i)
                    Color = ShapeSelectedColor;

                DrawCapsule(Renderer, BoneTransform, Setup->AggGeom.SphylElems[i], Color);
            }
        }
        else
        {
            // SkeletalMeshComponent가 없으면 원점 사용
            BoneTransform = FTransform();

            // Body가 선택되었는지 확인
            bool bBodySelected = (EditMode == EPhysicsAssetEditMode::Body && SelectedBodyIndex == BodyIdx);
            bool bShapeMode = (EditMode == EPhysicsAssetEditMode::Shape && SelectedBodyIndex == BodyIdx);

            // Capsule shapes 그리기 (no InverseBindPose available)
            for (int32 i = 0; i < Setup->AggGeom.SphylElems.Num(); ++i)
            {
                FVector4 Color = NormalColor;
                if (bBodySelected)
                    Color = SelectedColor;
                else if (bShapeMode && SelectedShapeType == EAggCollisionShape::Capsule && SelectedShapeIndex == i)
                    Color = ShapeSelectedColor;

                DrawCapsule(Renderer, BoneTransform, Setup->AggGeom.SphylElems[i], Color);
            }
        }
    }
}

void PhysicsAssetViewerState::DrawSphere(URenderer* Renderer, const FTransform& BoneTransform, const FKSphereElem& Sphere, const FVector4& Color) const
{
    // Shape의 로컬 위치를 본의 월드 트랜스폼으로 변환
    FVector WorldCenter = BoneTransform.TransformPosition(Sphere.Center);
    float Radius = Sphere.Radius;

    const int NumSegments = 16;
    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    // XY circle
    for (int i = 0; i < NumSegments; ++i)
    {
        float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        FVector p0 = WorldCenter + FVector(Radius * std::cos(a0), Radius * std::sin(a0), 0.0f);
        FVector p1 = WorldCenter + FVector(Radius * std::cos(a1), Radius * std::sin(a1), 0.0f);

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(Color);
    }

    // XZ circle
    for (int i = 0; i < NumSegments; ++i)
    {
        float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        FVector p0 = WorldCenter + FVector(Radius * std::cos(a0), 0.0f, Radius * std::sin(a0));
        FVector p1 = WorldCenter + FVector(Radius * std::cos(a1), 0.0f, Radius * std::sin(a1));

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(Color);
    }

    // YZ circle
    for (int i = 0; i < NumSegments; ++i)
    {
        float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        FVector p0 = WorldCenter + FVector(0.0f, Radius * std::cos(a0), Radius * std::sin(a0));
        FVector p1 = WorldCenter + FVector(0.0f, Radius * std::cos(a1), Radius * std::sin(a1));

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(Color);
    }

    Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void PhysicsAssetViewerState::DrawBox(URenderer* Renderer, const FTransform& BoneTransform, const FKBoxElem& Box, const FVector4& Color) const
{
    // Box의 로컬 트랜스폼 (Center + Rotation)
    // MakeFromEulerZYX expects DEGREES (it converts to radians internally)
    FQuat BoxRot = FQuat::MakeFromEulerZYX(Box.Rotation);
    FTransform BoxLocalTransform(Box.Center, BoxRot, FVector(1, 1, 1));

    // 본 트랜스폼과 결합
    FTransform BoxWorldTransform = BoneTransform.GetWorldTransform(BoxLocalTransform);
    // Compose world without inheriting parent scale into translation
    const FQuat BoxWorldRot = BoneTransform.Rotation * BoxRot;
    const FVector BoxWorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Box.Center);
    const FTransform BoxWorldNoScale(BoxWorldCenter, BoxWorldRot, FVector(1, 1, 1));

    FVector Extent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);

    // 로컬 코너들
    FVector LocalCorners[8] = {
        {-Extent.X, -Extent.Y, -Extent.Z}, {+Extent.X, -Extent.Y, -Extent.Z},
        {-Extent.X, +Extent.Y, -Extent.Z}, {+Extent.X, +Extent.Y, -Extent.Z},
        {-Extent.X, -Extent.Y, +Extent.Z}, {+Extent.X, -Extent.Y, +Extent.Z},
        {-Extent.X, +Extent.Y, +Extent.Z}, {+Extent.X, +Extent.Y, +Extent.Z},
    };

    // 월드 코너들
    FVector WorldCorners[8];
    for (int i = 0; i < 8; ++i)
    {
        WorldCorners[i] = BoxWorldNoScale.TransformPosition(LocalCorners[i]);
    }

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    static const int Edge[12][2] = {
        {0,1},{1,3},{3,2},{2,0}, // bottom
        {4,5},{5,7},{7,6},{6,4}, // top
        {0,4},{1,5},{2,6},{3,7}  // verticals
    };

    for (int i = 0; i < 12; ++i)
    {
        StartPoints.Add(WorldCorners[Edge[i][0]]);
        EndPoints.Add(WorldCorners[Edge[i][1]]);
        Colors.Add(Color);
    }

    Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void PhysicsAssetViewerState::DrawCapsule(URenderer* Renderer, const FTransform& BoneTransform, const FKCapsuleElem& Capsule, const FVector4& Color) const
{
    // Capsule rotation (stored in degrees, MakeFromEulerZYX converts internally)
    FQuat CapsuleRot = FQuat::MakeFromEulerZYX(Capsule.Rotation);

    // Transform center from bone-local to world space
    // Just use the bone transform directly on the capsule center
    FVector WorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Capsule.Center);

    // Combine rotations: bone world rotation * capsule local rotation
    FQuat WorldRot = BoneTransform.Rotation * CapsuleRot;

    FMatrix WorldMatrix = FMatrix::FromTRS(WorldCenter, WorldRot, FVector(1, 1, 1));

    float Radius = Capsule.Radius;
    float HalfLength = Capsule.Length * 0.5f;

    const int NumSlices = 8;
    const int NumHemisphereSegments = 8;

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    // Capsule is aligned along X-axis (PhysX convention)
    TArray<FVector> RightRing;   // +X end
    TArray<FVector> LeftRing;    // -X end
    RightRing.Reserve(NumSlices);
    LeftRing.Reserve(NumSlices);

    // Circular rings at +X and -X ends
    for (int i = 0; i < NumSlices; ++i)
    {
        float a = (static_cast<float>(i) / NumSlices) * TWO_PI;
        float y = Radius * std::sin(a);
        float z = Radius * std::cos(a);
        RightRing.Add(FVector(+HalfLength, y, z));
        LeftRing.Add(FVector(-HalfLength, y, z));
    }

    // Right and left circular rings
    for (int i = 0; i < NumSlices; ++i)
    {
        int j = (i + 1) % NumSlices;

        // Right ring (+X)
        StartPoints.Add(RightRing[i] * WorldMatrix);
        EndPoints.Add(RightRing[j] * WorldMatrix);
        Colors.Add(Color);

        // Left ring (-X)
        StartPoints.Add(LeftRing[i] * WorldMatrix);
        EndPoints.Add(LeftRing[j] * WorldMatrix);
        Colors.Add(Color);
    }

    // Vertical lines connecting rings
    for (int i = 0; i < NumSlices; ++i)
    {
        StartPoints.Add(RightRing[i] * WorldMatrix);
        EndPoints.Add(LeftRing[i] * WorldMatrix);
        Colors.Add(Color);
    }

    // Hemispheres (right and left) - two arcs each
    auto AddHemisphereArcs = [&](float CenterXSign)
    {
        float CenterX = CenterXSign * HalfLength;

        for (int i = 0; i < NumHemisphereSegments; ++i)
        {
            float t0 = (static_cast<float>(i) / NumHemisphereSegments) * PI;
            float t1 = (static_cast<float>(i + 1) / NumHemisphereSegments) * PI;

            // XY plane arc
            FVector PlaneXY0(CenterX + CenterXSign * Radius * std::sin(t0), Radius * std::cos(t0), 0.0f);
            FVector PlaneXY1(CenterX + CenterXSign * Radius * std::sin(t1), Radius * std::cos(t1), 0.0f);
            StartPoints.Add(PlaneXY0 * WorldMatrix);
            EndPoints.Add(PlaneXY1 * WorldMatrix);
            Colors.Add(Color);

            // XZ plane arc
            FVector PlaneXZ0(CenterX + CenterXSign * Radius * std::sin(t0), 0.0f, Radius * std::cos(t0));
            FVector PlaneXZ1(CenterX + CenterXSign * Radius * std::sin(t1), 0.0f, Radius * std::cos(t1));
            StartPoints.Add(PlaneXZ0 * WorldMatrix);
            EndPoints.Add(PlaneXZ1 * WorldMatrix);
            Colors.Add(Color);
        }
    };

    AddHemisphereArcs(+1.0f); // Right hemisphere (+X)
    AddHemisphereArcs(-1.0f); // Left hemisphere (-X)

    Renderer->AddLines(StartPoints, EndPoints, Colors);
}

// ============================================================================
// 반투명 솔리드 Body 드로잉 (언리얼 스타일)
// ============================================================================

void PhysicsAssetViewerState::DrawPhysicsBodiesSolid(URenderer* Renderer) const
{
    if (!bShowBodies || !PhysicsAsset || !Renderer)
        return;

    // SkeletalMeshComponent에서 본 위치 가져오기
    USkeletalMeshComponent* SkelComp = nullptr;
    if (PreviewActor)
    {
        SkelComp = PreviewActor->GetSkeletalMeshComponent();
    }

    const FSkeleton* Skeleton = CurrentMesh ? CurrentMesh->GetSkeleton() : nullptr;

    // 프리미티브 배치 시작
    Renderer->BeginPrimitiveBatch();

    // 기본 색상 (언리얼 스타일 - 반투명)
    const FVector4 NormalColor(1.0f, 1.0f, 1.0f, 0.15f);      // 반투명 흰색 (일반 Body)
    const FVector4 SelectedColor(0.3f, 0.7f, 1.0f, 0.4f);     // 반투명 파란색 (선택된 Body)
    const FVector4 ShapeSelectedColor(1.0f, 1.0f, 0.0f, 0.5f); // 반투명 노란색 (선택된 Shape)

    // 모든 BodySetup 순회
    for (int32 BodyIdx = 0; BodyIdx < PhysicsAsset->BodySetups.Num(); ++BodyIdx)
    {
        UBodySetup* Setup = PhysicsAsset->BodySetups[BodyIdx];
        if (!Setup)
            continue;

        // 본의 월드 트랜스폼 가져오기
        FTransform BoneTransform;
        if (SkelComp && Skeleton)
        {
            int32 BoneIndex = -1;
            for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
            {
                if (Skeleton->Bones[i].Name == Setup->BoneName.ToString())
                {
                    BoneIndex = i;
                    break;
                }
            }

            if (BoneIndex >= 0)
            {
                BoneTransform = SkelComp->GetBoneWorldTransform(BoneIndex);
            }
            else
            {
                BoneTransform = SkelComp->GetWorldTransform();
            }
        }

        // Body가 선택되었는지 확인
        bool bBodySelected = (EditMode == EPhysicsAssetEditMode::Body && SelectedBodyIndex == BodyIdx);
        bool bShapeMode = (EditMode == EPhysicsAssetEditMode::Shape && SelectedBodyIndex == BodyIdx);

        // Sphere shapes 그리기
        for (int32 i = 0; i < Setup->AggGeom.SphereElems.Num(); ++i)
        {
            const FKSphereElem& Sphere = Setup->AggGeom.SphereElems[i];

            FVector4 Color = NormalColor;
            if (bBodySelected)
                Color = SelectedColor;
            else if (bShapeMode && SelectedShapeType == EAggCollisionShape::Sphere && SelectedShapeIndex == i)
                Color = ShapeSelectedColor;

            // 메시 생성
            FMeshData SphereMesh;
            FPrimitiveGeometry::GenerateSphere(SphereMesh, Sphere.Radius, 12, 6, Color);

            // Shape의 로컬 트랜스폼 계산
            // Compose world (ignore parent scale for translation)
            const FVector SphereWorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Sphere.Center);
            const FQuat   SphereWorldRot    = BoneTransform.Rotation; // no local rotation for sphere
            FMatrix WorldMatrix = FMatrix::FromTRS(SphereWorldCenter, SphereWorldRot, FVector(1, 1, 1));

            Renderer->AddPrimitiveData(SphereMesh, WorldMatrix);
        }

        // Box shapes 그리기
        for (int32 i = 0; i < Setup->AggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Box = Setup->AggGeom.BoxElems[i];

            FVector4 Color = NormalColor;
            if (bBodySelected)
                Color = SelectedColor;
            else if (bShapeMode && SelectedShapeType == EAggCollisionShape::Box && SelectedShapeIndex == i)
                Color = ShapeSelectedColor;

            // 메시 생성
            FMeshData BoxMesh;
            FPrimitiveGeometry::GenerateBox(BoxMesh, FVector(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f), Color);

            // Shape의 로컬 트랜스폼 계산
            // MakeFromEulerZYX expects degrees
            FQuat BoxRot = FQuat::MakeFromEulerZYX(Box.Rotation);
            // Compose world (ignore parent scale for translation)
            const FVector BoxWorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Box.Center);
            const FQuat   BoxWorldRot    = BoneTransform.Rotation * BoxRot;
            FMatrix WorldMatrix = FMatrix::FromTRS(BoxWorldCenter, BoxWorldRot, FVector(1, 1, 1));

            Renderer->AddPrimitiveData(BoxMesh, WorldMatrix);
        }

        // Capsule shapes 그리기
        for (int32 i = 0; i < Setup->AggGeom.SphylElems.Num(); ++i)
        {
            const FKCapsuleElem& Capsule = Setup->AggGeom.SphylElems[i];

            FVector4 Color = NormalColor;
            if (bBodySelected)
                Color = SelectedColor;
            else if (bShapeMode && SelectedShapeType == EAggCollisionShape::Capsule && SelectedShapeIndex == i)
                Color = ShapeSelectedColor;

            // 메시 생성
            FMeshData CapsuleMesh;
            FPrimitiveGeometry::GenerateCapsule(CapsuleMesh, Capsule.Radius, Capsule.Length * 0.5f, 12, 4, Color);

            // Shape의 로컬 트랜스폼 계산
            // MakeFromEulerZYX expects degrees
            FQuat CapsuleRot = FQuat::MakeFromEulerZYX(Capsule.Rotation);
            FTransform ShapeLocalTransform(Capsule.Center, CapsuleRot, FVector(1, 1, 1));
            FTransform ShapeWorldTransform = BoneTransform.GetWorldTransform(ShapeLocalTransform);
            // Primitive capsule is Z-axis aligned; physics capsule long axis is X.
            // Rotate mesh -90deg about Y to map Z->X for correct orientation.
            {
                const FQuat CapsuleWorldRot = BoneTransform.Rotation * CapsuleRot;
                const FVector CapsuleWorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Capsule.Center);
                FQuat AlignZtoX = FQuat::MakeFromEulerZYX(FVector(0, -90, 0));
                FMatrix WorldMatrix = FMatrix::FromTRS(CapsuleWorldCenter, CapsuleWorldRot * AlignZtoX, FVector(1, 1, 1));
                Renderer->AddPrimitiveData(CapsuleMesh, WorldMatrix);
            }
            continue;
        }
    }

    // 프리미티브 배치 종료 및 렌더링
    Renderer->EndPrimitiveBatch();
}

void PhysicsAssetViewerState::DrawConstraints(URenderer* Renderer) const
{
    if (!bShowConstraints || !PhysicsAsset || !Renderer)
        return;

    USkeletalMeshComponent* SkelComp = nullptr;
    if (PreviewActor)
    {
        SkelComp = PreviewActor->GetSkeletalMeshComponent();
    }

    const FSkeleton* Skeleton = CurrentMesh ? CurrentMesh->GetSkeleton() : nullptr;
    if (!Skeleton || !SkelComp)
        return;

    // 라인용 배열 (연결선, 좌표축)
    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> LineColors;

    // 반투명 메시 배치 시작
    Renderer->BeginPrimitiveBatch();

    // 색상 정의 (언리얼 스타일 - 반투명)
    const FVector4 SwingConeColor(0.0f, 0.8f, 0.0f, 0.25f);           // 반투명 녹색 - Swing
    const FVector4 SwingConeSelectedColor(0.2f, 1.0f, 0.2f, 0.4f);    // 반투명 밝은 녹색 - 선택된 Swing
    const FVector4 TwistArcColor(1.0f, 0.5f, 0.0f, 0.3f);             // 반투명 주황 - Twist
    const FVector4 TwistArcSelectedColor(1.0f, 0.7f, 0.2f, 0.5f);     // 반투명 밝은 주황 - 선택된 Twist
    const FVector4 ConnectionColor(0.3f, 0.3f, 0.6f, 1.0f);           // 파란색 - 연결선
    const FVector4 SelectedConnectionColor(1.0f, 1.0f, 0.0f, 1.0f);   // 노란색 - 선택된 연결선

    for (int32 ConstraintIdx = 0; ConstraintIdx < PhysicsAsset->ConstraintSetups.Num(); ++ConstraintIdx)
    {
        UPhysicsConstraintSetup* Constraint = PhysicsAsset->ConstraintSetups[ConstraintIdx];
        if (!Constraint)
            continue;

        // 두 Body의 본 찾기
        int32 BoneIndex1 = -1;
        int32 BoneIndex2 = -1;

        for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Constraint->ConstraintBone1.ToString())
                BoneIndex1 = i;
            if (Skeleton->Bones[i].Name == Constraint->ConstraintBone2.ToString())
                BoneIndex2 = i;
        }

        if (BoneIndex1 < 0 || BoneIndex2 < 0)
            continue;

        // 두 본의 월드 트랜스폼
        FTransform Bone1Transform = SkelComp->GetBoneWorldTransform(BoneIndex1);
        FTransform Bone2Transform = SkelComp->GetBoneWorldTransform(BoneIndex2);

        FVector Pos1 = Bone1Transform.Translation;
        FVector Pos2 = Bone2Transform.Translation;

        bool bSelected = (EditMode == EPhysicsAssetEditMode::Constraint && SelectedConstraintIndex == ConstraintIdx);

        // Constraint 위치 (Child 본 위치 사용)
        FVector ConstraintPos = Pos2;
        // Constraint 회전 = 부모 본 회전 * Constraint 로컬 회전 (기즈모 조작 결과 반영)
        FQuat ConstraintLocalRot = FQuat::MakeFromEulerZYX(Constraint->ConstraintRotationInBody1);
        FQuat ConstraintRot = Bone1Transform.Rotation * ConstraintLocalRot;

        // 크기: 본 수준의 작은 크기 (1/3로 축소)
        float BoneDistance = (Pos2 - Pos1).Size();
        float VisualScale = FMath::Clamp(BoneDistance * 0.007f, 0.05f, 0.15f);

        // 연결선
        StartPoints.Add(Pos1);
        EndPoints.Add(Pos2);
        LineColors.Add(bSelected ? SelectedConnectionColor : ConnectionColor);

        // ===== Swing Cone (반투명 메시) =====
        float Swing1Limit = 0.0f;
        float Swing2Limit = 0.0f;

        if (Constraint->Swing1Motion == EAngularConstraintMotion::Limited)
            Swing1Limit = Constraint->Swing1LimitAngle;
        else if (Constraint->Swing1Motion == EAngularConstraintMotion::Free)
            Swing1Limit = 45.0f;  // Free는 45도로 표시 (너무 크지 않게)

        if (Constraint->Swing2Motion == EAngularConstraintMotion::Limited)
            Swing2Limit = Constraint->Swing2LimitAngle;
        else if (Constraint->Swing2Motion == EAngularConstraintMotion::Free)
            Swing2Limit = 45.0f;

        if (Swing1Limit > 0.0f || Swing2Limit > 0.0f)
        {
            FMeshData ConeMesh;
            FVector4 ConeColor = bSelected ? SwingConeSelectedColor : SwingConeColor;
            FPrimitiveGeometry::GenerateSwingCone(ConeMesh, Swing1Limit, Swing2Limit, VisualScale, 16, ConeColor);

            FMatrix WorldMatrix = FMatrix::FromTRS(ConstraintPos, ConstraintRot, FVector(1, 1, 1));
            Renderer->AddPrimitiveData(ConeMesh, WorldMatrix);
        }

        // ===== Twist Arc (반투명 메시) =====
        float TwistLimit = 0.0f;
        if (Constraint->TwistMotion == EAngularConstraintMotion::Limited)
            TwistLimit = Constraint->TwistLimitAngle;
        else if (Constraint->TwistMotion == EAngularConstraintMotion::Free)
            TwistLimit = 90.0f;

        if (TwistLimit > 0.0f)
        {
            FMeshData ArcMesh;
            FVector4 ArcColor = bSelected ? TwistArcSelectedColor : TwistArcColor;
            FPrimitiveGeometry::GenerateTwistArc(ArcMesh, TwistLimit, VisualScale * 0.7f, VisualScale * 0.15f, 16, ArcColor);

            FMatrix WorldMatrix = FMatrix::FromTRS(ConstraintPos, ConstraintRot, FVector(1, 1, 1));
            Renderer->AddPrimitiveData(ArcMesh, WorldMatrix);
        }

        // 선택된 경우 좌표축 표시
        if (bSelected)
        {
            float AxisLength = VisualScale * 0.8f;
            FVector XAxis = ConstraintRot.RotateVector(FVector(1, 0, 0));
            FVector YAxis = ConstraintRot.RotateVector(FVector(0, 1, 0));
            FVector ZAxis = ConstraintRot.RotateVector(FVector(0, 0, 1));

            // X축 (빨강 - Twist)
            StartPoints.Add(ConstraintPos);
            EndPoints.Add(ConstraintPos + XAxis * AxisLength);
            LineColors.Add(FVector4(1, 0, 0, 1));

            // Y축 (초록 - Swing1)
            StartPoints.Add(ConstraintPos);
            EndPoints.Add(ConstraintPos + YAxis * AxisLength);
            LineColors.Add(FVector4(0, 1, 0, 1));

            // Z축 (파랑 - Swing2)
            StartPoints.Add(ConstraintPos);
            EndPoints.Add(ConstraintPos + ZAxis * AxisLength);
            LineColors.Add(FVector4(0, 0, 1, 1));
        }
    }

    // 반투명 메시 배치 종료
    Renderer->EndPrimitiveBatch();

    // 라인 렌더링
    if (!StartPoints.empty())
    {
        Renderer->AddLines(StartPoints, EndPoints, LineColors);
    }
}

// Ray-Sphere 교차 검사 헬퍼
static bool RaySphereIntersect(const FRay& Ray, const FVector& Center, float Radius, float& OutT)
{
    FVector OC = Ray.Origin - Center;
    float A = FVector::Dot(Ray.Direction, Ray.Direction);
    float B = 2.0f * FVector::Dot(OC, Ray.Direction);
    float C = FVector::Dot(OC, OC) - Radius * Radius;
    float Discriminant = B * B - 4 * A * C;

    if (Discriminant < 0) return false;

    float T = (-B - sqrtf(Discriminant)) / (2.0f * A);
    if (T < 0)
    {
        T = (-B + sqrtf(Discriminant)) / (2.0f * A);
        if (T < 0) return false;
    }
    OutT = T;
    return true;
}

// Ray-Box 교차 검사 헬퍼 (OBB)
static bool RayBoxIntersect(const FRay& Ray, const FVector& Center, const FVector& HalfExtent, const FQuat& Rotation, float& OutT)
{
    // Ray를 Box의 로컬 공간으로 변환
    FQuat InvRot = Rotation.Conjugate();
    FVector LocalOrigin = InvRot.RotateVector(Ray.Origin - Center);
    FVector LocalDir = InvRot.RotateVector(Ray.Direction);

    // AABB 교차 검사
    float TMin = -FLT_MAX;
    float TMax = FLT_MAX;

    for (int i = 0; i < 3; ++i)
    {
        float O = (i == 0) ? LocalOrigin.X : ((i == 1) ? LocalOrigin.Y : LocalOrigin.Z);
        float D = (i == 0) ? LocalDir.X : ((i == 1) ? LocalDir.Y : LocalDir.Z);
        float E = (i == 0) ? HalfExtent.X : ((i == 1) ? HalfExtent.Y : HalfExtent.Z);

        if (fabsf(D) < 0.0001f)
        {
            if (O < -E || O > E) return false;
        }
        else
        {
            float T1 = (-E - O) / D;
            float T2 = (E - O) / D;
            if (T1 > T2) std::swap(T1, T2);
            TMin = std::max(TMin, T1);
            TMax = std::min(TMax, T2);
            if (TMin > TMax) return false;
        }
    }

    OutT = TMin >= 0 ? TMin : TMax;
    return OutT >= 0;
}

// Ray-Capsule 교차 검사 헬퍼
static bool RayCapsuleIntersect(const FRay& Ray, const FVector& Center, float Radius, float HalfLength, const FQuat& Rotation, float& OutT)
{
    // Capsule은 Z축 방향으로 정렬된 것으로 가정
    // Ray를 Capsule 로컬 공간으로 변환
    FQuat InvRot = Rotation.Conjugate();
    FVector LocalOrigin = InvRot.RotateVector(Ray.Origin - Center);
    FVector LocalDir = InvRot.RotateVector(Ray.Direction);

    // 먼저 무한 실린더와 교차 검사
    float A = LocalDir.Y * LocalDir.Y + LocalDir.Z * LocalDir.Z;
    float B = 2.0f * (LocalOrigin.Y * LocalDir.Y + LocalOrigin.Z * LocalDir.Z);
    float C = LocalOrigin.Y * LocalOrigin.Y + LocalOrigin.Z * LocalOrigin.Z - Radius * Radius;

    float Discriminant = B * B - 4 * A * C;
    float ClosestT = FLT_MAX;
    bool bHit = false;

    if (A > 0.0001f && Discriminant >= 0)
    {
        float T1 = (-B - sqrtf(Discriminant)) / (2.0f * A);
        float T2 = (-B + sqrtf(Discriminant)) / (2.0f * A);

        // 실린더 본체 부분 검사
        for (float T : {T1, T2})
        {
            if (T >= 0)
            {
                float X = LocalOrigin.X + T * LocalDir.X;
                if (X >= -HalfLength && X <= HalfLength)
                {
                    if (T < ClosestT) { ClosestT = T; bHit = true; }
                }
            }
        }
    }

    // 반구 캡 검사
    float SphereT;
    FVector TopCenter(+HalfLength, 0, 0);
    FVector BotCenter(-HalfLength, 0, 0);

    FVector TopOrigin = LocalOrigin - TopCenter;
    float TopA = FVector::Dot(LocalDir, LocalDir);
    float TopB = 2.0f * FVector::Dot(TopOrigin, LocalDir);
    float TopC = FVector::Dot(TopOrigin, TopOrigin) - Radius * Radius;
    float TopDisc = TopB * TopB - 4 * TopA * TopC;

    if (TopDisc >= 0)
    {
        SphereT = (-TopB - sqrtf(TopDisc)) / (2.0f * TopA);
        if (SphereT >= 0)
        {
            FVector HitPoint = LocalOrigin + LocalDir * SphereT;
            if (HitPoint.X >= HalfLength && SphereT < ClosestT) { ClosestT = SphereT; bHit = true; }
        }
    }

    FVector BotOrigin = LocalOrigin - BotCenter;
    float BotA = FVector::Dot(LocalDir, LocalDir);
    float BotB = 2.0f * FVector::Dot(BotOrigin, LocalDir);
    float BotC = FVector::Dot(BotOrigin, BotOrigin) - Radius * Radius;
    float BotDisc = BotB * BotB - 4 * BotA * BotC;

    if (BotDisc >= 0)
    {
        SphereT = (-BotB - sqrtf(BotDisc)) / (2.0f * BotA);
        if (SphereT >= 0)
        {
            FVector HitPoint = LocalOrigin + LocalDir * SphereT;
            if (HitPoint.X <= -HalfLength && SphereT < ClosestT) { ClosestT = SphereT; bHit = true; }
        }
    }

    OutT = ClosestT;
    return bHit;
}

bool PhysicsAssetViewerState::PickBodyOrShape(const FRay& Ray,
                                               int32& OutBodyIndex,
                                               EAggCollisionShape::Type& OutShapeType,
                                               int32& OutShapeIndex,
                                               float& OutDistance) const
{
    if (!PhysicsAsset || !PreviewActor) return false;

    USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
    const FSkeleton* Skeleton = CurrentMesh ? CurrentMesh->GetSkeleton() : nullptr;
    if (!SkelComp || !Skeleton) return false;

    float ClosestDist = FLT_MAX;
    bool bHit = false;
    int32 HitBodyIndex = -1;
    EAggCollisionShape::Type HitShapeType = EAggCollisionShape::Unknown;
    int32 HitShapeIndex = -1;

    // 모든 BodySetup 순회
    for (int32 BodyIdx = 0; BodyIdx < PhysicsAsset->BodySetups.Num(); ++BodyIdx)
    {
        UBodySetup* Setup = PhysicsAsset->BodySetups[BodyIdx];
        if (!Setup) continue;

        // 본의 월드 트랜스폼 가져오기
        FTransform BoneTransform;
        int32 BoneIndex = -1;
        for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Setup->BoneName.ToString())
            {
                BoneIndex = i;
                break;
            }
        }

        if (BoneIndex >= 0)
            BoneTransform = SkelComp->GetBoneWorldTransform(BoneIndex);
        else
            BoneTransform = SkelComp->GetWorldTransform();

        // Sphere 검사
        for (int32 i = 0; i < Setup->AggGeom.SphereElems.Num(); ++i)
            {
                const FKSphereElem& Sphere = Setup->AggGeom.SphereElems[i];
                // Compose world center without inheriting parent scale
                const FVector SphereWorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Sphere.Center);

                float T;
                if (RaySphereIntersect(Ray, SphereWorldCenter, Sphere.Radius, T))
                {
                if (T < ClosestDist)
                {
                    ClosestDist = T;
                    HitBodyIndex = BodyIdx;
                    HitShapeType = EAggCollisionShape::Sphere;
                    HitShapeIndex = i;
                    bHit = true;
                }
            }
        }

        // Box 검사
        for (int32 i = 0; i < Setup->AggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Box = Setup->AggGeom.BoxElems[i];
            // MakeFromEulerZYX expects degrees
            FQuat BoxRot = FQuat::MakeFromEulerZYX(Box.Rotation);
            // Compose world (ignore parent scale for translation)
            const FVector BoxWorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Box.Center);
            const FQuat   BoxWorldRot    = BoneTransform.Rotation * BoxRot;

            float T;
            FVector HalfExtent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);
            if (RayBoxIntersect(Ray, BoxWorldCenter, HalfExtent, BoxWorldRot, T))
            {
                if (T < ClosestDist)
                {
                    ClosestDist = T;
                    HitBodyIndex = BodyIdx;
                    HitShapeType = EAggCollisionShape::Box;
                    HitShapeIndex = i;
                    bHit = true;
                }
            }
        }

        // Capsule 검사
        for (int32 i = 0; i < Setup->AggGeom.SphylElems.Num(); ++i)
        {
            const FKCapsuleElem& Capsule = Setup->AggGeom.SphylElems[i];
            // MakeFromEulerZYX expects degrees
            FQuat CapsuleRot = FQuat::MakeFromEulerZYX(Capsule.Rotation);
            // Compose world (ignore parent scale for translation)
            const FVector CapsuleWorldCenter = BoneTransform.Translation + BoneTransform.Rotation.RotateVector(Capsule.Center);
            const FQuat   CapsuleWorldRot    = BoneTransform.Rotation * CapsuleRot;

            float T;
            if (RayCapsuleIntersect(Ray, CapsuleWorldCenter, Capsule.Radius, Capsule.Length * 0.5f, CapsuleWorldRot, T))
            {
                if (T < ClosestDist)
                {
                    ClosestDist = T;
                    HitBodyIndex = BodyIdx;
                    HitShapeType = EAggCollisionShape::Capsule;
                    HitShapeIndex = i;
                    bHit = true;
                }
            }
        }
    }

    if (bHit)
    {
        OutBodyIndex = HitBodyIndex;
        OutShapeType = HitShapeType;
        OutShapeIndex = HitShapeIndex;
        OutDistance = ClosestDist;
    }

    return bHit;
}

// Ray와 선분 사이의 최단 거리 계산 헬퍼
static float RaySegmentDistance(const FRay& Ray, const FVector& SegStart, const FVector& SegEnd, float& OutRayT)
{
    FVector SegDir = SegEnd - SegStart;
    float SegLength = SegDir.Size();
    if (SegLength < 0.0001f)
    {
        // 선분 길이가 0에 가까우면 점으로 처리
        FVector ToPoint = SegStart - Ray.Origin;
        OutRayT = FVector::Dot(ToPoint, Ray.Direction);
        FVector ClosestOnRay = Ray.Origin + Ray.Direction * FMath::Max(0.0f, OutRayT);
        return (SegStart - ClosestOnRay).Size();
    }

    SegDir = SegDir / SegLength;  // normalize

    FVector W0 = Ray.Origin - SegStart;
    float A = FVector::Dot(Ray.Direction, Ray.Direction);  // always 1 if normalized
    float B = FVector::Dot(Ray.Direction, SegDir);
    float C = FVector::Dot(SegDir, SegDir);  // always 1 if normalized
    float D = FVector::Dot(Ray.Direction, W0);
    float E = FVector::Dot(SegDir, W0);

    float Denom = A * C - B * B;

    float RayT, SegT;

    if (FMath::Abs(Denom) < 0.0001f)
    {
        // 평행한 경우
        RayT = 0.0f;
        SegT = E / C;
    }
    else
    {
        RayT = (B * E - C * D) / Denom;
        SegT = (A * E - B * D) / Denom;
    }

    // Ray는 양의 방향만
    RayT = FMath::Max(0.0f, RayT);

    // 선분은 [0, SegLength] 범위로 클램프
    SegT = FMath::Clamp(SegT, 0.0f, SegLength);

    FVector ClosestOnRay = Ray.Origin + Ray.Direction * RayT;
    FVector ClosestOnSeg = SegStart + SegDir * SegT;

    OutRayT = RayT;
    return (ClosestOnRay - ClosestOnSeg).Size();
}

bool PhysicsAssetViewerState::PickConstraint(const FRay& Ray,
                                              int32& OutConstraintIndex,
                                              float& OutDistance) const
{
    if (!PhysicsAsset || !PreviewActor) return false;

    USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
    const FSkeleton* Skeleton = CurrentMesh ? CurrentMesh->GetSkeleton() : nullptr;
    if (!SkelComp || !Skeleton) return false;

    float ClosestT = FLT_MAX;
    int32 HitConstraintIndex = -1;

    for (int32 ConstraintIdx = 0; ConstraintIdx < PhysicsAsset->ConstraintSetups.Num(); ++ConstraintIdx)
    {
        UPhysicsConstraintSetup* Constraint = PhysicsAsset->ConstraintSetups[ConstraintIdx];
        if (!Constraint) continue;

        // 두 본의 인덱스 찾기
        int32 BoneIndex1 = -1;
        int32 BoneIndex2 = -1;
        for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Constraint->ConstraintBone1.ToString())
                BoneIndex1 = i;
            if (Skeleton->Bones[i].Name == Constraint->ConstraintBone2.ToString())
                BoneIndex2 = i;
        }

        if (BoneIndex1 < 0 || BoneIndex2 < 0)
            continue;

        // Constraint 위치 (Child 본 = Pos2)
        FVector Pos1 = SkelComp->GetBoneWorldTransform(BoneIndex1).Translation;
        FVector Pos2 = SkelComp->GetBoneWorldTransform(BoneIndex2).Translation;
        FVector ConstraintPos = Pos2;

        // 피킹 반경: 시각화 크기와 동일
        float BoneDistance = (Pos2 - Pos1).Size();
        float PickRadius = FMath::Clamp(BoneDistance * 0.007f, 0.05f, 0.15f);

        // Ray-Sphere 교차 검사
        float T;
        if (RaySphereIntersect(Ray, ConstraintPos, PickRadius, T))
        {
            if (T < ClosestT)
            {
                ClosestT = T;
                HitConstraintIndex = ConstraintIdx;
            }
        }
    }

    if (HitConstraintIndex >= 0)
    {
        OutConstraintIndex = HitConstraintIndex;
        OutDistance = ClosestT;
        return true;
    }

    return false;
}

// ============================================================================
// Cloth Paint 관련 함수들
// ============================================================================

bool PhysicsAssetViewerState::PickClothVertex(const FRay& Ray,
                                               int32& OutClothVertexIndex,
                                               float& OutDistance) const
{
    if (!PreviewActor) return false;

    // ClothComponent 찾기
    UClothComponent* ClothComp = nullptr;
    USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
    if (SkelComp)
    {
        ClothComp = SkelComp->GetInternalClothComponent();
    }

    if (!ClothComp || ClothComp->GetClothVertexCount() == 0)
    {
        return false;
    }

    float ClosestT = FLT_MAX;
    int32 HitVertexIndex = -1;
    const float VertexPickRadius = ClothPaintBrushRadius * 0.5f;  // 브러시 반경의 절반

    // 모든 Cloth 정점 순회
    for (int32 i = 0; i < ClothComp->GetClothVertexCount(); ++i)
    {
        FVector VertexPos = ClothComp->GetClothVertexPosition(i);

        // Ray-Sphere 교차 검사 (각 정점을 작은 구로 취급)
        float T;
        if (RaySphereIntersect(Ray, VertexPos, VertexPickRadius, T))
        {
            if (T < ClosestT)
            {
                ClosestT = T;
                HitVertexIndex = i;
            }
        }
    }

    if (HitVertexIndex >= 0)
    {
        OutClothVertexIndex = HitVertexIndex;
        OutDistance = ClosestT;
        return true;
    }

    return false;
}

void PhysicsAssetViewerState::DrawClothWeights(URenderer* Renderer) const
{
    if (!Renderer || !bShowClothWeightVisualization) return;
    // ClothPaint 모드에서만 시각화
    if (EditMode != EPhysicsAssetEditMode::ClothPaint) return;
    if (!PreviewActor) return;

    USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
    if (!SkelComp) return;

    USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
    if (!SkelMesh || !SkelMesh->GetSkeletalMeshData()) return;

    const auto& MeshDataPtr = SkelMesh->GetSkeletalMeshData();
    const auto& Vertices = MeshDataPtr->Vertices;
    const auto& GroupInfos = MeshDataPtr->GroupInfos;
    const auto& Indices = MeshDataPtr->Indices;

    // SkeletalMeshComponent의 월드 변환 가져오기
    FTransform WorldTransform = SkelComp->GetWorldTransform();

    // Weight에 따른 색상 계산 람다 (반투명하게)
    auto GetWeightColor = [](float Weight) -> FVector4 {
        // 0(Red/고정) -> 0.5(Yellow) -> 1(Green/자유)
        float Alpha = 0.6f;  // 반투명
        if (Weight < 0.5f)
        {
            float t = Weight * 2.0f;
            return FVector4(1.0f, t, 0.0f, Alpha);
        }
        else
        {
            float t = (Weight - 0.5f) * 2.0f;
            return FVector4(1.0f - t, 1.0f, 0.0f, Alpha);
        }
    };

    // FMeshData로 삼각형 메시 생성
    FMeshData ClothMeshData;

    // Cloth Section의 삼각형을 시각화
    uint32 VertexIndex = 0;
    for (const auto& Group : GroupInfos)
    {
        if (!Group.bEnableCloth)
            continue;

        // 삼각형 단위로 순회 (3개씩)
        for (uint32 i = 0; i + 2 < Group.IndexCount; i += 3)
        {
            uint32 Idx0 = Indices[Group.StartIndex + i];
            uint32 Idx1 = Indices[Group.StartIndex + i + 1];
            uint32 Idx2 = Indices[Group.StartIndex + i + 2];

            // 버텍스 위치 (월드 좌표)
            FVector P0 = WorldTransform.TransformPosition(Vertices[Idx0].Position);
            FVector P1 = WorldTransform.TransformPosition(Vertices[Idx1].Position);
            FVector P2 = WorldTransform.TransformPosition(Vertices[Idx2].Position);

            // 노멀 계산 (면 노멀)
            FVector Edge1 = P1 - P0;
            FVector Edge2 = P2 - P0;
            FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();

            // 각 버텍스의 Weight 가져오기 (ClothVertexWeights 맵에서)
            float W0 = 1.0f, W1 = 1.0f, W2 = 1.0f;
            if (ClothVertexWeights)
            {
                auto It0 = ClothVertexWeights->find(Idx0);
                auto It1 = ClothVertexWeights->find(Idx1);
                auto It2 = ClothVertexWeights->find(Idx2);
                if (It0 != ClothVertexWeights->end()) W0 = It0->second;
                if (It1 != ClothVertexWeights->end()) W1 = It1->second;
                if (It2 != ClothVertexWeights->end()) W2 = It2->second;
            }

            // 각 버텍스의 색상
            FVector4 C0 = GetWeightColor(W0);
            FVector4 C1 = GetWeightColor(W1);
            FVector4 C2 = GetWeightColor(W2);

            // 삼각형 버텍스 추가
            ClothMeshData.Vertices.Add(P0);
            ClothMeshData.Vertices.Add(P1);
            ClothMeshData.Vertices.Add(P2);

            ClothMeshData.Color.Add(C0);
            ClothMeshData.Color.Add(C1);
            ClothMeshData.Color.Add(C2);

            ClothMeshData.Normal.Add(Normal);
            ClothMeshData.Normal.Add(Normal);
            ClothMeshData.Normal.Add(Normal);

            ClothMeshData.UV.Add(FVector2D(0, 0));
            ClothMeshData.UV.Add(FVector2D(1, 0));
            ClothMeshData.UV.Add(FVector2D(0.5f, 1));

            // 인덱스 추가
            ClothMeshData.Indices.Add(VertexIndex);
            ClothMeshData.Indices.Add(VertexIndex + 1);
            ClothMeshData.Indices.Add(VertexIndex + 2);

            VertexIndex += 3;
        }
    }

    if (ClothMeshData.Vertices.Num() > 0)
    {
        Renderer->BeginPrimitiveBatch();
        Renderer->AddPrimitiveData(ClothMeshData, FMatrix::Identity());
        Renderer->EndPrimitiveBatch();
    }
}

void PhysicsAssetViewerState::ApplyBrushToClothVertices(const FVector& BrushCenter, float BrushRadius,
                                                         float BrushStrength, float BrushFalloff, float PaintValue)
{
    if (!PreviewActor) return;

    USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
    if (!SkelComp) return;

    USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
    if (!SkelMesh || !SkelMesh->GetSkeletalMeshData()) return;

    const auto& MeshData = SkelMesh->GetSkeletalMeshData();
    const auto& Vertices = MeshData->Vertices;
    const auto& GroupInfos = MeshData->GroupInfos;
    const auto& Indices = MeshData->Indices;

    FTransform WorldTransform = SkelComp->GetWorldTransform();

    // Cloth가 활성화된 그룹의 버텍스들만 처리
    std::set<uint32> ClothVertexIndices;
    for (const auto& Group : GroupInfos)
    {
        if (!Group.bEnableCloth)
            continue;

        for (uint32 i = 0; i < Group.IndexCount; ++i)
        {
            uint32 VertIdx = Indices[Group.StartIndex + i];
            ClothVertexIndices.insert(VertIdx);
        }
    }

    // 맵이 없으면 생성
    if (!ClothVertexWeights)
    {
        ClothVertexWeights = std::make_unique<std::unordered_map<uint32, float>>();
    }

    // 브러시 영역 내 정점에 Weight 적용
    for (uint32 VertIdx : ClothVertexIndices)
    {
        FVector LocalPos = Vertices[VertIdx].Position;
        FVector WorldPos = WorldTransform.TransformPosition(LocalPos);
        float Distance = (WorldPos - BrushCenter).Size();

        if (Distance <= BrushRadius)
        {
            // Falloff 계산: 중심에서 멀어질수록 영향 감소
            float FalloffFactor = 1.0f;
            if (BrushFalloff > 0.0f && BrushRadius > 0.0f)
            {
                float NormalizedDist = Distance / BrushRadius;
                // Falloff 0 = hard edge (no falloff), Falloff 1 = linear falloff
                FalloffFactor = 1.0f - (NormalizedDist * BrushFalloff);
                FalloffFactor = FMath::Clamp(FalloffFactor, 0.0f, 1.0f);
            }

            // 최종 적용 강도
            float ApplyStrength = BrushStrength * FalloffFactor;

            // 현재 weight와 목표 weight 블렌딩
            float CurrentWeight = 1.0f;
            auto It = ClothVertexWeights->find(VertIdx);
            if (It != ClothVertexWeights->end())
            {
                CurrentWeight = It->second;
            }
            float NewWeight = FMath::Lerp(CurrentWeight, PaintValue, ApplyStrength);
            NewWeight = FMath::Clamp(NewWeight, 0.0f, 1.0f);

            (*ClothVertexWeights)[VertIdx] = NewWeight;
        }
    }

    // ClothComponent에도 weight 동기화 (시뮬레이션에 사용)
    UClothComponent* ClothComp = SkelComp->GetInternalClothComponent();
    if (ClothComp && ClothVertexWeights)
    {
        for (const auto& Pair : *ClothVertexWeights)
        {
            // ClothComponent의 인덱스 매핑이 필요할 수 있음
            // 현재는 글로벌 버텍스 인덱스를 그대로 사용
            uint32 VertIdx = Pair.first;
            float Weight = Pair.second;
            if (VertIdx < static_cast<uint32>(ClothComp->GetClothVertexCount()))
            {
                ClothComp->SetVertexWeight(static_cast<int32>(VertIdx), Weight);
            }
        }
    }
}
