#pragma once

#include <set>
#include "Source/Runtime/Engine/PhysicsEngine/ShapeElem.h"

class UWorld;
class FViewport;
class FViewportClient;
class ASkeletalMeshActor;
class AStaticMeshActor;
class USkeletalMesh;
class UPhysicsAsset;
class UBodySetup;
class AGizmoActor;

// Shape 타입 (추가할 때 선택)
enum class EPhysicsShapeType : uint8
{
    Sphere,
    Box,
    Capsule
};

// 편집 모드
enum class EPhysicsAssetEditMode : uint8
{
    Body,       // Body 선택/편집 모드
    Constraint, // Constraint 선택/편집 모드
    Shape       // 개별 Shape 선택/편집 모드
};

class PhysicsAssetViewerState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // Preview
    ASkeletalMeshActor* PreviewActor = nullptr;
    AStaticMeshActor* FloorActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    UPhysicsAsset* PhysicsAsset = nullptr;
    FString LoadedMeshPath;
    FString LoadedPhysicsAssetPath;

    // Selection
    int32 SelectedBodyIndex = -1;              // 현재 선택된 Body (Details 패널 표시용)
    int32 GraphFilterRootBodyIndex = -1;       // 그래프 필터링 기준 Body (Skeleton Tree 클릭 시만 변경)
    int32 SelectedConstraintIndex = -1;
    int32 SelectedShapeIndex = -1;          // Body 내의 Shape 인덱스
    EAggCollisionShape::Type SelectedShapeType = EAggCollisionShape::Unknown;
    FName SelectedBoneName;

    // Edit Mode
    EPhysicsAssetEditMode EditMode = EPhysicsAssetEditMode::Body;

    // Debug Draw Options
    bool bShowBodies = true;
    bool bShowConstraints = true;
    bool bShowBoneNames = false;
    bool bShowMesh = true;
    bool bShowSkeleton = false;
    bool bShowBodyNames = false;
    bool bWireframe = false;

    // Simulation
    bool bSimulating = false;
    bool bSimulationPaused = false;

    // UI State
    std::set<int32> ExpandedBoneIndices;    // Skeleton Tree에서 펼쳐진 본들
    int32 PropertiesTabIndex = 0;           // Properties 패널 탭 인덱스

    // Shape 추가 다이얼로그 상태
    bool bShowAddShapeDialog = false;
    EPhysicsShapeType AddShapeType = EPhysicsShapeType::Capsule;

    // Copy/Paste
    UBodySetup* CopiedBodySetup = nullptr;

    // Gizmo
    AGizmoActor* GizmoActor = nullptr;
    bool bIsGizmoDragging = false;

    // 헬퍼 메서드
    UBodySetup* GetSelectedBodySetup() const;
    bool HasValidSelection() const { return SelectedBodyIndex >= 0 || SelectedConstraintIndex >= 0; }
    void ClearSelection();
    void SelectBody(int32 BodyIndex);
    void SelectConstraint(int32 ConstraintIndex);
    void SelectShape(int32 BodyIndex, EAggCollisionShape::Type ShapeType, int32 ShapeIndex);

    // 디버그 드로잉
    void DrawPhysicsBodies(class URenderer* Renderer) const;

    // 반투명 솔리드 Body 드로잉 (언리얼 스타일)
    void DrawPhysicsBodiesSolid(class URenderer* Renderer) const;

    // Constraint 드로잉 (언리얼 스타일)
    void DrawConstraints(class URenderer* Renderer) const;

    // Body/Shape 피킹 (뷰포트에서 마우스 클릭 시)
    // 반환값: true이면 피킹 성공, OutBodyIndex/OutShapeType/OutShapeIndex에 결과 저장
    bool PickBodyOrShape(const struct FRay& Ray,
                         int32& OutBodyIndex,
                         EAggCollisionShape::Type& OutShapeType,
                         int32& OutShapeIndex,
                         float& OutDistance) const;

private:
    // 개별 Shape 드로잉 헬퍼
    void DrawSphere(class URenderer* Renderer, const FTransform& BoneTransform, const struct FKSphereElem& Sphere, const FVector4& Color) const;
    void DrawBox(class URenderer* Renderer, const FTransform& BoneTransform, const struct FKBoxElem& Box, const FVector4& Color) const;
    void DrawCapsule(class URenderer* Renderer, const FTransform& BoneTransform, const struct FKCapsuleElem& Capsule, const FVector4& Color) const;
};
