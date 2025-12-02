#include "pch.h"
#include "PhysicsAssetViewerState.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Core/Misc/PrimitiveGeometry.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
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
        }
    }

    EditMode = EPhysicsAssetEditMode::Body;
}

void PhysicsAssetViewerState::SelectConstraint(int32 ConstraintIndex)
{
    ClearSelection();
    SelectedConstraintIndex = ConstraintIndex;
    EditMode = EPhysicsAssetEditMode::Constraint;
}

void PhysicsAssetViewerState::SelectShape(int32 BodyIndex, EAggCollisionShape::Type ShapeType, int32 ShapeIndex)
{
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
        }
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
        WorldCorners[i] = BoxWorldTransform.TransformPosition(LocalCorners[i]);
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
            FTransform ShapeLocalTransform(Sphere.Center, FQuat(0, 0, 0, 1), FVector(1, 1, 1));
            FTransform ShapeWorldTransform = BoneTransform.GetWorldTransform(ShapeLocalTransform);
            FMatrix WorldMatrix = FMatrix::FromTRS(ShapeWorldTransform.Translation, ShapeWorldTransform.Rotation, FVector(1, 1, 1));

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
            FQuat BoxRot = FQuat::MakeFromEulerZYX(Box.Rotation * (PI / 180.0f));
            FTransform ShapeLocalTransform(Box.Center, BoxRot, FVector(1, 1, 1));
            FTransform ShapeWorldTransform = BoneTransform.GetWorldTransform(ShapeLocalTransform);
            FMatrix WorldMatrix = FMatrix::FromTRS(ShapeWorldTransform.Translation, ShapeWorldTransform.Rotation, FVector(1, 1, 1));

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
            FQuat CapsuleRot = FQuat::MakeFromEulerZYX(Capsule.Rotation * (PI / 180.0f));
            FTransform ShapeLocalTransform(Capsule.Center, CapsuleRot, FVector(1, 1, 1));
            FTransform ShapeWorldTransform = BoneTransform.GetWorldTransform(ShapeLocalTransform);
            FMatrix WorldMatrix = FMatrix::FromTRS(ShapeWorldTransform.Translation, ShapeWorldTransform.Rotation, FVector(1, 1, 1));

            Renderer->AddPrimitiveData(CapsuleMesh, WorldMatrix);
        }
    }

    // 프리미티브 배치 종료 및 렌더링
    Renderer->EndPrimitiveBatch();
}

void PhysicsAssetViewerState::DrawConstraints(URenderer* Renderer) const
{
    if (!bShowConstraints || !PhysicsAsset || !Renderer)
        return;

    // SkeletalMeshComponent에서 본 위치 가져오기
    USkeletalMeshComponent* SkelComp = nullptr;
    if (PreviewActor)
    {
        SkelComp = PreviewActor->GetSkeletalMeshComponent();
    }

    const FSkeleton* Skeleton = CurrentMesh ? CurrentMesh->GetSkeleton() : nullptr;
    if (!Skeleton || !SkelComp)
        return;

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    const FVector4 NormalConstraintColor(0.8f, 0.8f, 0.2f, 1.0f);  // 노란색
    const FVector4 SelectedConstraintColor(1.0f, 0.3f, 0.3f, 1.0f); // 빨간색

    // 모든 Constraint 순회
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

        // Constraint 선택 여부 확인
        bool bSelected = (EditMode == EPhysicsAssetEditMode::Constraint && SelectedConstraintIndex == ConstraintIdx);
        FVector4 ConstraintColor = bSelected ? SelectedConstraintColor : NormalConstraintColor;
        float LineThickness = bSelected ? 3.0f : 1.5f;

        // 두 본을 연결하는 라인
        StartPoints.Add(Pos1);
        EndPoints.Add(Pos2);
        Colors.Add(ConstraintColor);

        // 선택된 Constraint에만 좌표축 그리기
        if (bSelected)
        {
            // Constraint 위치 (중간점)
            FVector ConstraintPos = (Pos1 + Pos2) * 0.5f;

            // 좌표축 그리기 (X=빨강, Y=초록, Z=파랑)
            // 두 본 사이 거리의 30%로 축 길이 설정 (너무 길지 않게)
            float BoneDistance = (Pos2 - Pos1).Size();
            float AxisLength = FMath::Min(BoneDistance * 0.3f, 10.0f);  // 최대 10cm

            // X축 (빨강)
            StartPoints.Add(ConstraintPos);
            EndPoints.Add(ConstraintPos + FVector(AxisLength, 0, 0));
            Colors.Add(FVector4(1, 0, 0, 1));

            // Y축 (초록)
            StartPoints.Add(ConstraintPos);
            EndPoints.Add(ConstraintPos + FVector(0, AxisLength, 0));
            Colors.Add(FVector4(0, 1, 0, 1));

            // Z축 (파랑)
            StartPoints.Add(ConstraintPos);
            EndPoints.Add(ConstraintPos + FVector(0, 0, AxisLength));
            Colors.Add(FVector4(0, 0, 1, 1));
        }
    }

    // 라인 렌더링
    if (!StartPoints.empty())
    {
        Renderer->AddLines(StartPoints, EndPoints, Colors);
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
    float A = LocalDir.X * LocalDir.X + LocalDir.Y * LocalDir.Y;
    float B = 2.0f * (LocalOrigin.X * LocalDir.X + LocalOrigin.Y * LocalDir.Y);
    float C = LocalOrigin.X * LocalOrigin.X + LocalOrigin.Y * LocalOrigin.Y - Radius * Radius;

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
                float Z = LocalOrigin.Z + T * LocalDir.Z;
                if (Z >= -HalfLength && Z <= HalfLength)
                {
                    if (T < ClosestT) { ClosestT = T; bHit = true; }
                }
            }
        }
    }

    // 반구 캡 검사
    float SphereT;
    FVector TopCenter(0, 0, HalfLength);
    FVector BotCenter(0, 0, -HalfLength);

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
            if (HitPoint.Z >= HalfLength && SphereT < ClosestT) { ClosestT = SphereT; bHit = true; }
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
            if (HitPoint.Z <= -HalfLength && SphereT < ClosestT) { ClosestT = SphereT; bHit = true; }
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
            FTransform ShapeLocal(Sphere.Center, FQuat(0, 0, 0, 1), FVector(1, 1, 1));
            FTransform ShapeWorld = BoneTransform.GetWorldTransform(ShapeLocal);

            float T;
            if (RaySphereIntersect(Ray, ShapeWorld.Translation, Sphere.Radius, T))
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
            FQuat BoxRot = FQuat::MakeFromEulerZYX(Box.Rotation * (PI / 180.0f));
            FTransform ShapeLocal(Box.Center, BoxRot, FVector(1, 1, 1));
            FTransform ShapeWorld = BoneTransform.GetWorldTransform(ShapeLocal);

            float T;
            FVector HalfExtent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);
            if (RayBoxIntersect(Ray, ShapeWorld.Translation, HalfExtent, ShapeWorld.Rotation, T))
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
            FQuat CapsuleRot = FQuat::MakeFromEulerZYX(Capsule.Rotation * (PI / 180.0f));
            FTransform ShapeLocal(Capsule.Center, CapsuleRot, FVector(1, 1, 1));
            FTransform ShapeWorld = BoneTransform.GetWorldTransform(ShapeLocal);

            float T;
            if (RayCapsuleIntersect(Ray, ShapeWorld.Translation, Capsule.Radius, Capsule.Length * 0.5f, ShapeWorld.Rotation, T))
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
