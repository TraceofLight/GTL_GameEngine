#include "pch.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
#include "Gizmo/GizmoGeometry.h"
#include "RenderSettings.h"
#include "CameraActor.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "UIManager.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Picking.h"
#include "EditorEngine.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMeshActor.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsConstraintSetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/ShapeElem.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(AGizmoActor)

// ────────────────────────────────────────────────────────
// FGizmoBatchRenderer 구현
// ────────────────────────────────────────────────────────

void FGizmoBatchRenderer::AddMesh(const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices,
                                   const FVector4& InColor, const FVector& InLocation,
                                   const FQuat& InRotation, const FVector& InScale)
{
	FBatchedMesh Mesh;
	Mesh.Vertices = InVertices;
	Mesh.Indices = InIndices;
	Mesh.Color = InColor;
	Mesh.Location = InLocation;
	Mesh.Rotation = InRotation;
	Mesh.Scale = InScale;
	Mesh.bAlwaysVisible = true;

	// Vertex color를 Batch Color로 덮어쓰기 (호버링 색상 적용)
	for (FNormalVertex& Vtx : Mesh.Vertices)
	{
		Vtx.color = InColor;
	}

	Meshes.Add(Mesh);
}

void FGizmoBatchRenderer::FlushAndRender(URenderer* Renderer)
{
	if (!Renderer || Meshes.Num() == 0)
	{
		Clear();
		return;
	}

	// 배치 렌더링 시작
	Renderer->BeginPrimitiveBatch();

	for (const FBatchedMesh& Mesh : Meshes)
	{
		// FNormalVertex 배열을 FMeshData로 변환
		FMeshData MeshData;
		MeshData.Vertices.Reserve(Mesh.Vertices.Num());
		MeshData.Normal.Reserve(Mesh.Vertices.Num());
		MeshData.UV.Reserve(Mesh.Vertices.Num());
		MeshData.Color.Reserve(Mesh.Vertices.Num());

		for (const FNormalVertex& Vtx : Mesh.Vertices)
		{
			MeshData.Vertices.Add(Vtx.pos);
			MeshData.Normal.Add(Vtx.normal);
			MeshData.UV.Add(Vtx.tex);
			MeshData.Color.Add(Vtx.color);
		}

		MeshData.Indices = Mesh.Indices;

		// 월드 변환 행렬 생성
		FMatrix WorldMatrix = FMatrix::FromTRS(Mesh.Location, Mesh.Rotation, Mesh.Scale);

		// 프리미티브 추가
		Renderer->AddPrimitiveData(MeshData, WorldMatrix);
	}

	// 배치 렌더링 종료
	Renderer->EndPrimitiveBatch();

	Clear();
}

void FGizmoBatchRenderer::Clear()
{
	Meshes.Empty();
}

// ────────────────────────────────────────────────────────
// AGizmoActor 구현
// ────────────────────────────────────────────────────────

AGizmoActor::AGizmoActor()
{
	ObjectName = "Gizmo Actor";

	const float GizmoTotalSize = 1.5f;
	const float STGizmoTotalSize = 7.0f;    // Scale, Translation Gizmo
	const float RotationGizmoSize = 1.5f;   // Rotation Gizmo (크기는 동일, 두께만 증가)

	//======= Arrow Component 생성 =======
	RootComponent = CreateDefaultSubobject<USceneComponent>("DefaultSceneComponent");

	ArrowX = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowY = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowZ = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");

	ArrowX->SetDirection(FVector(1.0f, 0.0f, 0.0f));//빨
	ArrowY->SetDirection(FVector(0.0f, 1.0f, 0.0f));//초
	ArrowZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));//파

	ArrowX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ArrowY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ArrowZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ArrowX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ArrowY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ArrowZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	ArrowX->SetDefaultScale({ STGizmoTotalSize, STGizmoTotalSize, STGizmoTotalSize });
	ArrowY->SetDefaultScale({ STGizmoTotalSize, STGizmoTotalSize, STGizmoTotalSize });
	ArrowZ->SetDefaultScale({ STGizmoTotalSize, STGizmoTotalSize, STGizmoTotalSize });

	ArrowX->SetRenderPriority(100);
	ArrowY->SetRenderPriority(100);
	ArrowZ->SetRenderPriority(100);

	AddOwnedComponent(ArrowX);
	AddOwnedComponent(ArrowY);
	AddOwnedComponent(ArrowZ);
	GizmoArrowComponents.Add(ArrowX);
	GizmoArrowComponents.Add(ArrowY);
	GizmoArrowComponents.Add(ArrowZ);

	if (ArrowX) ArrowX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (ArrowY) ArrowY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (ArrowZ) ArrowZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	//======= Rotate Component 생성 =======
	RotateX = NewObject<UGizmoRotateComponent>();
	RotateY = NewObject<UGizmoRotateComponent>();
	RotateZ = NewObject<UGizmoRotateComponent>();

	RotateX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	RotateY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	RotateZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	RotateX->SetDefaultScale({ RotationGizmoSize, RotationGizmoSize, RotationGizmoSize });
	RotateY->SetDefaultScale({ RotationGizmoSize, RotationGizmoSize, RotationGizmoSize });
	RotateZ->SetDefaultScale({ RotationGizmoSize, RotationGizmoSize, RotationGizmoSize });

	RotateX->SetRenderPriority(100);
	RotateY->SetRenderPriority(100);
	RotateZ->SetRenderPriority(100);

	AddOwnedComponent(RotateX);
	AddOwnedComponent(RotateY);
	AddOwnedComponent(RotateZ);
	GizmoRotateComponents.Add(RotateX);
	GizmoRotateComponents.Add(RotateY);
	GizmoRotateComponents.Add(RotateZ);

	if (RotateX) RotateX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (RotateY) RotateY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (RotateZ) RotateZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	//======= Scale Component 생성 =======
	ScaleX = NewObject<UGizmoScaleComponent>();
	ScaleY = NewObject<UGizmoScaleComponent>();
	ScaleZ = NewObject<UGizmoScaleComponent>();

	ScaleX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ScaleY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ScaleZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	ScaleX->SetDefaultScale({ STGizmoTotalSize, STGizmoTotalSize, STGizmoTotalSize });
	ScaleY->SetDefaultScale({ STGizmoTotalSize, STGizmoTotalSize, STGizmoTotalSize });
	ScaleZ->SetDefaultScale({ STGizmoTotalSize, STGizmoTotalSize, STGizmoTotalSize });

	ScaleX->SetRenderPriority(100);
	ScaleY->SetRenderPriority(100);
	ScaleZ->SetRenderPriority(100);

	if (ScaleX) ScaleX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (ScaleY) ScaleY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (ScaleZ) ScaleZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	AddOwnedComponent(ScaleX);
	AddOwnedComponent(ScaleY);
	AddOwnedComponent(ScaleZ);
	GizmoScaleComponents.Add(ScaleX);
	GizmoScaleComponents.Add(ScaleY);
	GizmoScaleComponents.Add(ScaleZ);

	CurrentMode = EGizmoMode::Translate;

	// --- 드래그 상태 변수 초기화 ---
	DraggingAxis = 0;
	DragCamera = nullptr;
	HoverImpactPoint = FVector::Zero();
	DragImpactPoint = FVector::Zero();
	DragScreenVector = FVector2D::Zero();

	// 매니저 참조 초기화 (월드 소유)
	SelectionManager = GetWorld() ? GetWorld()->GetSelectionManager() : nullptr;
	InputManager = &UInputManager::GetInstance();
	UIManager = &UUIManager::GetInstance();
}

void AGizmoActor::Tick(float DeltaSeconds)
{
	if (!SelectionManager) SelectionManager = GetWorld() ? GetWorld()->GetSelectionManager() : nullptr;
	if (!InputManager) InputManager = &UInputManager::GetInstance();
	if (!UIManager) UIManager = &UUIManager::GetInstance();

	// Bone 타겟일 때는 본 위치를 추적
	if (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0)
	{
		FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);
		SetActorLocation(BoneWorldTransform.Translation);
		SetSpaceWorldMatrix(CurrentSpace, nullptr);
	}
	// Constraint 타겟일 때는 Child 본(Bone2) 위치를 추적
	else if (TargetType == EGizmoTargetType::Constraint && TargetSkeletalMeshComponent && TargetConstraintSetup &&
	         TargetConstraintBone1Index >= 0 && TargetConstraintBone2Index >= 0)
	{
		FTransform Bone2Transform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetConstraintBone2Index);
		SetActorLocation(Bone2Transform.Translation);
		SetSpaceWorldMatrix(CurrentSpace, nullptr);
	}
	// Shape 타겟일 때는 Shape의 월드 위치를 추적
	else if (TargetType == EGizmoTargetType::Shape && TargetSkeletalMeshComponent && TargetBodySetup && TargetBoneIndex >= 0 && TargetShapeIndex >= 0)
	{
		FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);
		FVector ShapeLocalCenter = FVector::Zero();

		switch (TargetShapeType)
		{
		case EAggCollisionShape::Sphere:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.SphereElems.Num())
				ShapeLocalCenter = TargetBodySetup->AggGeom.SphereElems[TargetShapeIndex].Center;
			break;
		case EAggCollisionShape::Box:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
				ShapeLocalCenter = TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex].Center;
			break;
		case EAggCollisionShape::Capsule:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
				ShapeLocalCenter = TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex].Center;
			break;
		default:
			break;
		}

		FVector ShapeWorldPos = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(ShapeLocalCenter);
		SetActorLocation(ShapeWorldPos);
		SetSpaceWorldMatrix(CurrentSpace, nullptr);
	}
	// Actor/Component 타겟일 때만 선택된 컴포넌트 위치 추적 (Bone/Shape/Constraint 타겟은 위에서 처리됨)
	else if (TargetType == EGizmoTargetType::Actor && SelectionManager && SelectionManager->HasSelection() && CameraActor)
	{
		USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();

		// 기즈모 위치를 선택된 액터 위치로 업데이트
		if (SelectedComponent)
		{
			// OnDrag 함수가 컴포넌트의 위치를 변경하면,
			// Tick 함수는 그 변경된 위치를 읽어 기즈모 액터 자신을 이동시킵니다.
			SetSpaceWorldMatrix(CurrentSpace, SelectedComponent);
			SetActorLocation(SelectedComponent->GetWorldLocation());
		}
	}
	UpdateComponentVisibility();
}

AGizmoActor::~AGizmoActor()
{
	// Components are centrally destroyed by AActor's destructor
	ArrowX = ArrowY = ArrowZ = nullptr;
	ScaleX = ScaleY = ScaleZ = nullptr;
	RotateX = RotateY = RotateZ = nullptr;
}

void AGizmoActor::SetMode(EGizmoMode NewMode)
{
	CurrentMode = NewMode;

	// 모드 변경 시 기즈모 회전 업데이트 (Scale 모드는 항상 로컬 회전 적용)
	if (SelectionManager)
	{
		USceneComponent* SelectedComp = SelectionManager->GetSelectedComponent();
		if (SelectedComp)
		{
			SetSpaceWorldMatrix(CurrentSpace, SelectedComp);
		}
	}
}

EGizmoMode AGizmoActor::GetMode()
{
	return CurrentMode;
}

void AGizmoActor::SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* SelectedComponent)
{
	// Scale 모드가 아닐 때만 CurrentSpace 업데이트
	// Scale 모드는 렌더링/로직에서만 Local 강제, CurrentSpace는 유지
	if (CurrentMode != EGizmoMode::Scale)
	{
		SetSpace(NewSpace);
	}

	// Scale 모드는 무조건 Local 회전 사용 (렌더링/로직용)
	EGizmoSpace EffectiveSpace = (CurrentMode == EGizmoMode::Scale) ? EGizmoSpace::Local : NewSpace;

	// Bone 타겟인 경우
	if (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0)
	{
		FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);

		if (EffectiveSpace == EGizmoSpace::Local)
		{
			SetActorRotation(BoneWorldTransform.Rotation);
		}
		else if (EffectiveSpace == EGizmoSpace::World)
		{
			SetActorRotation(FQuat::Identity());
		}
		return;
	}

	// Constraint 타겟인 경우
	if (TargetType == EGizmoTargetType::Constraint && TargetSkeletalMeshComponent && TargetConstraintSetup &&
	    TargetConstraintBone1Index >= 0 && TargetConstraintBone2Index >= 0)
	{
		// Constraint의 현재 월드 회전 = 부모 본 회전 * Constraint 로컬 회전
		FTransform Bone1Transform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetConstraintBone1Index);
		FQuat ConstraintLocalRot = FQuat::MakeFromEulerZYX(TargetConstraintSetup->ConstraintRotationInBody1);
		FQuat ConstraintWorldRot = Bone1Transform.Rotation * ConstraintLocalRot;

		if (EffectiveSpace == EGizmoSpace::Local)
		{
			SetActorRotation(ConstraintWorldRot);
		}
		else if (EffectiveSpace == EGizmoSpace::World)
		{
			SetActorRotation(FQuat::Identity());
		}
		return;
	}

	// Shape 타겟인 경우
	if (TargetType == EGizmoTargetType::Shape && TargetSkeletalMeshComponent && TargetBodySetup && TargetBoneIndex >= 0 && TargetShapeIndex >= 0)
	{
		FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);
		FVector ShapeLocalRotation = FVector::Zero();

		switch (TargetShapeType)
		{
		case EAggCollisionShape::Box:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
				ShapeLocalRotation = TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex].Rotation;
			break;
		case EAggCollisionShape::Capsule:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
				ShapeLocalRotation = TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex].Rotation;
			break;
		default:
			break;  // Sphere has no rotation
		}

		FQuat ShapeLocalQuat = FQuat::MakeFromEulerZYX(ShapeLocalRotation);
		FQuat ShapeWorldRot = BoneWorldTransform.Rotation * ShapeLocalQuat;

		if (EffectiveSpace == EGizmoSpace::Local)
		{
			SetActorRotation(ShapeWorldRot);
		}
		else if (EffectiveSpace == EGizmoSpace::World)
		{
			SetActorRotation(FQuat::Identity());
		}
		return;
	}

	// Actor/Component 타겟인 경우
	if (!SelectedComponent)
		return;

	if (EffectiveSpace == EGizmoSpace::Local)
	{
		// 기즈모 액터 자체를 타겟의 회전으로 설정합니다.
		FQuat TargetRot = SelectedComponent->GetWorldRotation();
		SetActorRotation(TargetRot);
	}
	else if (EffectiveSpace == EGizmoSpace::World)
	{
		// 기즈모 액터를 월드 축에 정렬 (단위 회전으로 설정)
		SetActorRotation(FQuat::Identity());
	}
}

void AGizmoActor::NextMode(EGizmoMode GizmoMode)
{
	CurrentMode = GizmoMode;
}

TArray<USceneComponent*>* AGizmoActor::GetGizmoComponents()
{
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
		return &GizmoArrowComponents;
	case EGizmoMode::Rotate:
		return &GizmoRotateComponents;
	case EGizmoMode::Scale:
		return &GizmoScaleComponents;
	}
	return nullptr;
}

EGizmoMode AGizmoActor::GetGizmoMode() const
{
	return CurrentMode;
}

// 개선된 축 투영 함수 - 수직 각도에서도 안정적
static FVector2D GetStableAxisDirection(const FVector& WorldAxis, const ACameraActor* Camera)
{
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();

	// 카메라와 축이 정렬될 때 불안정하게 튀는
	// "예외 처리" if 블록을 완전히 제거합니다.

	// "일반적인 경우"의 스크린 투영 로직만 사용합니다.
	float RightDot = FVector::Dot(WorldAxis, CameraRight);
	float UpDot = FVector::Dot(WorldAxis, CameraUp);

	// DirectX 스크린 좌표계 고려 (Y축 반전)
	FVector2D ScreenDirection = FVector2D(RightDot, -UpDot);

	// 안전한 정규화 (최소 길이 보장)
	float Length = std::sqrt(ScreenDirection.X * ScreenDirection.X + ScreenDirection.Y * ScreenDirection.Y);
	if (Length > 0.001f)
	{
		return ScreenDirection * (1.0f / Length);
	}

	// 투영된 길이가 0에 가까우면 (즉, 축이 카메라를 쳐다보면)
	// 이 코드가 실행되어 안정적인 기본 X축 방향을 반환합니다.
	return FVector2D(1.0f, 0.0f);
}

void AGizmoActor::OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport)
{
	// DraggingAxis == 0 이면 드래그 중이 아니므로 반환
	if (!Camera || DraggingAxis == 0)
	{
		return;
	}

	// Bone 타겟인 경우: TargetSkeletalMeshComponent를 사용
	bool bIsBoneMode = (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0);
	// Shape 타겟인 경우: Shape의 로컬 좌표를 수정
	bool bIsShapeMode = (TargetType == EGizmoTargetType::Shape && TargetSkeletalMeshComponent && TargetBodySetup && TargetBoneIndex >= 0 && TargetShapeIndex >= 0);
	// Constraint 타겟인 경우: Constraint의 회전 프레임을 수정
	bool bIsConstraintMode = (TargetType == EGizmoTargetType::Constraint && TargetSkeletalMeshComponent && TargetConstraintSetup && TargetConstraintBone1Index >= 0 && TargetConstraintBone2Index >= 0);

	// Actor/Component 타겟인 경우: Target 검증
	if (!bIsBoneMode && !bIsShapeMode && !bIsConstraintMode && !Target)
	{
		return;
	}

	// 디버그: Shape 모드 상태 확인
	{
		char debugMsg[512];
		sprintf_s(debugMsg, "[OnDrag] TargetType=%d, bIsShapeMode=%d, TargetBodySetup=%p, TargetBoneIndex=%d, TargetShapeIndex=%d, DraggingAxis=%u, CurrentMode=%d\n",
			(int)TargetType, bIsShapeMode ? 1 : 0, (void*)TargetBodySetup, TargetBoneIndex, TargetShapeIndex, DraggingAxis, (int)CurrentMode);
		OutputDebugStringA(debugMsg);
	}

	// MouseDeltaX/Y는 이제 드래그 시작점으로부터의 '총 변위(Total Offset)'입니다.
	FVector2D MouseOffset(MouseDeltaX, MouseDeltaY);

	// ────────────── 모드별 처리 (Stateful 방식) ──────────────
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
	case EGizmoMode::Scale:
	{
		// === 평면 드래그 또는 중심 구체 드래그 처리 ===
		if (DraggingAxis == 8 || DraggingAxis == 16 || DraggingAxis == 32 || DraggingAxis == 64)
		{
			// 레이 생성
			FVector2D ViewportSize(Viewport ? static_cast<float>(Viewport->GetSizeX()) : UInputManager::GetInstance().GetScreenSize().X,
			                       Viewport ? static_cast<float>(Viewport->GetSizeY()) : UInputManager::GetInstance().GetScreenSize().Y);
			FVector2D ViewportOffset(Viewport ? static_cast<float>(Viewport->GetStartX()) : 0.0f,
			                          Viewport ? static_cast<float>(Viewport->GetStartY()) : 0.0f);

			// 현재 마우스 위치 = 드래그 시작 위치 + 오프셋
			FVector2D CurrentMousePos = DragStartPosition + MouseOffset;

			FMatrix View = Camera->GetViewMatrix();
			float aspect = ViewportSize.X / ViewportSize.Y;
			FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);

			FVector CameraPos = Camera->GetActorLocation();
			FVector CameraRight = Camera->GetRight();
			FVector CameraUp = Camera->GetUp();
			FVector CameraForward = Camera->GetForward();

			FRay Ray = MakeRayFromViewport(View, Proj, CameraPos, CameraRight, CameraUp, CameraForward,
			                                CurrentMousePos + ViewportOffset, ViewportSize, ViewportOffset);

			// 평면 정의
			FVector PlaneNormal;
			FVector PlanePoint = DragStartLocation;

			if (DraggingAxis == 64) // 중심 구체: 카메라 평면
			{
				PlaneNormal = CameraForward;
			}
			else // 평면 드래그
			{
				FQuat BaseRot = (CurrentSpace == EGizmoSpace::Local) ? DragStartRotation : FQuat::Identity();

				if (DraggingAxis == 8)      // XY 평면
				{
					PlaneNormal = BaseRot.RotateVector(FVector(0, 0, 1)); // Z축
				}
				else if (DraggingAxis == 16) // XZ 평면
				{
					PlaneNormal = BaseRot.RotateVector(FVector(0, 1, 0)); // Y축
				}
				else if (DraggingAxis == 32) // YZ 평면
				{
					PlaneNormal = BaseRot.RotateVector(FVector(1, 0, 0)); // X축
				}
			}

			// 레이-평면 교차 검사
			float T;
			if (IntersectRayPlane(Ray, PlanePoint, PlaneNormal, T))
			{
				FVector HitPoint = Ray.Origin + Ray.Direction * T;

				if (CurrentMode == EGizmoMode::Translate)
				{
					// Translation: 드래그 시작 지점 대비 변위 계산 (축 드래그와 동일한 패턴)
					FVector Delta = HitPoint - DragImpactPoint;
					FVector NewLocation = DragStartLocation + Delta;

					if (bIsBoneMode)
					{
						FTransform NewWorldTransform;
						NewWorldTransform.Translation = NewLocation;
						NewWorldTransform.Rotation = DragStartRotation;
						NewWorldTransform.Scale3D = DragStartScale;
						TargetSkeletalMeshComponent->SetBoneWorldTransform(TargetBoneIndex, NewWorldTransform);
					}
					else if (bIsShapeMode)
					{
						// Shape의 새로운 월드 위치를 로컬 Center로 변환
						FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);
						FVector NewLocalCenter = BoneWorldTransform.Rotation.Inverse().RotateVector(NewLocation - BoneWorldTransform.Translation);

						// Shape의 Center 업데이트
						switch (TargetShapeType)
						{
						case EAggCollisionShape::Sphere:
							if (TargetShapeIndex < TargetBodySetup->AggGeom.SphereElems.Num())
								TargetBodySetup->AggGeom.SphereElems[TargetShapeIndex].Center = NewLocalCenter;
							break;
						case EAggCollisionShape::Box:
							if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
								TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex].Center = NewLocalCenter;
							break;
						case EAggCollisionShape::Capsule:
							if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
								TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex].Center = NewLocalCenter;
							break;
						default:
							break;
						}
					}
					else if (Target)
					{
						Target->SetWorldLocation(NewLocation);
					}
				}
				else if (CurrentMode == EGizmoMode::Scale)
				{
					// Scale: 드래그 시작 지점 대비 변위 계산 (평면에서의 실제 이동량)
					FVector Delta = HitPoint - DragImpactPoint;
					FQuat BaseRot = (CurrentSpace == EGizmoSpace::Local) ? DragStartRotation : FQuat::Identity();

					FVector NewScale = DragStartScale;

					if (DraggingAxis == 64) // 중심 구체: 균등 스케일
					{
						// 카메라 평면에서의 이동 거리를 스케일로 변환
						float DeltaLength = Delta.Size();
						FVector ToCamera = CameraPos - DragStartLocation;
						float Sign = (FVector::Dot(Delta, ToCamera) > 0.0f) ? 1.0f : -1.0f;
						float ScaleFactor = 1.0f + Sign * DeltaLength * 0.01f; // 스케일 속도 조절
						NewScale = DragStartScale * ScaleFactor;
					}
					else // 평면 드래그: 두 축 스케일
					{
						FVector Axis0, Axis1;

						if (DraggingAxis == 8)      // XY 평면
						{
							Axis0 = BaseRot.RotateVector(FVector(1, 0, 0));
							Axis1 = BaseRot.RotateVector(FVector(0, 1, 0));
						}
						else if (DraggingAxis == 16) // XZ 평면
						{
							Axis0 = BaseRot.RotateVector(FVector(1, 0, 0));
							Axis1 = BaseRot.RotateVector(FVector(0, 0, 1));
						}
						else if (DraggingAxis == 32) // YZ 평면
						{
							Axis0 = BaseRot.RotateVector(FVector(0, 1, 0));
							Axis1 = BaseRot.RotateVector(FVector(0, 0, 1));
						}

						// 각 축에 대한 델타 계산
						float Delta0 = FVector::Dot(Delta, Axis0);
						float Delta1 = FVector::Dot(Delta, Axis1);

						// 스케일 증가량 계산 (0.01 = 속도 조절)
						float ScaleFactor0 = 1.0f + Delta0 * 0.01f;
						float ScaleFactor1 = 1.0f + Delta1 * 0.01f;

						// Local 공간에서 스케일 적용
						FVector LocalScale = NewScale;
						if (DraggingAxis == 8)      // XY
						{
							LocalScale.X *= ScaleFactor0;
							LocalScale.Y *= ScaleFactor1;
						}
						else if (DraggingAxis == 16) // XZ
						{
							LocalScale.X *= ScaleFactor0;
							LocalScale.Z *= ScaleFactor1;
						}
						else if (DraggingAxis == 32) // YZ
						{
							LocalScale.Y *= ScaleFactor0;
							LocalScale.Z *= ScaleFactor1;
						}

						NewScale = LocalScale;
					}

					// 스케일 적용
					if (bIsBoneMode)
					{
						FTransform NewWorldTransform;
						NewWorldTransform.Translation = DragStartLocation;
						NewWorldTransform.Rotation = DragStartRotation;
						NewWorldTransform.Scale3D = NewScale;
						TargetSkeletalMeshComponent->SetBoneWorldTransform(TargetBoneIndex, NewWorldTransform);
					}
					else if (Target)
					{
						Target->SetRelativeScale(NewScale);
					}
				}
			}

			return; // 평면/구체 드래그는 여기서 종료
		}

		// --- 드래그 시작 시점의 축 계산 (기존 축 드래그) ---
		FVector Axis{};
		if (CurrentSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
		{
			switch (DraggingAxis)
			{
			case 1: Axis = DragStartRotation.RotateVector(FVector(1, 0, 0)); break;
			case 2: Axis = DragStartRotation.RotateVector(FVector(0, 1, 0)); break;
			case 4: Axis = DragStartRotation.RotateVector(FVector(0, 0, 1)); break;  // Z축
			}
		}
		else if (CurrentSpace == EGizmoSpace::World)
		{
			switch (DraggingAxis)
			{
			case 1: Axis = FVector(1, 0, 0); break;
			case 2: Axis = FVector(0, 1, 0); break;
			case 4: Axis = FVector(0, 0, 1); break;  // Z축
			}
		}

		// ────────────── 픽셀 당 월드 이동량 계산 (Translate/Scale용) ──────────────
		FVector2D ScreenAxis = GetStableAxisDirection(Axis, Camera);
		float h = Viewport ? static_cast<float>(Viewport->GetSizeY()) : UInputManager::GetInstance().GetScreenSize().Y;
		if (h <= 0.0f) h = 1.0f;
		float w = Viewport ? static_cast<float>(Viewport->GetSizeX()) : UInputManager::GetInstance().GetScreenSize().X;
		float aspect = w / h;
		FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);
		bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
		float worldPerPixel = 0.0f;
		if (bOrtho)
		{
			float halfH = 1.0f / Proj.M[1][1];
			worldPerPixel = (2.0f * halfH) / h;
		}
		else
		{
			float yScale = Proj.M[1][1];
			FVector camPos = Camera->GetActorLocation();
			FVector gizPos = GetActorLocation();
			FVector camF = Camera->GetForward();
			float z = FVector::Dot(gizPos - camPos, camF);
			if (z < 1.0f) z = 1.0f;
			worldPerPixel = (2.0f * z) / (h * yScale);
		}

		float ProjectedPx = (MouseOffset.X * ScreenAxis.X + MouseOffset.Y * ScreenAxis.Y);
		float TotalMovement = ProjectedPx * worldPerPixel;

		if (CurrentMode == EGizmoMode::Translate)
		{
			FVector NewLocation = DragStartLocation + Axis * TotalMovement;

			if (bIsBoneMode)
			{
				FTransform NewWorldTransform;
				NewWorldTransform.Translation = NewLocation;
				NewWorldTransform.Rotation = DragStartRotation;
				NewWorldTransform.Scale3D = DragStartScale;
				TargetSkeletalMeshComponent->SetBoneWorldTransform(TargetBoneIndex, NewWorldTransform);
			}
			else if (bIsShapeMode)
			{
				// 디버그: Translate Shape 분기 진입
				{
					char debugMsg[256];
					sprintf_s(debugMsg, "[OnDrag Translate] Shape branch! NewLoc=(%.2f,%.2f,%.2f)\n",
						NewLocation.X, NewLocation.Y, NewLocation.Z);
					OutputDebugStringA(debugMsg);
				}
				// Shape의 새로운 월드 위치를 로컬 Center로 변환
				FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);
				FVector NewLocalCenter = BoneWorldTransform.Rotation.Inverse().RotateVector(NewLocation - BoneWorldTransform.Translation);

				// Shape의 Center 업데이트
				switch (TargetShapeType)
				{
				case EAggCollisionShape::Sphere:
					if (TargetShapeIndex < TargetBodySetup->AggGeom.SphereElems.Num())
						TargetBodySetup->AggGeom.SphereElems[TargetShapeIndex].Center = NewLocalCenter;
					break;
				case EAggCollisionShape::Box:
					if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
						TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex].Center = NewLocalCenter;
					break;
				case EAggCollisionShape::Capsule:
					if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
						TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex].Center = NewLocalCenter;
					break;
				default:
					break;
				}
			}
			else if (bIsConstraintMode)
			{
				// Constraint는 Translate 의미 없음 - 무시
			}
			else if (Target)
			{
				Target->SetWorldLocation(NewLocation);
			}
		}
		else // Scale
		{
			FVector NewScale = DragStartScale;
			switch (DraggingAxis)
			{
			case 1: NewScale.X += TotalMovement; break;
			case 2: NewScale.Y += TotalMovement; break;
			case 4: NewScale.Z += TotalMovement; break;  // Z축
			}

			if (bIsBoneMode)
			{
				// DragStart 시점의 Location/Rotation 유지하며 Scale만 변경
				FTransform NewWorldTransform;
				NewWorldTransform.Translation = DragStartLocation;
				NewWorldTransform.Rotation = DragStartRotation;
				NewWorldTransform.Scale3D = NewScale;
				TargetSkeletalMeshComponent->SetBoneWorldTransform(TargetBoneIndex, NewWorldTransform);
			}
			else if (bIsShapeMode)
			{
				// 디버그: Scale Shape 분기 진입
				{
					char debugMsg[256];
					sprintf_s(debugMsg, "[OnDrag Scale] Shape branch! ShapeType=%d, ShapeIndex=%d, DraggingAxis=%u, TotalMovement=%.3f\n",
						(int)TargetShapeType, TargetShapeIndex, DraggingAxis, TotalMovement);
					OutputDebugStringA(debugMsg);
				}
				// Shape의 크기 업데이트 (축에 따라 다른 속성)
				switch (TargetShapeType)
				{
				case EAggCollisionShape::Sphere:
					if (TargetShapeIndex < TargetBodySetup->AggGeom.SphereElems.Num())
					{
						// Sphere는 모든 축에서 Radius만 조정
						float NewRadius = DragStartShapeRadius + TotalMovement;
						if (NewRadius > 0.01f)
							TargetBodySetup->AggGeom.SphereElems[TargetShapeIndex].Radius = NewRadius;
					}
					break;
				case EAggCollisionShape::Box:
					if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
					{
						FKBoxElem& Box = TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex];
						switch (DraggingAxis)
						{
						case 1: Box.X = FMath::Max(0.01f, DragStartShapeX + TotalMovement); break;
						case 2: Box.Y = FMath::Max(0.01f, DragStartShapeY + TotalMovement); break;
						case 4: Box.Z = FMath::Max(0.01f, DragStartShapeZ + TotalMovement); break;  // Z축
						}
					}
					break;
				case EAggCollisionShape::Capsule:
					if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
					{
						FKCapsuleElem& Capsule = TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex];
						// Capsule은 X축 방향으로 정렬됨 (PhysX 컨벤션)
						// X축 = Length, Y/Z축 = Radius
						switch (DraggingAxis)
						{
						case 1:  // X축 = Length
							Capsule.Length = FMath::Max(0.01f, DragStartShapeLength + TotalMovement);
							break;
						case 2:  // Y축 = Radius
						case 4:  // Z축 = Radius
							Capsule.Radius = FMath::Max(0.01f, DragStartShapeRadius + TotalMovement);
							break;
						}
					}
					break;
				default:
					break;
				}
			}
			else if (bIsConstraintMode)
			{
				// Constraint는 Scale 의미 없음 - 무시
			}
			else if (Target)
			{
				// 디버그: Scale Target (Actor/Component) 분기 진입
				OutputDebugStringA("[OnDrag Scale] Target (Actor/Component) branch - NOT Shape!\n");
				Target->SetWorldScale(NewScale);
			}
		}
		break;
	}
	case EGizmoMode::Rotate:
	{
		// ==============================================================
		// UGizmoManager의 정밀 회전 로직 (Stateful, Total Offset 기반)
		// ==============================================================

		float RotationSpeed = 0.005f;

		// ProcessGizmoDragging에서 미리 계산해둔 2D 스크린 드래그 벡터(DragScreenVector) 사용
		float ProjectedAmount = (MouseOffset.X * DragScreenVector.X + MouseOffset.Y * DragScreenVector.Y);

		// 총 회전 각도 계산
		float TotalAngle = ProjectedAmount * RotationSpeed;
		// 게이지 표시용 각도 저장 (±2π 범위로 정규화, 음수는 역방향 게이지)
	CurrentRotationAngle = std::fmod(TotalAngle, 2.0f * PI);

		// 회전의 기준이 될 로컬 축 벡터
		FVector LocalAxisVector;
		switch (DraggingAxis)
		{
		case 1: LocalAxisVector = FVector(1, 0, 0); break;  // X축
		case 2: LocalAxisVector = FVector(0, 1, 0); break;  // Y축
		case 4: LocalAxisVector = FVector(0, 0, 1); break;  // Z축 (Picking 시스템 매칭)
		default: LocalAxisVector = FVector(1, 0, 0);
		}

		FQuat NewRot;
		if (CurrentSpace == EGizmoSpace::World)
		{
			// 월드 축(LocalAxisVector)을 기준으로 총 각도(TotalAngle)만큼 회전하는 델타 계산
			FQuat DeltaQuat = FQuat::FromAxisAngle(LocalAxisVector, TotalAngle);
			// '시작 회전 * 델타'로 최종 회전 계산
			NewRot = DeltaQuat * DragStartRotation;
		}
		else // Local
		{
			// 로컬 축을 드래그 시작 시점의 월드 축으로 변환
			FVector WorldSpaceRotationAxis = DragStartRotation.RotateVector(LocalAxisVector);

			// 월드 공간에서 변환된 축을 기준으로 총 각도만큼 회전하는 델타 계산
			FQuat DeltaQuat = FQuat::FromAxisAngle(WorldSpaceRotationAxis, TotalAngle);
			// '시작 회전 * 델타'로 최종 회전 계산
			NewRot = DeltaQuat * DragStartRotation;
		}

		if (bIsBoneMode)
		{
			// DragStart 시점의 Location/Scale 유지하며 Rotation만 변경
			FTransform NewWorldTransform;
			NewWorldTransform.Translation = DragStartLocation;
			NewWorldTransform.Rotation = NewRot;
			NewWorldTransform.Scale3D = DragStartScale;
			TargetSkeletalMeshComponent->SetBoneWorldTransform(TargetBoneIndex, NewWorldTransform);
		}
		else if (bIsShapeMode)
		{
			// Shape의 새로운 월드 회전을 로컬 Rotation으로 변환
			// NewRot = BoneWorldRot * ShapeLocalRot 이므로
			// ShapeLocalRot = BoneWorldRot.Inverse * NewRot
			FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);
			FQuat NewShapeLocalRot = BoneWorldTransform.Rotation.Inverse() * NewRot;
			FVector NewShapeLocalEuler = NewShapeLocalRot.ToEulerZYXDeg();

			// Shape의 Rotation 업데이트 (Sphere는 회전 없음)
			switch (TargetShapeType)
			{
			case EAggCollisionShape::Box:
				if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
					TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex].Rotation = NewShapeLocalEuler;
				break;
			case EAggCollisionShape::Capsule:
				if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
					TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex].Rotation = NewShapeLocalEuler;
				break;
			default:
				break;  // Sphere has no rotation
			}
		}
		else if (bIsConstraintMode)
		{
			// Constraint 프레임 회전 업데이트
			// 월드 회전을 부모 본 기준 로컬 회전으로 변환
			FTransform Bone1Transform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetConstraintBone1Index);
			FQuat NewLocalRot = Bone1Transform.Rotation.Inverse() * NewRot;

			// Constraint의 Frame1 회전 업데이트 (부모 본 기준) - Euler degrees로 저장
			TargetConstraintSetup->ConstraintRotationInBody1 = NewLocalRot.ToEulerZYXDeg();
		}
		else
		{
			Target->SetWorldRotation(NewRot);
		}
		break;
	}
	}
}

void AGizmoActor::ProcessGizmoInteraction(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	// Skip interaction when disabled (e.g., when Particle Editor is focused)
	if (!bInteractionEnabled)
	{
		return;
	}

	// ImGui 드래그 드롭 중에는 기즈모 인터랙션 무시
	const ImGuiPayload* DragPayload = ImGui::GetDragDropPayload();
	if (DragPayload != nullptr)
	{
		return;
	}

	if (!SelectionManager)
	{
		return;
	}

	// Physics Asset 타겟 모드 (Bone, Shape, Constraint)일 때는 SelectedComponent가 없어도 기즈모 인터랙션 처리
	bool bIsBoneTargetMode = (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0);
	bool bIsShapeTargetMode = (TargetType == EGizmoTargetType::Shape && TargetSkeletalMeshComponent && TargetBodySetup && TargetBoneIndex >= 0 && TargetShapeIndex >= 0);
	bool bIsConstraintTargetMode = (TargetType == EGizmoTargetType::Constraint && TargetSkeletalMeshComponent && TargetConstraintSetup);
	bool bIsPhysicsAssetTargetMode = bIsBoneTargetMode || bIsShapeTargetMode || bIsConstraintTargetMode;

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();

	if (!bIsPhysicsAssetTargetMode && (!SelectedComponent || !Camera))
	{
		return;
	}
	if (!Camera)
	{
		return;
	}

	// 기즈모 드래그
	ProcessGizmoDragging(Camera, Viewport, MousePositionX, MousePositionY);

	// 호버링 (드래그 중이 아닐 때만)
	if (!bIsDragging)
	{
		ProcessGizmoHovering(Camera, Viewport, MousePositionX, MousePositionY);
	}
}

void AGizmoActor::ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	if (!Camera) return;

	FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
	FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));
	FVector2D ViewportMousePos(static_cast<float>(MousePositionX) + ViewportOffset.X,
		static_cast<float>(MousePositionY) + ViewportOffset.Y);

	if (!bIsDragging)
	{
		GizmoAxis = CPickingSystem::IsHoveringGizmoForViewport(
			this,
			Camera,
			ViewportMousePos,
			ViewportSize,
			ViewportOffset,
			Viewport,
			HoverImpactPoint
		);
	}

	if (GizmoAxis > 0)	//기즈모 축이 0이상이라면 선택 된것
	{
		bIsHovering = true;
	}
	else
	{
		bIsHovering = false;
	}
}

void AGizmoActor::ProcessGizmoDragging(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	// Physics Asset 타겟 모드 (Bone, Shape, Constraint)일 때는 SelectedComponent가 없어도 처리
	bool bIsBoneTargetMode = (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0);
	bool bIsShapeTargetMode = (TargetType == EGizmoTargetType::Shape && TargetSkeletalMeshComponent && TargetBodySetup && TargetBoneIndex >= 0 && TargetShapeIndex >= 0);
	bool bIsConstraintTargetMode = (TargetType == EGizmoTargetType::Constraint && TargetSkeletalMeshComponent && TargetConstraintSetup);
	bool bIsPhysicsAssetTargetMode = bIsBoneTargetMode || bIsShapeTargetMode || bIsConstraintTargetMode;

	USceneComponent* SelectedComponent = SelectionManager ? SelectionManager->GetSelectedComponent() : nullptr;

	if (!bIsPhysicsAssetTargetMode && !SelectedComponent)
	{
		// 드래그 중인데 선택 대상이 사라진 경우 드래그 종료
		if (bIsDragging)
		{
			EndDrag();
		}
		return;
	}
	if (!Camera)
	{
		return;
	}

	FVector2D CurrentMousePosition(MousePositionX, MousePositionY);

	// GetAsyncKeyState로 OS 레벨에서 마우스 버튼 상태 직접 확인
	bool bMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	// --- 1. Begin Drag (드래그 시작) ---
	// 본 모드가 아닐 때만 자동 시작 (기존 에디터 뷰포트 호환성 유지)
	// 본 모드일 때는 StartDrag()가 명시적으로 호출되어야 함
	// Shape/Constraint 모드는 자동 시작 허용
	bool bAllowAutoDrag = !bIsBoneTargetMode;  // Shape, Constraint, 일반 Actor/Component 모드
	if (bAllowAutoDrag && bMouseDown && !bIsDragging && GizmoAxis > 0)
	{
		bIsDragging = true;
		bDuplicatedThisDrag = false;  // 새 드래그 시작 시 복사 플래그 리셋
		DraggingAxis = GizmoAxis;
		DragCamera = Camera;

		// Alt 키가 눌려있고 아직 이번 드래그에서 복사하지 않았으면 Actor / Component 복제
		// GetAsyncKeyState로 OS 레벨에서 Alt 키 상태 직접 확인 (마우스 버튼과 동기화)
		bool bIsAltDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
		if (bIsAltDown && !bDuplicatedThisDrag)
		{
			if (SelectionManager->IsActorMode())
			{
				// 액터 모드: 액터 전체 복제
				AActor* SelectedActor = SelectionManager->GetSelectedActor();
				if (SelectedActor && World)
				{
					AActor* DuplicatedActor = SelectedActor->Duplicate();
					if (DuplicatedActor)
					{
						DuplicatedActor->SetActorLocation(SelectedActor->GetActorLocation());
						DuplicatedActor->SetActorRotation(SelectedActor->GetActorRotation());
						DuplicatedActor->SetActorScale(SelectedActor->GetActorScale());

						FString ActorTypeName = SelectedActor->GetClass()->Name;
						FString UniqueName = World->GenerateUniqueActorName(ActorTypeName);
						DuplicatedActor->ObjectName = FName(UniqueName);

						World->AddActorToLevel(DuplicatedActor);

						SelectionManager->ClearSelection();
						SelectionManager->SelectActor(DuplicatedActor);

						SelectedComponent = DuplicatedActor->GetRootComponent();
						bDuplicatedThisDrag = true;
					}
				}
			}
			else
			{
				// 컴포넌트 모드: 선택된 컴포넌트만 복제
				USceneComponent* SourceComponent = Cast<USceneComponent>(SelectionManager->GetSelectedComponent());
				AActor* OwnerActor = SelectionManager->GetSelectedActor();
				if (SourceComponent && OwnerActor && !SourceComponent->IsNative())
				{
					// 컴포넌트 복제
					USceneComponent* DuplicatedComponent = Cast<USceneComponent>(SourceComponent->Duplicate());
					if (DuplicatedComponent)
					{
						// 액터에 추가
						OwnerActor->AddOwnedComponent(DuplicatedComponent);

						// 같은 부모에 붙이기
						USceneComponent* ParentComponent = SourceComponent->GetAttachParent();
						if (ParentComponent)
						{
							DuplicatedComponent->SetupAttachment(ParentComponent, EAttachmentRule::KeepRelative);
						}

						// 월드에 등록
						if (World)
						{
							DuplicatedComponent->RegisterComponent(World);
						}

						// 새 컴포넌트 선택
						SelectionManager->SelectComponent(DuplicatedComponent);
						SelectedComponent = DuplicatedComponent;
						bDuplicatedThisDrag = true;
					}
				}
			}
		}

		// 드래그 시작 상태 저장
		DragStartPosition = CurrentMousePosition;
		DragImpactPoint = HoverImpactPoint;

		if (bIsShapeTargetMode)
		{
			// Shape 모드: 실제 Shape 월드 위치/회전 계산 (StartDrag와 동일한 로직)
			FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);

			FVector ShapeLocalCenter = FVector::Zero();
			FVector ShapeLocalRotation = FVector::Zero();

			// Shape별 시작 데이터 저장 및 로컬 Center/Rotation 가져오기
			switch (TargetShapeType)
			{
			case EAggCollisionShape::Sphere:
				if (TargetShapeIndex < TargetBodySetup->AggGeom.SphereElems.Num())
				{
					FKSphereElem& Sphere = TargetBodySetup->AggGeom.SphereElems[TargetShapeIndex];
					ShapeLocalCenter = Sphere.Center;
					DragStartShapeCenter = Sphere.Center;
					DragStartShapeRadius = Sphere.Radius;
				}
				break;
			case EAggCollisionShape::Box:
				if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
				{
					FKBoxElem& Box = TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex];
					ShapeLocalCenter = Box.Center;
					ShapeLocalRotation = Box.Rotation;
					DragStartShapeCenter = Box.Center;
					DragStartShapeRotation = Box.Rotation;
					DragStartShapeX = Box.X;
					DragStartShapeY = Box.Y;
					DragStartShapeZ = Box.Z;
				}
				break;
			case EAggCollisionShape::Capsule:
				if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
				{
					FKCapsuleElem& Capsule = TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex];
					ShapeLocalCenter = Capsule.Center;
					ShapeLocalRotation = Capsule.Rotation;
					DragStartShapeCenter = Capsule.Center;
					DragStartShapeRotation = Capsule.Rotation;
					DragStartShapeRadius = Capsule.Radius;
					DragStartShapeLength = Capsule.Length;
				}
				break;
			default:
				break;
			}

			// Shape의 월드 위치와 회전 계산
			DragStartLocation = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(ShapeLocalCenter);
			FQuat ShapeLocalQuat = FQuat::MakeFromEulerZYX(ShapeLocalRotation);
			DragStartRotation = BoneWorldTransform.Rotation * ShapeLocalQuat;
			DragStartScale = FVector::One();
		}
		else if (bIsConstraintTargetMode)
		{
			// Constraint 모드: 실제 Constraint 월드 회전 계산 (StartDrag와 동일한 로직)
			FTransform Bone1Transform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetConstraintBone1Index);
			FTransform Bone2Transform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetConstraintBone2Index);
			DragStartLocation = Bone2Transform.Translation;

			// Constraint의 현재 프레임 로컬 회전
			FQuat ConstraintLocalRot = FQuat::MakeFromEulerZYX(TargetConstraintSetup->ConstraintRotationInBody1);
			// Constraint 월드 회전 = Parent 본 회전 * Constraint 로컬 회전
			DragStartRotation = Bone1Transform.Rotation * ConstraintLocalRot;

			DragStartScale = FVector::One();
			DragStartConstraintRotation = ConstraintLocalRot;
		}
		else if (SelectedComponent)
		{
			// 일반 Actor/Component 모드
			DragStartLocation = SelectedComponent->GetWorldLocation();
			DragStartRotation = SelectedComponent->GetWorldRotation();
			DragStartScale = SelectedComponent->GetWorldScale();
		}

		// (회전용) 드래그 시작 시점의 2D 드래그 벡터 계산
		if (CurrentMode == EGizmoMode::Rotate)
		{
			FVector WorldAxis(0.f);
			FVector LocalAxisVector(0.f);
			switch (DraggingAxis)
			{
			case 1: LocalAxisVector = FVector(1, 0, 0); break;  // X축
			case 2: LocalAxisVector = FVector(0, 1, 0); break;  // Y축
			case 4: LocalAxisVector = FVector(0, 0, 1); break;  // Z축 (Picking 시스템 매칭)
			}

			if (CurrentSpace == EGizmoSpace::World)
			{
				WorldAxis = LocalAxisVector;
			}
			else
			{
				WorldAxis = DragStartRotation.RotateVector(LocalAxisVector);
			}

			FVector ClickVector = DragImpactPoint - DragStartLocation;
			FVector Tangent3D = FVector::Cross(WorldAxis, ClickVector);

			float RightDot = FVector::Dot(Tangent3D, DragCamera->GetRight());
			float UpDot = FVector::Dot(Tangent3D, DragCamera->GetUp());

			DragScreenVector = FVector2D(RightDot, -UpDot);
			DragScreenVector = DragScreenVector.GetNormalized();
		}
	}

	// --- 2. Continue Drag (드래그 지속) ---
	if (bMouseDown && bIsDragging)
	{
		// 드래그 시작점으로부터의 '총 변위(Total Offset)' 계산
		FVector2D MouseOffset = CurrentMousePosition - DragStartPosition;

		// OnDrag 함수에 고정된 축(DraggingAxis)과 총 변위(MouseOffset)를 전달
		OnDrag(SelectedComponent, DraggingAxis, MouseOffset.X, MouseOffset.Y, Camera, Viewport);

		// Bone 타겟일 때 본 라인 실시간 업데이트
		if (bIsBoneTargetMode && TargetSkeletalMeshComponent)
		{
			AActor* Owner = TargetSkeletalMeshComponent->GetOwner();
			if (Owner)
			{
				if (auto* SkeletalActor = dynamic_cast<class ASkeletalMeshActor*>(Owner))
				{
					SkeletalActor->RebuildBoneLines(TargetBoneIndex, false);
				}
			}
		}
	}

	// --- 3. End Drag (드래그 종료) ---
	// 드래그 중인데 마우스가 떼졌으면 무조건 종료
	if (bIsDragging && !bMouseDown)
	{
		EndDrag();
	}
}

void AGizmoActor::ProcessGizmoModeSwitch()
{
	// Skip mode switch when interaction is disabled (e.g., when Particle Editor is focused)
	if (!bInteractionEnabled)
	{
		return;
	}

	// 우클릭 드래그 중에는 기즈모 모드/스페이스 변경 불가
	if (InputManager->IsMouseButtonDown(RightButton) || bIsDragging) // 드래그 중 변경 불가
	{
		return;
	}

	// Q 키 또는 ESC 키: Select 모드
	if (InputManager->IsKeyPressed('Q') || InputManager->IsKeyPressed(VK_ESCAPE))
	{
		SetMode(EGizmoMode::Select);
	}
	// W 키: Move (Translate) 모드
	else if (InputManager->IsKeyPressed('W'))
	{
		SetMode(EGizmoMode::Translate);
	}
	// E 키: Rotate 모드
	else if (InputManager->IsKeyPressed('E'))
	{
		SetMode(EGizmoMode::Rotate);
	}
	// R 키: Scale 모드
	else if (InputManager->IsKeyPressed('R'))
	{
		SetMode(EGizmoMode::Scale);
	}
	// 스페이스 키로 기즈모 모드 순환 전환
	else if (InputManager->IsKeyPressed(VK_SPACE) && !InputManager->IsKeyDown(VK_CONTROL))
	{
		int GizmoModeIndex = static_cast<int>(GetMode());
		GizmoModeIndex = (GizmoModeIndex + 1) % static_cast<uint32>(EGizmoMode::Select);	// 3 = enum 개수
		EGizmoMode NewGizmoMode = static_cast<EGizmoMode>(GizmoModeIndex);
		NextMode(NewGizmoMode);
	}
	// Ctrl + ` 키로 월드-로컬 모드 전환
	if (InputManager->IsKeyDown(VK_CONTROL) && InputManager->IsKeyPressed(VK_OEM_3))
	{
		if (GetSpace() == EGizmoSpace::World)
		{
			SetSpace(EGizmoSpace::Local);
		}
		else
		{
			SetSpace(EGizmoSpace::World);
		}
	}
}

bool AGizmoActor::StartDrag(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	// 이미 드래그 중이면 무시
	if (bIsDragging)
	{
		return false;
	}

	// 호버링 상태가 아니면 드래그 시작 불가
	if (GizmoAxis <= 0)
	{
		return false;
	}

	// 본/Shape/Constraint 타겟 모드 체크
	bool bIsBoneTargetMode = (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0);
	bool bIsShapeTargetMode = (TargetType == EGizmoTargetType::Shape && TargetSkeletalMeshComponent && TargetBodySetup && TargetBoneIndex >= 0 && TargetShapeIndex >= 0);
	bool bIsConstraintTargetMode = (TargetType == EGizmoTargetType::Constraint && TargetSkeletalMeshComponent && TargetConstraintSetup && TargetConstraintBone1Index >= 0 && TargetConstraintBone2Index >= 0);
	USceneComponent* SelectedComponent = SelectionManager ? SelectionManager->GetSelectedComponent() : nullptr;

	if (!bIsBoneTargetMode && !bIsShapeTargetMode && !bIsConstraintTargetMode && !SelectedComponent)
	{
		return false;
	}

	// 드래그 시작
	bIsDragging = true;
	DraggingAxis = GizmoAxis;
	DragCamera = Camera;

	// InputManager에 기즈모 드래그 상태 알림
	InputManager->SetIsGizmoDragging(true);

	FVector2D CurrentMousePosition(MousePositionX, MousePositionY);

	// 드래그 시작 상태 저장 (SetBoneEditingMode 호출 전에 해야 델타가 적용된 위치를 가져옴)
	if (bIsBoneTargetMode)
	{
		FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);
		DragStartLocation = BoneWorldTransform.Translation;
		DragStartRotation = BoneWorldTransform.Rotation;
		DragStartScale = BoneWorldTransform.Scale3D;
	}
	else if (bIsShapeTargetMode)
	{
		// Shape 모드: 본의 월드 트랜스폼과 Shape의 로컬 데이터 저장
		FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);

		// Shape의 로컬 Center로 월드 위치 계산
		FVector ShapeLocalCenter = FVector::Zero();
		FVector ShapeLocalRotation = FVector::Zero();

		switch (TargetShapeType)
		{
		case EAggCollisionShape::Sphere:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.SphereElems.Num())
			{
				FKSphereElem& Sphere = TargetBodySetup->AggGeom.SphereElems[TargetShapeIndex];
				ShapeLocalCenter = Sphere.Center;
				DragStartShapeCenter = Sphere.Center;
				DragStartShapeRadius = Sphere.Radius;
			}
			break;
		case EAggCollisionShape::Box:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.BoxElems.Num())
			{
				FKBoxElem& Box = TargetBodySetup->AggGeom.BoxElems[TargetShapeIndex];
				ShapeLocalCenter = Box.Center;
				ShapeLocalRotation = Box.Rotation;
				DragStartShapeCenter = Box.Center;
				DragStartShapeRotation = Box.Rotation;
				DragStartShapeX = Box.X;
				DragStartShapeY = Box.Y;
				DragStartShapeZ = Box.Z;
			}
			break;
		case EAggCollisionShape::Capsule:
			if (TargetShapeIndex < TargetBodySetup->AggGeom.SphylElems.Num())
			{
				FKCapsuleElem& Capsule = TargetBodySetup->AggGeom.SphylElems[TargetShapeIndex];
				ShapeLocalCenter = Capsule.Center;
				ShapeLocalRotation = Capsule.Rotation;
				DragStartShapeCenter = Capsule.Center;
				DragStartShapeRotation = Capsule.Rotation;
				DragStartShapeRadius = Capsule.Radius;
				DragStartShapeLength = Capsule.Length;
			}
			break;
		default:
			break;
		}

		// Shape의 월드 위치를 DragStartLocation으로 저장
		DragStartLocation = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(ShapeLocalCenter);

		// Shape의 월드 회전을 DragStartRotation으로 저장
		FQuat ShapeLocalQuat = FQuat::MakeFromEulerZYX(ShapeLocalRotation);
		DragStartRotation = BoneWorldTransform.Rotation * ShapeLocalQuat;

		DragStartScale = FVector::One();
	}
	else if (bIsConstraintTargetMode)
	{
		// Constraint 모드: Parent 본(Bone1)과 Child 본(Bone2)의 트랜스폼으로 Constraint 위치 계산
		FTransform Bone1Transform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetConstraintBone1Index);
		FTransform Bone2Transform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetConstraintBone2Index);

		// Constraint 위치 = 두 본 사이의 원뿔 꼭지점 (Bone2 위치)
		DragStartLocation = Bone2Transform.Translation;

		// Constraint의 현재 프레임 로컬 회전
		FQuat ConstraintLocalRot = FQuat::MakeFromEulerZYX(TargetConstraintSetup->ConstraintRotationInBody1);

		// Constraint 월드 회전 = Parent 본 회전 * Constraint 로컬 회전 (Shape와 동일한 패턴)
		DragStartRotation = Bone1Transform.Rotation * ConstraintLocalRot;

		DragStartScale = FVector::One();

		// Constraint의 현재 프레임 회전 저장 (Euler degrees → Quaternion)
		DragStartConstraintRotation = ConstraintLocalRot;
	}

	// 본 타겟 모드일 때 편집 중인 본 인덱스 설정 (AnimInstance가 해당 본을 덮어쓰지 않도록)
	if (bIsBoneTargetMode)
	{
		TargetSkeletalMeshComponent->SetBoneEditingMode(true, TargetBoneIndex);
	}
	else if (!bIsShapeTargetMode && !bIsConstraintTargetMode && SelectedComponent)
	{
		// Actor/Component 모드: 선택된 컴포넌트의 트랜스폼 저장
		DragStartLocation = SelectedComponent->GetWorldLocation();
		DragStartRotation = SelectedComponent->GetWorldRotation();
		DragStartScale = SelectedComponent->GetWorldScale();
	}
	DragStartPosition = CurrentMousePosition;

	// 호버링 시점의 3D 충돌 지점을 드래그 시작 지점으로 래치
	DragImpactPoint = HoverImpactPoint;

	// (회전용) 드래그 시작 시점의 2D 드래그 벡터 계산
	if (CurrentMode == EGizmoMode::Rotate && DragCamera)
	{
		FVector WorldAxis(0.f);
		FVector LocalAxisVector(0.f);
		switch (DraggingAxis)
		{
		case 1: LocalAxisVector = FVector(1, 0, 0); break;  // X축
		case 2: LocalAxisVector = FVector(0, 1, 0); break;  // Y축
		case 4: LocalAxisVector = FVector(0, 0, 1); break;  // Z축 (Picking 시스템 매칭)
		}

		if (CurrentSpace == EGizmoSpace::World)
		{
			WorldAxis = LocalAxisVector;
		}
		else // Local
		{
			WorldAxis = DragStartRotation.RotateVector(LocalAxisVector);
		}

		// 3D 충돌 지점과 기즈모 중심 사이의 벡터
		FVector ClickVector = DragImpactPoint - DragStartLocation;

		// 3D 회전 축과 3D 클릭 벡터를 외적하여 3D 접선을 구함
		FVector Tangent3D = FVector::Cross(WorldAxis, ClickVector);

		// 3D 접선을 2D 스크린에 투영
		float RightDot = FVector::Dot(Tangent3D, DragCamera->GetRight());
		float UpDot = FVector::Dot(Tangent3D, DragCamera->GetUp());

		// DirectX 스크린 좌표계 (Y축 반전)를 고려하고 정규화
		DragScreenVector = FVector2D(RightDot, -UpDot);
		DragScreenVector = DragScreenVector.GetNormalized();
	}

	return true;
}

void AGizmoActor::EndDrag()
{
	if (!bIsDragging)
	{
		return;
	}

	USceneComponent* SelectedComponent = SelectionManager ? SelectionManager->GetSelectedComponent() : nullptr;

	// 본 타겟 모드였다면 Bone Editing 모드 해제
	bool bIsBoneTargetMode = (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0);
	if (bIsBoneTargetMode)
	{
		TargetSkeletalMeshComponent->SetBoneEditingMode(false);
	}

	bIsDragging = false;
	bDuplicatedThisDrag = false;  // 복사 플래그 리셋
	DraggingAxis = 0;
	DragCamera = nullptr;
	CurrentRotationAngle = 0.0f;  // 회전 각도 리셋
	GizmoAxis = 0; // 하이라이트 해제

	// InputManager에 기즈모 드래그 종료 알림
	if (InputManager)
	{
		InputManager->SetIsGizmoDragging(false);
	}

	SetSpaceWorldMatrix(CurrentSpace, SelectedComponent);
}

void AGizmoActor::UpdateComponentVisibility()
{
	// 선택된 액터가 있거나 Physics Asset 타겟(Bone/Shape/Constraint)이 있으면 기즈모 표시
	bool bHasBoneTarget = (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0);
	bool bHasShapeTarget = (TargetType == EGizmoTargetType::Shape && TargetSkeletalMeshComponent && TargetBodySetup && TargetBoneIndex >= 0 && TargetShapeIndex >= 0);
	bool bHasConstraintTarget = (TargetType == EGizmoTargetType::Constraint && TargetSkeletalMeshComponent && TargetConstraintSetup);
	bool bHasSelection = (SelectionManager && SelectionManager->GetSelectedComponent()) ||
	                     bHasBoneTarget || bHasShapeTarget || bHasConstraintTarget;

	// bRender 플래그 설정 (RenderGizmoExtensions 실행 조건)
	bRender = bHasSelection;

	// 드래그 중일 때는 고정된 축(DraggingAxis)을, 아닐 때는 호버 축(GizmoAxis)을 사용
	uint32 HighlightAxis = bIsDragging ? DraggingAxis : GizmoAxis;

	// Arrow Components (Translate 모드)
	bool bShowArrows = bHasSelection && (CurrentMode == EGizmoMode::Translate);
	if (ArrowX) { ArrowX->SetActive(bShowArrows); ArrowX->SetHighlighted(ShouldHighlightAxis(HighlightAxis, 1), 1); }
	if (ArrowY) { ArrowY->SetActive(bShowArrows); ArrowY->SetHighlighted(ShouldHighlightAxis(HighlightAxis, 2), 2); }
	if (ArrowZ) { ArrowZ->SetActive(bShowArrows); ArrowZ->SetHighlighted(ShouldHighlightAxis(HighlightAxis, 4), 4); }

	// Rotate Components (Rotate 모드) - QuarterRing procedural rendering 사용, old components 비활성화
	bool bShowRotates = false;  // QuarterRing 시스템으로 대체됨
	if (RotateX) { RotateX->SetActive(bShowRotates); RotateX->SetHighlighted(false, 1); }
	if (RotateY) { RotateY->SetActive(bShowRotates); RotateY->SetHighlighted(false, 2); }
	if (RotateZ) { RotateZ->SetActive(bShowRotates); RotateZ->SetHighlighted(false, 3); }

	// Scale Components (Scale 모드)
	bool bShowScales = bHasSelection && (CurrentMode == EGizmoMode::Scale);
	if (ScaleX) { ScaleX->SetActive(bShowScales); ScaleX->SetHighlighted(ShouldHighlightAxis(HighlightAxis, 1), 1); }
	if (ScaleY) { ScaleY->SetActive(bShowScales); ScaleY->SetHighlighted(ShouldHighlightAxis(HighlightAxis, 2), 2); }
	if (ScaleZ) { ScaleZ->SetActive(bShowScales); ScaleZ->SetHighlighted(ShouldHighlightAxis(HighlightAxis, 4), 4); }
}

bool AGizmoActor::ShouldHighlightAxis(uint32 HighlightValue, uint32 AxisDirection) const
{
	if (HighlightValue == 0)
	{
		return false;
	}

	// 정확히 일치하면 하이라이트
	if (HighlightValue == AxisDirection)
	{
		return true;
	}

	// 평면 기즈모 선택 시 해당 축들도 하이라이트
	if (HighlightValue == 8)  // XY 평면
	{
		return (AxisDirection == 1 || AxisDirection == 2);  // X축, Y축
	}
	if (HighlightValue == 16) // XZ 평면
	{
		return (AxisDirection == 1 || AxisDirection == 4);  // X축, Z축
	}
	if (HighlightValue == 32) // YZ 평면
	{
		return (AxisDirection == 2 || AxisDirection == 4);  // Y축, Z축
	}

	// Scale 모드에서 중심 구체 선택 시 모든 축 하이라이트
	if (HighlightValue == 64 && CurrentMode == EGizmoMode::Scale)
	{
		return (AxisDirection == 1 || AxisDirection == 2 || AxisDirection == 4);  // 모든 축
	}

	return false;
}

void AGizmoActor::OnDrag(USceneComponent* SelectedComponent, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera)
{
	OnDrag(SelectedComponent, GizmoAxis, MouseDeltaX, MouseDeltaY, Camera, nullptr);
}

// ────────────────────────────────────────────────────────
// Bone Target Functions
// ────────────────────────────────────────────────────────

void AGizmoActor::SetBoneTarget(USkeletalMeshComponent* InComponent, int32 InBoneIndex)
{
	if (!InComponent || InBoneIndex < 0)
	{
		ClearBoneTarget();
		return;
	}

	TargetType = EGizmoTargetType::Bone;
	TargetSkeletalMeshComponent = InComponent;
	TargetBoneIndex = InBoneIndex;

	// 기즈모 위치를 본의 월드 트랜스폼으로 설정
	FTransform BoneWorldTransform = InComponent->GetBoneWorldTransform(InBoneIndex);
	RootComponent->SetWorldLocation(BoneWorldTransform.Translation);
	RootComponent->SetWorldRotation(BoneWorldTransform.Rotation);

	// Local 모드에서는 본의 로컬 회전 사용
	SetSpaceWorldMatrix(CurrentSpace, nullptr);
}

void AGizmoActor::ClearBoneTarget()
{
	TargetType = EGizmoTargetType::Actor;
	TargetSkeletalMeshComponent = nullptr;
	TargetBoneIndex = -1;
}

// ────────────────────────────────────────────────────────
// Constraint Target Functions
// ────────────────────────────────────────────────────────

void AGizmoActor::SetConstraintTarget(USkeletalMeshComponent* InComponent, UPhysicsConstraintSetup* InConstraint, int32 InBone1Index, int32 InBone2Index)
{
	if (!InComponent || !InConstraint || InBone1Index < 0 || InBone2Index < 0)
	{
		ClearConstraintTarget();
		return;
	}

	TargetType = EGizmoTargetType::Constraint;
	TargetSkeletalMeshComponent = InComponent;
	TargetConstraintSetup = InConstraint;
	TargetConstraintBone1Index = InBone1Index;
	TargetConstraintBone2Index = InBone2Index;

	// 두 본의 월드 트랜스폼
	FTransform Bone1Transform = InComponent->GetBoneWorldTransform(InBone1Index);
	FTransform Bone2Transform = InComponent->GetBoneWorldTransform(InBone2Index);

	// Constraint 위치: Child 본(Bone2) 위치 (시각화/피킹과 일치)
	FVector ConstraintPos = Bone2Transform.Translation;

	// Constraint 회전: Parent 본(Bone1)의 회전 사용 (기준 프레임)
	FQuat ConstraintRot = Bone1Transform.Rotation;

	RootComponent->SetWorldLocation(ConstraintPos);
	RootComponent->SetWorldRotation(ConstraintRot);

	SetSpaceWorldMatrix(CurrentSpace, nullptr);
}

void AGizmoActor::ClearConstraintTarget()
{
	TargetType = EGizmoTargetType::Actor;
	TargetSkeletalMeshComponent = nullptr;
	TargetConstraintSetup = nullptr;
	TargetConstraintBone1Index = -1;
	TargetConstraintBone2Index = -1;
}

// ────────────────────────────────────────────────────────
// Shape Target Functions
// ────────────────────────────────────────────────────────

void AGizmoActor::SetShapeTarget(USkeletalMeshComponent* InComponent, UBodySetup* InBodySetup, int32 InBoneIndex, EAggCollisionShape::Type InShapeType, int32 InShapeIndex)
{
	char debugMsg[256];
	sprintf_s(debugMsg, "[Gizmo] SetShapeTarget: InComponent=%p, InBodySetup=%p, BoneIndex=%d, ShapeType=%d, ShapeIndex=%d\n",
		(void*)InComponent, (void*)InBodySetup, InBoneIndex, (int)InShapeType, InShapeIndex);
	OutputDebugStringA(debugMsg);

	if (!InComponent || !InBodySetup || InBoneIndex < 0 || InShapeIndex < 0)
	{
		OutputDebugStringA("[Gizmo] SetShapeTarget: Validation failed, clearing target\n");
		ClearShapeTarget();
		return;
	}

	TargetType = EGizmoTargetType::Shape;
	TargetSkeletalMeshComponent = InComponent;
	TargetBodySetup = InBodySetup;
	TargetBoneIndex = InBoneIndex;
	TargetShapeType = InShapeType;
	TargetShapeIndex = InShapeIndex;

	// 본의 월드 트랜스폼
	FTransform BoneWorldTransform = InComponent->GetBoneWorldTransform(InBoneIndex);

	// Shape의 로컬 Center 가져오기
	FVector ShapeLocalCenter = FVector::Zero();
	FVector ShapeLocalRotation = FVector::Zero();

	switch (InShapeType)
	{
	case EAggCollisionShape::Sphere:
		if (InShapeIndex < InBodySetup->AggGeom.SphereElems.Num())
		{
			ShapeLocalCenter = InBodySetup->AggGeom.SphereElems[InShapeIndex].Center;
		}
		break;
	case EAggCollisionShape::Box:
		if (InShapeIndex < InBodySetup->AggGeom.BoxElems.Num())
		{
			ShapeLocalCenter = InBodySetup->AggGeom.BoxElems[InShapeIndex].Center;
			ShapeLocalRotation = InBodySetup->AggGeom.BoxElems[InShapeIndex].Rotation;
		}
		break;
	case EAggCollisionShape::Capsule:
		if (InShapeIndex < InBodySetup->AggGeom.SphylElems.Num())
		{
			ShapeLocalCenter = InBodySetup->AggGeom.SphylElems[InShapeIndex].Center;
			ShapeLocalRotation = InBodySetup->AggGeom.SphylElems[InShapeIndex].Rotation;
		}
		break;
	default:
		break;
	}

	// Shape Center를 월드 좌표로 변환
	FVector ShapeWorldPos = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(ShapeLocalCenter);

	// Shape 로컬 회전을 월드 회전으로 변환
	FQuat ShapeLocalQuat = FQuat::MakeFromEulerZYX(ShapeLocalRotation);
	FQuat ShapeWorldRot = BoneWorldTransform.Rotation * ShapeLocalQuat;

	RootComponent->SetWorldLocation(ShapeWorldPos);
	RootComponent->SetWorldRotation(ShapeWorldRot);

	sprintf_s(debugMsg, "[Gizmo] SetShapeTarget: BonePos=(%.2f,%.2f,%.2f), ShapeLocalCenter=(%.2f,%.2f,%.2f), ShapeWorldPos=(%.2f,%.2f,%.2f)\n",
		BoneWorldTransform.Translation.X, BoneWorldTransform.Translation.Y, BoneWorldTransform.Translation.Z,
		ShapeLocalCenter.X, ShapeLocalCenter.Y, ShapeLocalCenter.Z,
		ShapeWorldPos.X, ShapeWorldPos.Y, ShapeWorldPos.Z);
	OutputDebugStringA(debugMsg);

	sprintf_s(debugMsg, "[Gizmo] SetShapeTarget complete: TargetType=%d, TargetBoneIndex=%d, TargetShapeIndex=%d\n",
		(int)TargetType, TargetBoneIndex, TargetShapeIndex);
	OutputDebugStringA(debugMsg);

	SetSpaceWorldMatrix(CurrentSpace, nullptr);
}

void AGizmoActor::ClearShapeTarget()
{
	TargetType = EGizmoTargetType::Actor;
	TargetSkeletalMeshComponent = nullptr;
	TargetBodySetup = nullptr;
	TargetBoneIndex = -1;
	TargetShapeType = static_cast<EAggCollisionShape::Type>(0);
	TargetShapeIndex = -1;
}

// ────────────────────────────────────────────────────────
// 평면 기즈모 렌더링
// ────────────────────────────────────────────────────────

void AGizmoActor::RenderTranslatePlanes(const FVector& GizmoLocation, const FQuat& BaseRot, float RenderScale, URenderer* Renderer)
{
	const float CornerPos = 0.3f * RenderScale;
	const float HandleRadius = 0.02f * RenderScale;
	constexpr int32 NumSegments = 8;

	// 기즈모 축 색상 (X=빨강, Y=초록, Z=파랑)
	const FVector4 GizmoColor[3] = {
		FVector4(1.0f, 0.0f, 0.0f, 1.0f),  // X: 빨강
		FVector4(0.0f, 1.0f, 0.0f, 1.0f),  // Y: 초록
		FVector4(0.0f, 0.0f, 1.0f, 1.0f)   // Z: 파랑
	};

	// 평면 정보 구조체
	struct FPlaneInfo
	{
		EGizmoDirection Direction;
		FVector Tangent1;
		FVector Tangent2;
	};

	FPlaneInfo Planes[3] = {
		{EGizmoDirection::XY_Plane, {1, 0, 0}, {0, 1, 0}},
		{EGizmoDirection::XZ_Plane, {1, 0, 0}, {0, 0, 1}},
		{EGizmoDirection::YZ_Plane, {0, 1, 0}, {0, 0, 1}}
	};

	FGizmoBatchRenderer Batch;

	for (const FPlaneInfo& PlaneInfo : Planes)
	{
		FVector T1 = PlaneInfo.Tangent1;
		FVector T2 = PlaneInfo.Tangent2;
		FVector PlaneNormal = FVector::Cross(T1, T2);
		PlaneNormal = PlaneNormal.GetNormalized();

		// 각 평면의 두 선분에 사용할 색상 인덱스
		EGizmoDirection Seg1Color, Seg2Color;
		if (PlaneInfo.Direction == EGizmoDirection::XY_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;  // X
			Seg2Color = EGizmoDirection::Right;    // Y
		}
		else if (PlaneInfo.Direction == EGizmoDirection::XZ_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;  // X
			Seg2Color = EGizmoDirection::Up;       // Z
		}
		else  // YZ_Plane
		{
			Seg1Color = EGizmoDirection::Right;    // Y
			Seg2Color = EGizmoDirection::Up;       // Z
		}

		// 평면이 선택되었는지 확인
		bool bIsPlaneSelected = (static_cast<uint32>(PlaneInfo.Direction) == GizmoAxis);

		// 색상 계산 (비트 플래그 값에서 인덱스로 변환)
		auto DirectionToAxisIndex = [](EGizmoDirection Dir) -> int32
		{
			switch (Dir)
			{
			case EGizmoDirection::Forward: return 0;  // X (비트 1)
			case EGizmoDirection::Right:   return 1;  // Y (비트 2)
			case EGizmoDirection::Up:      return 2;  // Z (비트 4)
			default:                       return 0;
			}
		};

		FVector4 Seg1ColorFinal, Seg2ColorFinal;
		if (bIsPlaneSelected && bIsDragging)
		{
			Seg1ColorFinal = Seg2ColorFinal = FVector4(0.8f, 0.8f, 0.0f, 1.0f);  // 드래그 중: 짙은 노란색
		}
		else if (bIsPlaneSelected)
		{
			Seg1ColorFinal = Seg2ColorFinal = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 호버 중: 밝은 노란색
		}
		else
		{
			Seg1ColorFinal = GizmoColor[DirectionToAxisIndex(Seg1Color)];
			Seg2ColorFinal = GizmoColor[DirectionToAxisIndex(Seg2Color)];
		}

		// 선분 1 메쉬 생성 (T1 방향 선분)
		{
			TArray<FNormalVertex> Vertices;
			TArray<uint32> Indices;

			FVector Start1 = T1 * CornerPos;
			FVector End1 = T1 * CornerPos + T2 * CornerPos;
			FVector Dir1 = End1 - Start1;
			Dir1 = Dir1.GetNormalized();
			FVector Perp1_1 = FVector::Cross(Dir1, PlaneNormal);
			Perp1_1 = Perp1_1.GetNormalized();
			FVector Perp1_2 = FVector::Cross(Dir1, Perp1_1);
			Perp1_2 = Perp1_2.GetNormalized();

			for (int32 i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1_1 * std::cosf(Angle) + Perp1_2 * std::sinf(Angle)) * HandleRadius;

				FNormalVertex Vtx1, Vtx2;
				Vtx1.pos = Start1 + Offset;
				Vtx1.normal = Offset.GetNormalized();
				Vtx1.color = Seg1ColorFinal;
				Vtx1.tex = FVector2D(0, 0);
				Vtx1.Tangent = FVector4(1, 0, 0, 1);

				Vtx2.pos = End1 + Offset;
				Vtx2.normal = Offset.GetNormalized();
				Vtx2.color = Seg1ColorFinal;
				Vtx2.tex = FVector2D(0, 0);
				Vtx2.Tangent = FVector4(1, 0, 0, 1);

				Vertices.Add(Vtx1);
				Vertices.Add(Vtx2);
			}

			for (int32 i = 0; i < NumSegments; ++i)
			{
				int32 Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(Vertices, Indices, Seg1ColorFinal, GizmoLocation, BaseRot);
		}

		// 선분 2 메쉬 생성 (T2 방향 선분)
		{
			TArray<FNormalVertex> Vertices;
			TArray<uint32> Indices;

			FVector Start2 = T2 * CornerPos;
			FVector End2 = T1 * CornerPos + T2 * CornerPos;
			FVector Dir2 = End2 - Start2;
			Dir2 = Dir2.GetNormalized();
			FVector Perp2_1 = FVector::Cross(Dir2, PlaneNormal);
			Perp2_1 = Perp2_1.GetNormalized();
			FVector Perp2_2 = FVector::Cross(Dir2, Perp2_1);
			Perp2_2 = Perp2_2.GetNormalized();

			for (int32 i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp2_1 * std::cosf(Angle) + Perp2_2 * std::sinf(Angle)) * HandleRadius;

				FNormalVertex Vtx1, Vtx2;
				Vtx1.pos = Start2 + Offset;
				Vtx1.normal = Offset.GetNormalized();
				Vtx1.color = Seg2ColorFinal;
				Vtx1.tex = FVector2D(0, 0);
				Vtx1.Tangent = FVector4(1, 0, 0, 1);

				Vtx2.pos = End2 + Offset;
				Vtx2.normal = Offset.GetNormalized();
				Vtx2.color = Seg2ColorFinal;
				Vtx2.tex = FVector2D(0, 0);
				Vtx2.Tangent = FVector4(1, 0, 0, 1);

				Vertices.Add(Vtx1);
				Vertices.Add(Vtx2);
			}

			for (int32 i = 0; i < NumSegments; ++i)
			{
				int32 Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(Vertices, Indices, Seg2ColorFinal, GizmoLocation, BaseRot);
		}
	}

	// 모든 메쉬를 일괄 렌더링 (TODO: FlushAndRender 구현 필요)
	Batch.FlushAndRender(Renderer);
}

void AGizmoActor::RenderScalePlanes(const FVector& GizmoLocation, const FQuat& BaseRot, float RenderScale, URenderer* Renderer)
{
	const float MidPoint = 0.4f * RenderScale;
	const float HandleRadius = 0.02f * RenderScale;
	constexpr int32 NumSegments = 8;

	// 기즈모 축 색상 (X=빨강, Y=초록, Z=파랑)
	const FVector4 GizmoColor[3] = {
		FVector4(1.0f, 0.0f, 0.0f, 1.0f),  // X: 빨강
		FVector4(0.0f, 1.0f, 0.0f, 1.0f),  // Y: 초록
		FVector4(0.0f, 0.0f, 1.0f, 1.0f)   // Z: 파랑
	};

	// 평면 정보 구조체
	struct FPlaneInfo
	{
		EGizmoDirection Direction;
		FVector Tangent1;
		FVector Tangent2;
	};

	FPlaneInfo Planes[3] = {
		{EGizmoDirection::XY_Plane, {1, 0, 0}, {0, 1, 0}},
		{EGizmoDirection::XZ_Plane, {1, 0, 0}, {0, 0, 1}},
		{EGizmoDirection::YZ_Plane, {0, 1, 0}, {0, 0, 1}}
	};

	FGizmoBatchRenderer Batch;

	for (const FPlaneInfo& PlaneInfo : Planes)
	{
		FVector T1 = PlaneInfo.Tangent1;
		FVector T2 = PlaneInfo.Tangent2;

		FVector Point1 = T1 * MidPoint;
		FVector Point2 = T2 * MidPoint;
		FVector MidCenter = (Point1 + Point2) * 0.5f;

		FVector PlaneNormal = FVector::Cross(T1, T2);
		PlaneNormal = PlaneNormal.GetNormalized();

		// 각 평면의 두 선분에 사용할 색상 인덱스
		EGizmoDirection Seg1Color, Seg2Color;
		if (PlaneInfo.Direction == EGizmoDirection::XY_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;  // X
			Seg2Color = EGizmoDirection::Right;    // Y
		}
		else if (PlaneInfo.Direction == EGizmoDirection::XZ_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;  // X
			Seg2Color = EGizmoDirection::Up;       // Z
		}
		else  // YZ_Plane
		{
			Seg1Color = EGizmoDirection::Right;    // Y
			Seg2Color = EGizmoDirection::Up;       // Z
		}

		// 평면이 선택되었는지 확인 (평면 자체 또는 Center 선택 시 하이라이팅)
		bool bIsPlaneSelected = (static_cast<uint32>(PlaneInfo.Direction) == GizmoAxis);
		bool bIsCenterSelected = (static_cast<uint32>(EGizmoDirection::Center) == GizmoAxis);

		// 색상 계산 (비트 플래그 값에서 인덱스로 변환)
		auto DirectionToAxisIndex = [](EGizmoDirection Dir) -> int32
		{
			switch (Dir)
			{
			case EGizmoDirection::Forward: return 0;  // X (비트 1)
			case EGizmoDirection::Right:   return 1;  // Y (비트 2)
			case EGizmoDirection::Up:      return 2;  // Z (비트 4)
			default:                       return 0;
			}
		};

		FVector4 Seg1ColorFinal, Seg2ColorFinal;
		if ((bIsPlaneSelected || bIsCenterSelected) && bIsDragging)
		{
			Seg1ColorFinal = Seg2ColorFinal = FVector4(0.8f, 0.8f, 0.0f, 1.0f);  // 드래그 중: 짙은 노란색
		}
		else if (bIsPlaneSelected || bIsCenterSelected)
		{
			Seg1ColorFinal = Seg2ColorFinal = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 호버 중: 밝은 노란색
		}
		else
		{
			Seg1ColorFinal = GizmoColor[DirectionToAxisIndex(Seg1Color)];
			Seg2ColorFinal = GizmoColor[DirectionToAxisIndex(Seg2Color)];
		}

		// 선분 1 메쉬 생성 (Point1 → MidCenter 대각선)
		{
			TArray<FNormalVertex> Vertices;
			TArray<uint32> Indices;

			FVector Start = Point1;
			FVector End = MidCenter;
			FVector DiagDir = End - Start;
			DiagDir = DiagDir.GetNormalized();
			FVector Perp1 = FVector::Cross(DiagDir, PlaneNormal);
			Perp1 = Perp1.GetNormalized();
			FVector Perp2 = FVector::Cross(DiagDir, Perp1);
			Perp2 = Perp2.GetNormalized();

			for (int32 i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1 * std::cosf(Angle) + Perp2 * std::sinf(Angle)) * HandleRadius;

				FNormalVertex Vtx1, Vtx2;
				Vtx1.pos = Start + Offset;
				Vtx1.normal = Offset.GetNormalized();
				Vtx1.color = Seg1ColorFinal;
				Vtx1.tex = FVector2D(0, 0);
				Vtx1.Tangent = FVector4(1, 0, 0, 1);

				Vtx2.pos = End + Offset;
				Vtx2.normal = Offset.GetNormalized();
				Vtx2.color = Seg1ColorFinal;
				Vtx2.tex = FVector2D(0, 0);
				Vtx2.Tangent = FVector4(1, 0, 0, 1);

				Vertices.Add(Vtx1);
				Vertices.Add(Vtx2);
			}

			for (int32 i = 0; i < NumSegments; ++i)
			{
				int32 Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(Vertices, Indices, Seg1ColorFinal, GizmoLocation, BaseRot);
		}

		// 선분 2 메쉬 생성 (MidCenter → Point2 대각선)
		{
			TArray<FNormalVertex> Vertices;
			TArray<uint32> Indices;

			FVector Start = MidCenter;
			FVector End = Point2;
			FVector DiagDir = End - Start;
			DiagDir = DiagDir.GetNormalized();
			FVector Perp1 = FVector::Cross(DiagDir, PlaneNormal);
			Perp1 = Perp1.GetNormalized();
			FVector Perp2 = FVector::Cross(DiagDir, Perp1);
			Perp2 = Perp2.GetNormalized();

			for (int32 i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1 * std::cosf(Angle) + Perp2 * std::sinf(Angle)) * HandleRadius;

				FNormalVertex Vtx1, Vtx2;
				Vtx1.pos = Start + Offset;
				Vtx1.normal = Offset.GetNormalized();
				Vtx1.color = Seg2ColorFinal;
				Vtx1.tex = FVector2D(0, 0);
				Vtx1.Tangent = FVector4(1, 0, 0, 1);

				Vtx2.pos = End + Offset;
				Vtx2.normal = Offset.GetNormalized();
				Vtx2.color = Seg2ColorFinal;
				Vtx2.tex = FVector2D(0, 0);
				Vtx2.Tangent = FVector4(1, 0, 0, 1);

				Vertices.Add(Vtx1);
				Vertices.Add(Vtx2);
			}

			for (int32 i = 0; i < NumSegments; ++i)
			{
				int32 Next = (i + 1) % NumSegments;
				Indices.Add(i * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 0);
				Indices.Add(Next * 2 + 0);
				Indices.Add(i * 2 + 1);
				Indices.Add(Next * 2 + 1);
			}

			Batch.AddMesh(Vertices, Indices, Seg2ColorFinal, GizmoLocation, BaseRot);
		}
	}

	// 모든 메쉬를 일괄 렌더링 (TODO: FlushAndRender 구현 필요)
	Batch.FlushAndRender(Renderer);
}

void AGizmoActor::RenderCenterSphere(const FVector& GizmoLocation, float RenderScale, URenderer* Renderer)
{
	const float SphereRadius = 0.08f * RenderScale;  // 평면(0.3)보다 작지만 적당한 크기
	constexpr int32 NumSegments = 16;
	constexpr int32 NumRings = 8;

	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	// UV Sphere 생성
	for (int32 Ring = 0; Ring <= NumRings; ++Ring)
	{
		float Phi = static_cast<float>(Ring) / NumRings * PI;
		float SinPhi = std::sinf(Phi);
		float CosPhi = std::cosf(Phi);

		for (int32 Seg = 0; Seg <= NumSegments; ++Seg)
		{
			float Theta = static_cast<float>(Seg) / NumSegments * 2.0f * PI;
			float SinTheta = std::sinf(Theta);
			float CosTheta = std::cosf(Theta);

			FVector Pos(SinPhi * CosTheta, SinPhi * SinTheta, CosPhi);
			FVector Normal = Pos;
			Normal = Normal.GetNormalized();
			Pos = Pos * SphereRadius;

			FNormalVertex Vtx;
			Vtx.pos = Pos;
			Vtx.normal = Normal;
			Vtx.color = FVector4(1, 1, 1, 1);  // 기본 색상은 흰색 (나중에 하이라이팅 로직 추가 예정)
			Vtx.tex = FVector2D(0, 0);
			Vtx.Tangent = FVector4(1, 0, 0, 1);

			Vertices.Add(Vtx);
		}
	}

	// 인덱스 생성
	for (int32 Ring = 0; Ring < NumRings; ++Ring)
	{
		for (int32 Seg = 0; Seg < NumSegments; ++Seg)
		{
			int32 Current = Ring * (NumSegments + 1) + Seg;
			int32 Next = Current + NumSegments + 1;

			Indices.Add(Current);
			Indices.Add(Next);
			Indices.Add(Current + 1);

			Indices.Add(Current + 1);
			Indices.Add(Next);
			Indices.Add(Next + 1);
		}
	}

	// 중심 구체 색상: 흰색 또는 하이라이트
	bool bIsCenterSelected = (static_cast<uint32>(EGizmoDirection::Center) == GizmoAxis);
	FVector4 SphereColor;
	if (bIsCenterSelected && bIsDragging)
	{
		SphereColor = FVector4(0.8f, 0.8f, 0.0f, 1.0f);  // 드래그 중: 짙은 노란색
	}
	else if (bIsCenterSelected)
	{
		SphereColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 호버 중: 밝은 노란색
	}
	else
	{
		SphereColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);  // 기본: 흰색
	}

	// 배칭에 추가 및 즉시 렌더링
	FGizmoBatchRenderer Batch;
	Batch.AddMesh(Vertices, Indices, SphereColor, GizmoLocation);
	Batch.FlushAndRender(Renderer);
}

void AGizmoActor::RenderRotationCircles(const FVector& GizmoLocation, const FQuat& BaseRot, const FVector4& AxisColor,
                                         const FVector& BaseAxis0, const FVector& BaseAxis1, float RenderScale, URenderer* Renderer)
{
	// Rotation 링 충돌 설정
	const float InnerRadius = 0.75f * RenderScale;
	const float OuterRadius = 0.85f * RenderScale;
	const float Thickness = 0.05f * RenderScale;  // 링 두께 증가 (0.02 → 0.05)
	const float SnapAngleDegrees = 10.0f;  // 기본 스냅 각도

	// BaseRot으로 회전된 월드 축
	FVector RenderWorldAxis0 = BaseRot.RotateVector(BaseAxis0);
	FVector RenderWorldAxis1 = BaseRot.RotateVector(BaseAxis1);

	FGizmoBatchRenderer Batch;

	// 1. Inner circle: 두꺼운 축 색상 선
	{
		TArray<FNormalVertex> InnerVertices;
		TArray<uint32> InnerIndices;

		const float InnerLineThickness = Thickness * 2.0f;
		FGizmoGeometry::GenerateCircleLineMesh(RenderWorldAxis0, RenderWorldAxis1,
		                                       InnerRadius, InnerLineThickness, InnerVertices, InnerIndices);

		if (!InnerIndices.IsEmpty())
		{
			Batch.AddMesh(InnerVertices, InnerIndices, AxisColor, GizmoLocation);
		}
	}

	// 2. Outer circle: 얇은 노란색 선
	{
		TArray<FNormalVertex> OuterVertices;
		TArray<uint32> OuterIndices;

		const float OuterLineThickness = Thickness * 1.0f;
		FGizmoGeometry::GenerateCircleLineMesh(RenderWorldAxis0, RenderWorldAxis1,
		                                       OuterRadius, OuterLineThickness, OuterVertices, OuterIndices);

		if (!OuterIndices.IsEmpty())
		{
			FVector4 YellowColor(1.0f, 1.0f, 0.0f, 1.0f);
			Batch.AddMesh(OuterVertices, OuterIndices, YellowColor, GizmoLocation);
		}
	}

	// 3. 각도 눈금 렌더링 (스냅 각도마다 작은 눈금, 90도마다 큰 눈금)
	{
		TArray<FNormalVertex> TickVertices;
		TArray<uint32> TickIndices;

		FGizmoGeometry::GenerateAngleTickMarks(RenderWorldAxis0, RenderWorldAxis1,
		                                        InnerRadius, OuterRadius, Thickness, SnapAngleDegrees,
		                                        TickVertices, TickIndices);

		if (!TickIndices.IsEmpty())
		{
			FVector4 YellowColor(1.0f, 1.0f, 0.0f, 1.0f);
			Batch.AddMesh(TickVertices, TickIndices, YellowColor, GizmoLocation);
		}
	}

	// 4. 회전 각도 Arc 렌더링 (현재 회전 각도 표시)
	float DisplayAngle = CurrentRotationAngle;

	// X축과 Y축은 각도 반전
	const bool bIsXAxis = (std::abs(BaseAxis0.X) < 0.1f && std::abs(BaseAxis0.Y) < 0.1f && std::abs(BaseAxis0.Z) > 0.9f &&
	                       std::abs(BaseAxis1.X) < 0.1f && std::abs(BaseAxis1.Y) > 0.9f && std::abs(BaseAxis1.Z) < 0.1f);
	const bool bIsYAxis = (std::abs(BaseAxis0.X) > 0.9f && std::abs(BaseAxis0.Y) < 0.1f && std::abs(BaseAxis0.Z) < 0.1f &&
	                       std::abs(BaseAxis1.X) < 0.1f && std::abs(BaseAxis1.Y) < 0.1f && std::abs(BaseAxis1.Z) > 0.9f);
	if (bIsXAxis || bIsYAxis)
	{
		DisplayAngle = -DisplayAngle;
	}

	if (std::abs(DisplayAngle) > 0.001f)
	{
		TArray<FNormalVertex> ArcVertices;
		TArray<uint32> ArcIndices;

		// Arc를 약간 더 두껍게 (링보다 살짝 크게)
		const float ArcInnerRadius = InnerRadius * 0.98f;
		const float ArcOuterRadius = OuterRadius * 1.02f;

		// Arc 시작 방향: World 모드는 (0,0,0), Local 모드는 BaseAxis0
		FVector StartDir = (CurrentSpace == EGizmoSpace::World) ? FVector(0, 0, 0) : BaseRot.RotateVector(BaseAxis0);

		FGizmoGeometry::GenerateRotationArcMesh(RenderWorldAxis0, RenderWorldAxis1,
		                                         ArcInnerRadius, ArcOuterRadius, Thickness,
		                                         DisplayAngle, StartDir, ArcVertices, ArcIndices);

		if (!ArcIndices.IsEmpty())
		{
			FVector4 WhiteColor(1.0f, 1.0f, 1.0f, 1.0f);  // 흰색 아크 게이지
			Batch.AddMesh(ArcVertices, ArcIndices, WhiteColor, GizmoLocation);
		}
	}

	// 모든 메쉬를 일괄 렌더링
	Batch.FlushAndRender(Renderer);
}

// ────────────────────────────────────────────────────────
// 확장 기즈모 렌더링 (평면, 구체, Rotation 시각화)
// ────────────────────────────────────────────────────────
void AGizmoActor::RenderGizmoExtensions(URenderer* Renderer, ACameraActor* Camera)
{
	if (!Renderer || !Camera || !bRender)
	{
		return;
	}

	// 기즈모 위치 계산
	FVector GizmoLocation = GetActorLocation();

	// Screen-constant scale 계산 (ViewZ 기반)
	const FMatrix& ViewMatrix = Camera->GetViewMatrix();
	const FMatrix& ProjectionMatrix = Camera->GetProjectionMatrix();
	const uint32 ViewportWidth = Renderer->GetCurrentViewportWidth();
	const uint32 ViewportHeight = Renderer->GetCurrentViewportHeight();

	FVector CameraPos = Camera->GetActorLocation();
	FVector CameraForward = Camera->GetForward();
	FVector ToGizmo = GizmoLocation - CameraPos;
	float ViewZ = FVector::Dot(ToGizmo, CameraForward);

	if (ViewZ <= 0.0f)
	{
		return; // 카메라 뒤에 있음
	}

	// FOV 보정 (ProjectionMatrix[1][1] = cot(FOV_Y/2))
	float ProjYY = ProjectionMatrix.M[1][1];
	constexpr float TargetPixels = 128.0f; // 기준 스크린 크기 (픽셀)
	float RenderScale = (TargetPixels * ViewZ) / (ProjYY * ViewportHeight * 0.5f);

	// 기본 회전 (World/Local 공간)
	// SetSpaceWorldMatrix()에서 GizmoActor 자체의 회전을 이미 설정했으므로,
	// 모든 타겟 타입(Actor/Component/Bone/Shape/Constraint)에 대해 일관되게 동작
	FQuat BaseRot = GetActorRotation();

	// Rotate 모드 드래그 중에는 시작 시점의 회전에 고정 (기즈모가 같이 회전하지 않도록)
	// 단, World 모드일 때는 Identity 유지
	if (bIsDragging && CurrentMode == EGizmoMode::Rotate)
	{
		if (CurrentSpace == EGizmoSpace::World)
		{
			BaseRot = FQuat::Identity();  // World 모드: 월드 축 정렬 유지
		}
		else
		{
			BaseRot = DragStartRotation;  // Local 모드: 드래그 시작 회전 고정
		}
	}

	// ═══════════════════════════════════════════════════════
	// 평면 기즈모 렌더링 (하이라이팅되지 않은 경우 먼저 렌더링)
	// ═══════════════════════════════════════════════════════
	bool bAnyPlaneHighlighted = false; // TODO: 피킹 시스템 구현 후 실제 하이라이팅 체크

	if (!bAnyPlaneHighlighted)
	{
		if (CurrentMode == EGizmoMode::Translate)
		{
			RenderTranslatePlanes(GizmoLocation, BaseRot, RenderScale, Renderer);
		}
		else if (CurrentMode == EGizmoMode::Scale)
		{
			RenderScalePlanes(GizmoLocation, BaseRot, RenderScale, Renderer);
		}
	}

	// ═══════════════════════════════════════════════════════
	// Rotation 기즈모 렌더링
	// ═══════════════════════════════════════════════════════
	if (CurrentMode == EGizmoMode::Rotate)
	{
		// 유휴 상태: QuarterRing 렌더링 (카메라 플립 판정 포함)
		if (!bIsDragging)
		{
			// BaseAxis 정의
			const FVector BaseAxis0[3] = {
				FVector(0, 0, 1),  // X축: Z→Y
				FVector(1, 0, 0),  // Y축: X→Z
				FVector(1, 0, 0)   // Z축: X→Y
			};
			const FVector BaseAxis1[3] = {
				FVector(0, 1, 0),  // X축: Y
				FVector(0, 0, 1),  // Y축: Z
				FVector(0, 1, 0)   // Z축: Y
			};

			// X축 QuarterRing (빨강)
			RenderRotationQuarterRing(GizmoLocation, BaseRot, 1,
			                           BaseAxis0[0], BaseAxis1[0], RenderScale, Renderer, Camera);

			// Y축 QuarterRing (초록)
			RenderRotationQuarterRing(GizmoLocation, BaseRot, 2,
			                           BaseAxis0[1], BaseAxis1[1], RenderScale, Renderer, Camera);

			// Z축 QuarterRing (파랑)
			RenderRotationQuarterRing(GizmoLocation, BaseRot, 4,
			                           BaseAxis0[2], BaseAxis1[2], RenderScale, Renderer, Camera);
		}
	}

	// 드래그 중: 360도 링 + 각도 표시
	if (CurrentMode == EGizmoMode::Rotate && bIsDragging && DraggingAxis != 0)
	{
		FVector AxisX(1, 0, 0), AxisY(0, 1, 0), AxisZ(0, 0, 1);
		FVector LocalAxis0, LocalAxis1;  // 로컬 축 (RenderRotationCircles에서 BaseRot로 회전)
		FVector4 AxisColor;

		if (DraggingAxis == 1) // X축
		{
			LocalAxis0 = AxisZ;  // (0,0,1) - QuarterRing BaseAxis0[0]과 일치
			LocalAxis1 = AxisY;  // (0,1,0) - QuarterRing BaseAxis1[0]과 일치
			AxisColor = FVector4(1, 0, 0, 1);
		}
		else if (DraggingAxis == 2) // Y축
		{
			LocalAxis0 = AxisX;  // (1,0,0) - QuarterRing BaseAxis0[1]과 일치
			LocalAxis1 = AxisZ;  // (0,0,1) - QuarterRing BaseAxis1[1]과 일치
			AxisColor = FVector4(0, 1, 0, 1);
		}
		else if (DraggingAxis == 4) // Z축
		{
			LocalAxis0 = AxisX;  // (1,0,0) - QuarterRing BaseAxis0[2]와 일치
			LocalAxis1 = AxisY;  // (0,1,0) - QuarterRing BaseAxis1[2]와 일치
			AxisColor = FVector4(0, 0, 1, 1);
		}
		else
		{
			return;
		}

		RenderRotationCircles(GizmoLocation, BaseRot, AxisColor, LocalAxis0, LocalAxis1, RenderScale, Renderer);

	}

	// ═══════════════════════════════════════════════════════
	// 중심 구체 렌더링
	// ═══════════════════════════════════════════════════════
	if (CurrentMode == EGizmoMode::Translate || CurrentMode == EGizmoMode::Scale)
	{
		RenderCenterSphere(GizmoLocation, RenderScale, Renderer);
	}

	// ═══════════════════════════════════════════════════════
	// 평면 기즈모가 하이라이팅된 경우 마지막에 렌더링 (축 위에 보이도록)
	// ═══════════════════════════════════════════════════════
	if (bAnyPlaneHighlighted)
	{
		if (CurrentMode == EGizmoMode::Translate)
		{
			RenderTranslatePlanes(GizmoLocation, BaseRot, RenderScale, Renderer);
		}
		else if (CurrentMode == EGizmoMode::Scale)
		{
			RenderScalePlanes(GizmoLocation, BaseRot, RenderScale, Renderer);
		}
	}
}

// ────────────────────────────────────────────────────────
// Rotation QuarterRing 렌더링 (비드래그 시 기본 시각화)
// ────────────────────────────────────────────────────────
void AGizmoActor::RenderRotationQuarterRing(const FVector& GizmoLocation, const FQuat& BaseRot, uint32 Direction,
                                             const FVector& BaseAxis0, const FVector& BaseAxis1, float RenderScale,
                                             URenderer* Renderer, ACameraActor* Camera)
{
	if (!Renderer || !Camera)
	{
		return;
	}

	// Rotation 링 충돌 설정
	const float InnerRadius = 0.75f * RenderScale;
	const float OuterRadius = 0.85f * RenderScale;
	const float Thickness = 0.05f * RenderScale;

	// 카메라 정보
	const FVector CameraPos = Camera->GetActorLocation();
	const FVector DirectionToWidget = (GizmoLocation - CameraPos).GetNormalized();

	// 월드 공간 축 계산
	FVector WorldAxis0 = BaseRot.RotateVector(BaseAxis0);
	FVector WorldAxis1 = BaseRot.RotateVector(BaseAxis1);

	// 플립 판정 (Unreal 표준)
	const bool bMirrorAxis0 = (FVector::Dot(WorldAxis0, DirectionToWidget) <= 0.0f);
	const bool bMirrorAxis1 = (FVector::Dot(WorldAxis1, DirectionToWidget) <= 0.0f);

	const FVector RenderWorldAxis0 = bMirrorAxis0 ? WorldAxis0 : -WorldAxis0;
	const FVector RenderWorldAxis1 = bMirrorAxis1 ? WorldAxis1 : -WorldAxis1;

	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	FGizmoGeometry::GenerateQuarterRingMesh(RenderWorldAxis0, RenderWorldAxis1,
	                                         InnerRadius, OuterRadius, Thickness,
	                                         Vertices, Indices);

	// 색상 결정 (Direction에 따라: Picking 시스템 매칭)
	FVector4 RingColor(1, 0, 0, 1);  // 기본 빨강
	if (Direction == 1)  // X축
	{
		RingColor = FVector4(1, 0, 0, 1);  // 빨강
	}
	else if (Direction == 2)  // Y축
	{
		RingColor = FVector4(0, 1, 0, 1);  // 초록
	}
	else if (Direction == 4)  // Z축
	{
		RingColor = FVector4(0, 0, 1, 1);  // 파랑
	}

	// 하이라이팅: 선택된 축만 노란색으로 표시
	// 호버링이 없을 때는 기본 색상 유지
	bool bIsHighlighted = (GizmoAxis != 0 && GizmoAxis == Direction);
	if (bIsDragging && bIsHighlighted)
	{
		RingColor = FVector4(0.8f, 0.8f, 0.0f, 1.0f);  // 드래그 중: 짙은 노란색
	}
	else if (bIsHighlighted)
	{
		RingColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 호버 중: 밝은 노란색
	}

	// 배칭 렌더링
	FGizmoBatchRenderer Batch;
	Batch.AddMesh(Vertices, Indices, RingColor, GizmoLocation);
	Batch.FlushAndRender(Renderer);
}
