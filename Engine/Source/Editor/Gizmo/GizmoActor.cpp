#include "pch.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
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

IMPLEMENT_CLASS(AGizmoActor)

AGizmoActor::AGizmoActor()
{
	ObjectName = "Gizmo Actor";

	const float GizmoTotalSize = 1.5f;
	const float STGizmoTotalSize = 7.0f;    // Scale, Translation Gizmo

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

	RotateX->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	RotateY->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	RotateZ->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });

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
}

EGizmoMode AGizmoActor::GetMode()
{
	return CurrentMode;
}

void AGizmoActor::SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* SelectedComponent)
{
	SetSpace(NewSpace);

	// Bone 타겟인 경우
	if (TargetType == EGizmoTargetType::Bone && TargetSkeletalMeshComponent && TargetBoneIndex >= 0)
	{
		FTransform BoneWorldTransform = TargetSkeletalMeshComponent->GetBoneWorldTransform(TargetBoneIndex);

		if (NewSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
		{
			SetActorRotation(BoneWorldTransform.Rotation);
		}
		else if (NewSpace == EGizmoSpace::World)
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

		if (NewSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
		{
			SetActorRotation(ConstraintWorldRot);
		}
		else if (NewSpace == EGizmoSpace::World)
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

		if (NewSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
		{
			SetActorRotation(ShapeWorldRot);
		}
		else if (NewSpace == EGizmoSpace::World)
		{
			SetActorRotation(FQuat::Identity());
		}
		return;
	}

	// Actor/Component 타겟인 경우
	if (!SelectedComponent)
		return;

	if (NewSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
	{
		// 기즈모 액터 자체를 타겟의 회전으로 설정합니다.
		FQuat TargetRot = SelectedComponent->GetWorldRotation();
		SetActorRotation(TargetRot);
	}
	else if (NewSpace == EGizmoSpace::World)
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
	float Length = ScreenDirection.Length();
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
		// --- 드래그 시작 시점의 축 계산 ---
		FVector Axis{};
		if (CurrentSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
		{
			switch (DraggingAxis)
			{
			case 1: Axis = DragStartRotation.RotateVector(FVector(1, 0, 0)); break;
			case 2: Axis = DragStartRotation.RotateVector(FVector(0, 1, 0)); break;
			case 3: Axis = DragStartRotation.RotateVector(FVector(0, 0, 1)); break;
			}
		}
		else if (CurrentSpace == EGizmoSpace::World)
		{
			switch (DraggingAxis)
			{
			case 1: Axis = FVector(1, 0, 0); break;
			case 2: Axis = FVector(0, 1, 0); break;
			case 3: Axis = FVector(0, 0, 1); break;
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
			case 3: NewScale.Z += TotalMovement; break;
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
						case 3: Box.Z = FMath::Max(0.01f, DragStartShapeZ + TotalMovement); break;
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
						case 3:  // Z축 = Radius
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

		// 회전의 기준이 될 로컬 축 벡터
		FVector LocalAxisVector;
		switch (DraggingAxis)
		{
		case 1: LocalAxisVector = FVector(1, 0, 0); break;
		case 2: LocalAxisVector = FVector(0, 1, 0); break;
		case 3: LocalAxisVector = FVector(0, 0, 1); break;
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
			case 1: LocalAxisVector = FVector(1, 0, 0); break;
			case 2: LocalAxisVector = FVector(0, 1, 0); break;
			case 3: LocalAxisVector = FVector(0, 0, 1); break;
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

	// Q 키: Select 모드
	if (InputManager->IsKeyPressed('Q'))
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
	// Tab 키로 월드-로컬 모드 전환
	if (InputManager->IsKeyPressed(VK_TAB))
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
		case 1: LocalAxisVector = FVector(1, 0, 0); break;
		case 2: LocalAxisVector = FVector(0, 1, 0); break;
		case 3: LocalAxisVector = FVector(0, 0, 1); break;
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

	// 드래그 중일 때는 고정된 축(DraggingAxis)을, 아닐 때는 호버 축(GizmoAxis)을 사용
	uint32 HighlightAxis = bIsDragging ? DraggingAxis : GizmoAxis;

	// Arrow Components (Translate 모드)
	bool bShowArrows = bHasSelection && (CurrentMode == EGizmoMode::Translate);
	if (ArrowX) { ArrowX->SetActive(bShowArrows); ArrowX->SetHighlighted(HighlightAxis == 1, 1); }
	if (ArrowY) { ArrowY->SetActive(bShowArrows); ArrowY->SetHighlighted(HighlightAxis == 2, 2); }
	if (ArrowZ) { ArrowZ->SetActive(bShowArrows); ArrowZ->SetHighlighted(HighlightAxis == 3, 3); }

	// Rotate Components (Rotate 모드)
	bool bShowRotates = bHasSelection && (CurrentMode == EGizmoMode::Rotate);
	if (RotateX) { RotateX->SetActive(bShowRotates); RotateX->SetHighlighted(HighlightAxis == 1, 1); }
	if (RotateY) { RotateY->SetActive(bShowRotates); RotateY->SetHighlighted(HighlightAxis == 2, 2); }
	if (RotateZ) { RotateZ->SetActive(bShowRotates); RotateZ->SetHighlighted(HighlightAxis == 3, 3); }

	// Scale Components (Scale 모드)
	bool bShowScales = bHasSelection && (CurrentMode == EGizmoMode::Scale);
	if (ScaleX) { ScaleX->SetActive(bShowScales); ScaleX->SetHighlighted(HighlightAxis == 1, 1); }
	if (ScaleY) { ScaleY->SetActive(bShowScales); ScaleY->SetHighlighted(HighlightAxis == 2, 2); }
	if (ScaleZ) { ScaleZ->SetActive(bShowScales); ScaleZ->SetHighlighted(HighlightAxis == 3, 3); }
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
