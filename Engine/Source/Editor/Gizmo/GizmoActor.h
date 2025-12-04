#pragma once
#include "Actor.h"
#include "Enums.h"
#include "VertexData.h"

class UGizmoArrowComponent;
class UGizmoScaleComponent;
class UGizmoRotateComponent;
class ACameraActor;
class USelectionManager;
class UInputManager;
class UUIManager;
class URenderer;
class CPickingSystem;
class FViewport;
class USkeletalMeshComponent;

class UPhysicsConstraintSetup;
class UBodySetup;
struct FKShapeElem;
namespace EAggCollisionShape { enum Type : int; }

/**
 * @brief Gizmo 렌더링용 배칭 구조체
 * - 여러 메쉬를 한 번에 수집하고 일괄 렌더링
 * - 버퍼 생성/해제 보일러플레이트 제거
 */
struct FGizmoBatchRenderer
{
    struct FBatchedMesh
    {
        TArray<FNormalVertex> Vertices;
        TArray<uint32> Indices;
        FVector4 Color;
        FVector Location;
        FQuat Rotation;
        FVector Scale;
        bool bAlwaysVisible;

        FBatchedMesh()
            : Color(1, 1, 1, 1)
            , Location(0, 0, 0)
            , Rotation(FQuat::Identity())
            , Scale(1, 1, 1)
            , bAlwaysVisible(true)
        {}
    };

    TArray<FBatchedMesh> Meshes;

    void AddMesh(const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices,
                 const FVector4& InColor, const FVector& InLocation,
                 const FQuat& InRotation = FQuat::Identity(),
                 const FVector& InScale = FVector(1, 1, 1));

    void FlushAndRender(class URenderer* Renderer);

    void Clear();
};

// Gizmo 타겟 타입
enum class EGizmoTargetType : uint8
{
    Actor,        // 일반 Actor/Component 타겟
    Bone,         // SkeletalMeshComponent의 특정 본 타겟
    Constraint,   // Physics Constraint 타겟
    Shape         // Physics Shape 타겟 (Body 내의 개별 Shape)
};

class AGizmoActor : public AActor
{
public:
    DECLARE_CLASS(AGizmoActor, AActor)
    AGizmoActor();

    virtual void Tick(float DeltaSeconds) override;
protected:
    ~AGizmoActor() override;

public:

// ────────────────
// Getter Functions
// ────────────────
    UGizmoArrowComponent* GetArrowX() const { return ArrowX; }
    UGizmoArrowComponent* GetArrowY() const { return ArrowY; }
    UGizmoArrowComponent* GetArrowZ() const { return ArrowZ; }
    UGizmoScaleComponent* GetScaleX() const { return ScaleX; }
    UGizmoScaleComponent* GetScaleY() const { return ScaleY; }
    UGizmoScaleComponent* GetScaleZ() const { return ScaleZ; }
    UGizmoRotateComponent* GetRotateX() const { return RotateX; }
    UGizmoRotateComponent* GetRotateY() const { return RotateY; }
    UGizmoRotateComponent* GetRotateZ() const { return RotateZ; }
    void SetMode(EGizmoMode NewMode);
    EGizmoMode GetMode();
    void SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* Target);
    void SetSpace(EGizmoSpace NewSpace) { CurrentSpace = NewSpace; }
    EGizmoSpace GetSpace() const { return CurrentSpace; }

    bool GetbRender() const { return bRender; }
    void SetbRender(bool bInRender) { bRender = bInRender; }

    bool GetbIsHovering() const { return bIsHovering; }
    void SetbIsHovering(bool bInIsHovering) { bIsHovering = bInIsHovering; }

    bool GetbIsDragging() const { return bIsDragging; }

    // Interaction enabled control (for tool windows like Particle Editor)
    void SetInteractionEnabled(bool bEnabled) { bInteractionEnabled = bEnabled; }
    bool IsInteractionEnabled() const { return bInteractionEnabled; }

    void NextMode(EGizmoMode GizmoMode);
    TArray<USceneComponent*>* GetGizmoComponents();


    EGizmoMode GetGizmoMode() const;

    void OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport);
    void OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera);

    // Gizmo interaction methods
   // void SetTargetActor(AActor* InTargetActor) { TargetActor = InTargetActor; Tick(0.f);  }
    void SetEditorCameraActor(ACameraActor* InCameraActor) { CameraActor = InCameraActor; }
    ACameraActor* GetEditorCameraActor() const { return CameraActor; }

    // Bone target functions
    void SetBoneTarget(USkeletalMeshComponent* InComponent, int32 InBoneIndex);
    void ClearBoneTarget();
    bool IsBoneTarget() const { return TargetType == EGizmoTargetType::Bone; }
    EGizmoTargetType GetTargetType() const { return TargetType; }

    // Constraint target functions
    void SetConstraintTarget(USkeletalMeshComponent* InComponent, UPhysicsConstraintSetup* InConstraint, int32 InBone1Index, int32 InBone2Index);
    void ClearConstraintTarget();
    bool IsConstraintTarget() const { return TargetType == EGizmoTargetType::Constraint; }
    UPhysicsConstraintSetup* GetTargetConstraint() const { return TargetConstraintSetup; }

    // Shape target functions
    void SetShapeTarget(USkeletalMeshComponent* InComponent, UBodySetup* InBodySetup, int32 InBoneIndex, EAggCollisionShape::Type InShapeType, int32 InShapeIndex);
    void ClearShapeTarget();
    bool IsShapeTarget() const { return TargetType == EGizmoTargetType::Shape; }
    UBodySetup* GetTargetBodySetup() const { return TargetBodySetup; }
    EAggCollisionShape::Type GetTargetShapeType() const { return TargetShapeType; }
    int32 GetTargetShapeIndex() const { return TargetShapeIndex; }

    void ProcessGizmoInteraction(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
    void ProcessGizmoModeSwitch();

    // 명시적 드래그 시작
    bool StartDrag(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
    void EndDrag();

    // 확장 기즈모 렌더링 (평면, 구체, Rotation 시각화)
    void RenderGizmoExtensions(URenderer* Renderer, ACameraActor* Camera);

    // 어차피 gizmo가 게임모드에서 안나오니까 할 필요 없을지도?
    // ───── 복사 관련 ────────────────────────────
    /*void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(AGizmoActor)*/

protected:

    UGizmoArrowComponent* ArrowX;
    UGizmoArrowComponent* ArrowY;
    UGizmoArrowComponent* ArrowZ;
    TArray<USceneComponent*> GizmoArrowComponents;

    UGizmoScaleComponent* ScaleX;
    UGizmoScaleComponent* ScaleY;
    UGizmoScaleComponent* ScaleZ;
    TArray<USceneComponent*> GizmoScaleComponents;

    UGizmoRotateComponent* RotateX;
    UGizmoRotateComponent* RotateY;
    UGizmoRotateComponent* RotateZ;
    TArray<USceneComponent*> GizmoRotateComponents;
    bool bRender = false;
    bool bIsHovering = false;
    bool bIsDragging = false;
    bool bDuplicatedThisDrag = false;  // Alt + 드래그 복사가 이번 드래그에서 이미 수행됐는지
    bool bInteractionEnabled = true;  // Set to false when tool windows (e.g., Particle Editor) are focused
    EGizmoMode CurrentMode = EGizmoMode::Translate;
    EGizmoSpace CurrentSpace = EGizmoSpace::World;

    // Interaction state
    /*AActor* TargetActor = nullptr;
    USceneComponent* SelectedComponent = nullptr;*/
    ACameraActor* CameraActor = nullptr;

    // Bone target state
    EGizmoTargetType TargetType = EGizmoTargetType::Actor;
    USkeletalMeshComponent* TargetSkeletalMeshComponent = nullptr;
    int32 TargetBoneIndex = -1;

    // Constraint target state
    UPhysicsConstraintSetup* TargetConstraintSetup = nullptr;
    int32 TargetConstraintBone1Index = -1;
    int32 TargetConstraintBone2Index = -1;

    // Shape target state
    UBodySetup* TargetBodySetup = nullptr;
    EAggCollisionShape::Type TargetShapeType = static_cast<EAggCollisionShape::Type>(0);
    int32 TargetShapeIndex = -1;

    // Manager references
    USelectionManager* SelectionManager = nullptr;
    UInputManager* InputManager = nullptr;
    UUIManager* UIManager = nullptr;

    uint32 GizmoAxis{};
    // Gizmo interaction methods

    void ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
    void ProcessGizmoDragging(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY);
    void UpdateComponentVisibility();

private:
    // 평면 기즈모 렌더링 함수
    void RenderTranslatePlanes(const FVector& GizmoLocation, const FQuat& BaseRot, float RenderScale, class URenderer* Renderer);
    void RenderScalePlanes(const FVector& GizmoLocation, const FQuat& BaseRot, float RenderScale, class URenderer* Renderer);
    void RenderCenterSphere(const FVector& GizmoLocation, float RenderScale, class URenderer* Renderer);

    // Rotation 기즈모 렌더링 함수
    void RenderRotationCircles(const FVector& GizmoLocation, const FQuat& BaseRot, const FVector4& AxisColor,
                                const FVector& BaseAxis0, const FVector& BaseAxis1, float RenderScale, class URenderer* Renderer);
    void RenderRotationQuarterRing(const FVector& GizmoLocation, const FQuat& BaseRot, uint32 Direction,
                                    const FVector& BaseAxis0, const FVector& BaseAxis1, float RenderScale, class URenderer* Renderer, ACameraActor* Camera);

    // 하이라이트 헬퍼 함수
    bool ShouldHighlightAxis(uint32 HighlightValue, uint32 AxisDirection) const;

    // 드래그 시작 시점의 타겟 트랜스폼
    FQuat DragStartRotation;
    FVector DragStartLocation;
    FVector DragStartScale;

    // Shape 드래그 시작 시점의 로컬 데이터
    FVector DragStartShapeCenter;
    FVector DragStartShapeRotation;  // Euler angles
    float DragStartShapeRadius = 0.0f;
    float DragStartShapeX = 0.0f;
    float DragStartShapeY = 0.0f;
    float DragStartShapeZ = 0.0f;
    float DragStartShapeLength = 0.0f;

    // Constraint 드래그 시작 시점의 데이터
    FQuat DragStartConstraintRotation;

    // 드래그 시작 시점의 마우스 및 카메라 정보
    FVector2D DragStartPosition;
    ACameraActor* DragCamera = nullptr;

    // 드래그가 시작될 때 고정된 축 (0 = 드래그 중 아님)
    uint32 DraggingAxis = 0;

    // Rotation 드래그 시 현재 회전 각도 (라디안)
    float CurrentRotationAngle = 0.0f;

    // 호버 시 3D 충돌 지점 (PickingSystem이 채워줌)
    FVector HoverImpactPoint;
    // 드래그 시작 시 3D 충돌 지점
    FVector DragImpactPoint;
    // (회전용) 계산된 2D 스크린 드래그 벡터
    FVector2D DragScreenVector;
};
