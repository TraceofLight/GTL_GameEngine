#include "pch.h"
#include "FViewportClient.h"
#include "FViewport.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "World.h"
#include "Picking.h"
#include "SelectionManager.h"
#include "Gizmo/GizmoActor.h"
#include "RenderManager.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "PrimitiveComponent.h"
#include "Clipboard/ClipboardManager.h"
#include "InputManager.h"
#include "PlayerCameraManager.h"
#include "SceneView.h"
#include "PostProcessing/PostProcessing.h"
#include "ImGui/imgui.h"
#include <ctime>

FVector FViewportClient::CameraAddPosition{};
float FViewportClient::SharedOrthoZoom = 0.1f;

FViewportClient::FViewportClient()
{
	ViewportType = EViewportType::Perspective;
	// 직교 뷰별 기본 카메라 설정
	Camera = NewObject<ACameraActor>();
	SetupCameraMode();
}

FViewportClient::~FViewportClient()
{
}

void FViewportClient::Tick(float DeltaTime)
{
	if (PerspectiveCameraInput)
	{
		Camera->ProcessEditorCameraInput(DeltaTime);
	}
	MouseWheel(DeltaTime);
	static UClipboardManager* ClipboardManager = NewObject<UClipboardManager>();

	// 키보드 입력 처리 (Ctrl+C/V)
	if (World)
	{
		UInputManager& InputManager = UInputManager::GetInstance();
		USelectionManager* SelectionManager = World->GetSelectionManager();

		// Ctrl 키가 눌려있는지 확인
		bool bIsCtrlDown = InputManager.IsKeyDown(VK_CONTROL);

		// Ctrl + C: 복사
		if (bIsCtrlDown && InputManager.IsKeyPressed('C'))
		{
			if (SelectionManager)
			{
				// 선택된 Actor가 있으면 Actor 복사
				if (SelectionManager->IsActorMode() && SelectionManager->GetSelectedActor())
				{
					ClipboardManager->CopyActor(SelectionManager->GetSelectedActor());
				}
			}
		}
		// Ctrl + V: 붙여넣기
		else if (bIsCtrlDown && InputManager.IsKeyPressed('V'))
		{
			// Actor 붙여넣기
			if (ClipboardManager->HasCopiedActor())
			{
				// 오프셋 적용 (1미터씩 이동)
				FVector Offset(1.0f, 1.0f, 1.0f);
				AActor* NewActor = ClipboardManager->PasteActorToWorld(World, Offset);

				if (NewActor && SelectionManager)
				{
					// 새로 생성된 Actor 선택
					SelectionManager->ClearSelection();
					SelectionManager->SelectActor(NewActor);
				}
			}
		}
	}
}

void FViewportClient::Draw(FViewport* Viewport)
{
	if (!Viewport || !World)
	{
		return;
	}

	URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
	if (!Renderer)
	{
		return;
	}

	// PIE 중 렌더 호출
	if (World->bPie)
	{
		APlayerCameraManager* PlayerCameraManager = World->GetPlayerCameraManager();
		if (PlayerCameraManager)
		{
			PlayerCameraManager->CacheViewport(Viewport);	// 한프레임 지연 있고, 단일 뷰포트만 지원 (일단 이렇게 처리)

			// PIE 중에도 카메라가 없으면 에디터 카메라로 fallback 처리
			if (PlayerCameraManager->GetViewCamera())
			{
				FMinimalViewInfo* MinimalViewInfo = PlayerCameraManager->GetCurrentViewInfo();
				TArray<FPostProcessModifier> Modifiers = PlayerCameraManager->GetModifiers();

				FSceneView CurrentViewInfo(MinimalViewInfo, &World->GetRenderSettings());
				CurrentViewInfo.Modifiers = Modifiers;
				World->GetRenderSettings().SetViewMode(ViewMode);

				// 더 명확한 이름의 함수를 호출
				Renderer->RenderSceneForView(World, &CurrentViewInfo, Viewport);
				return;
			}
		}
	}

	// 1. 뷰 타입에 따라 카메라 설정 등 사전 작업을 먼저 수행
	switch (ViewportType)
	{
	case EViewportType::Perspective:
	{
		Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Perspective);
		break;
	}
	default: // 모든 Orthographic 케이스
	{
		Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
		SetupCameraMode();
		break;
	}
	}

	FSceneView RenderView(Camera->GetCameraComponent(), Viewport, &World->GetRenderSettings());

	// 에디터 DoF 적용 (ShowFlag 체크)
	if (EditorDoFSettings.bEnabled && HasShowFlag(World->GetRenderSettings().GetShowFlags(), EEngineShowFlags::SF_DepthOfField))
	{
		FPostProcessModifier DoFModifier;
		DoFModifier.Type = EPostProcessEffectType::DepthOfField;
		DoFModifier.Priority = 100;  // 다른 효과보다 나중에 적용
		DoFModifier.bEnabled = true;
		DoFModifier.Weight = 1.0f;
		DoFModifier.SourceObject = nullptr;
		// Params0: FocusDistance, FocusRange, NearBlurScale, FarBlurScale (Cinematic용)
		DoFModifier.Payload.Params0 = FVector4(
			EditorDoFSettings.FocusDistance,
			EditorDoFSettings.FocusRange,
			EditorDoFSettings.NearBlurScale,
			EditorDoFSettings.FarBlurScale
		);
		// Params1: MaxBlurRadius, BokehSize, DoFMode, PointFocusFalloff
		DoFModifier.Payload.Params1 = FVector4(
			EditorDoFSettings.MaxBlurRadius,
			EditorDoFSettings.BokehSize,
			static_cast<float>(EditorDoFSettings.DoFMode),
			EditorDoFSettings.PointFocusFalloff
		);
		// Params2: FocalLength, FNumber, SensorWidth, BlurMethod (Physical용 + 블러방식)
		DoFModifier.Payload.Params2 = FVector4(
			EditorDoFSettings.FocalLength,
			EditorDoFSettings.FNumber,
			EditorDoFSettings.SensorWidth,
			static_cast<float>(EditorDoFSettings.BlurMethod)
		);
		// Params3: FocusPoint.x, FocusPoint.y, FocusPoint.z, FocusRadius (PointFocus용)
		DoFModifier.Payload.Params3 = FVector4(
			EditorDoFSettings.FocusPoint.X,
			EditorDoFSettings.FocusPoint.Y,
			EditorDoFSettings.FocusPoint.Z,
			EditorDoFSettings.FocusRadius
		);
		// Color: TiltShiftCenterY, TiltShiftBandWidth, TiltShiftBlurScale, PointFocusBlurScale
		DoFModifier.Payload.Color = FLinearColor(
			EditorDoFSettings.TiltShiftCenterY,
			EditorDoFSettings.TiltShiftBandWidth,
			EditorDoFSettings.TiltShiftBlurScale,
			EditorDoFSettings.PointFocusBlurScale
		);
		// Params4: BleedingMethod (번짐 처리 방식)
		DoFModifier.Payload.Params4 = FVector4(
			static_cast<float>(EditorDoFSettings.BleedingMethod),
			0.0f, 0.0f, 0.0f  // 미래 확장용
		);
		RenderView.Modifiers.Add(DoFModifier);
	}

	// 2. 렌더링 호출은 뷰 타입 설정이 모두 끝난 후 마지막에 한 번만 수행
	World->GetRenderSettings().SetViewMode(ViewMode);

	// 더 명확한 이름의 함수를 호출
	Renderer->RenderSceneForView(World, &RenderView, Viewport);
}

void FViewportClient::SetupCameraMode()
{
	switch (ViewportType)
	{
	case EViewportType::Perspective:
		Camera->SetActorLocation(PerspectiveCameraPosition);
		Camera->SetRotationFromEulerAngles(PerspectiveCameraRotation);
		Camera->GetCameraComponent()->SetFOV(PerspectiveCameraFov);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 50000.0f);
		break;
	case EViewportType::Orthographic_Top:
		Camera->SetActorLocation({ CameraAddPosition.X, CameraAddPosition.Y, 1000 });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 90, 0 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 50000.0f);
		Camera->GetCameraComponent()->SetOrthoZoom(SharedOrthoZoom);
		break;
	case EViewportType::Orthographic_Bottom:
		Camera->SetActorLocation({ CameraAddPosition.X, CameraAddPosition.Y, -1000 });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, -90, 0 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 50000.0f);
		Camera->GetCameraComponent()->SetOrthoZoom(SharedOrthoZoom);
		break;
	case EViewportType::Orthographic_Left:
		Camera->SetActorLocation({ CameraAddPosition.X, 1000 , CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, -90 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 50000.0f);
		Camera->GetCameraComponent()->SetOrthoZoom(SharedOrthoZoom);
		break;
	case EViewportType::Orthographic_Right:
		Camera->SetActorLocation({ CameraAddPosition.X, -1000, CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 90 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 50000.0f);
		Camera->GetCameraComponent()->SetOrthoZoom(SharedOrthoZoom);
		break;
	case EViewportType::Orthographic_Front:
		Camera->SetActorLocation({ -1000 , CameraAddPosition.Y, CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 0 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 50000.0f);
		Camera->GetCameraComponent()->SetOrthoZoom(SharedOrthoZoom);
		break;
	case EViewportType::Orthographic_Back:
		Camera->SetActorLocation({ 1000 , CameraAddPosition.Y, CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 180 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 50000.0f);
		Camera->GetCameraComponent()->SetOrthoZoom(SharedOrthoZoom);
		break;
	}
}

void FViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	if (World->GetGizmoActor())
		World->GetGizmoActor()->ProcessGizmoInteraction(Camera, Viewport, static_cast<float>(X), static_cast<float>(Y));

	if (!bIsMouseButtonDown &&
		(!World->GetGizmoActor() || !World->GetGizmoActor()->GetbIsHovering()) &&
		bIsMouseRightButtonDown) // 직교투영이고 마우스 버튼이 눌려있을 때
	{
		if (ViewportType != EViewportType::Perspective)
		{
			// LockCursor 상태에서도 올바른 델타를 얻기 위해 InputManager 사용
			FVector2D MouseDelta = INPUT.GetMouseDelta();
			float deltaX = MouseDelta.X;
			float deltaY = MouseDelta.Y;

			if (Camera && (deltaX != 0 || deltaY != 0))
			{
				// 기준 픽셀→월드 스케일
				const float basePixelToWorld = 0.05f;

				// 줌인(값↑)일수록 더 천천히 움직이도록 역수 적용
				float zoom = Camera->GetCameraComponent()->GetZoomFactor();
				zoom = (zoom <= 0.f) ? 1.f : zoom; // 안전장치
				const float pixelToWorld = basePixelToWorld * zoom;

				const FVector right = Camera->GetRight();
				const FVector up = Camera->GetUp();

				CameraAddPosition = CameraAddPosition
					- right * (deltaX * pixelToWorld)
					+ up * (deltaY * pixelToWorld);

				SetupCameraMode();
			}
		}
		else if (ViewportType == EViewportType::Perspective)
		{
			PerspectiveCameraInput = true;
		}
	}
}

void FViewportClient::MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
	if (!Viewport || !World)
		return;

	// GetInstance viewport size
	FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
	FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));

	// X, Y are already local coordinates within the viewport, convert to global coordinates for picking
	FVector2D ViewportMousePos(static_cast<float>(X) + ViewportOffset.X, static_cast<float>(Y) + ViewportOffset.Y);

	if (Button == 0)
	{
		bIsMouseButtonDown = true;

		// 피킹이 비활성화된 경우 (플로팅 윈도우 뷰포트 등) 피킹 로직 건너뜀
		if (!bPickingEnabled)
		{
			return;
		}

		if (!World->GetGizmoActor())
		{
			return;
		}

		// 기즈모 호버링 중이면 피킹 하지 않음
		if (World->GetGizmoActor()->GetbIsHovering())
		{
			return;
		}

		// 더블 클릭 감지
		double CurrentTime = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		int32 DeltaX = abs(X - LastClickX);
		int32 DeltaY = abs(Y - LastClickY);
		bool bIsDoubleClick = (CurrentTime - LastClickTime < DoubleClickTime) &&
							  (DeltaX < DoubleClickDistance) && (DeltaY < DoubleClickDistance);

		// 클릭 정보 저장
		LastClickTime = CurrentTime;
		LastClickX = X;
		LastClickY = Y;

		// 피킹 수행
		Camera->SetWorld(World);
		UPrimitiveComponent* PickedComponent = URenderManager::GetInstance().GetRenderer()->GetPrimitiveCollided(
			static_cast<int>(ViewportMousePos.X), static_cast<int>(ViewportMousePos.Y));

		USelectionManager* SelectionMgr = World->GetSelectionManager();
		AActor* PickedActor = PickedComponent ? PickedComponent->GetOwner() : nullptr;
		AActor* CurrentSelectedActor = SelectionMgr->GetSelectedActor();

		if (PickedActor && PickedComponent)
		{
			if (bIsDoubleClick)
			{
				// 더블 클릭: 컴포넌트 피킹 모드로 진입
				SelectionMgr->SelectActorAndComponent(PickedActor, PickedComponent);
			}
			else
			{
				// 싱글 클릭
				if (!CurrentSelectedActor)
				{
					// 선택 없음 → Actor 선택
					SelectionMgr->SelectActor(PickedActor);
				}
				else if (SelectionMgr->IsActorMode())
				{
					// Actor 모드에서 클릭 → 항상 Actor 선택 (같은 Actor든 다른 Actor든)
					SelectionMgr->SelectActor(PickedActor);
				}
				else
				{
					// Component 모드에서 클릭
					if (CurrentSelectedActor == PickedActor)
					{
						// 같은 Actor 내 다른 컴포넌트 → 컴포넌트 전환
						SelectionMgr->SelectActorAndComponent(PickedActor, PickedComponent);
					}
					else
					{
						// 다른 Actor → Actor 모드로 복귀
						SelectionMgr->SelectActor(PickedActor);
					}
				}
			}
		}
		else
		{
			// 빈 공간 클릭: 선택 해제
			SelectionMgr->ClearSelection();
		}
	}
	else if (Button == 1)
	{
		//우클릭시
		bIsMouseRightButtonDown = true;
		MouseLastX = X;
		MouseLastY = Y;
	}
}

void FViewportClient::MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
	if (Button == 0) // Left mouse button
	{
		bIsMouseButtonDown = false;

		// 드래그 종료 처리를 위해 한번 더 호출
		if (World->GetGizmoActor())
		{
			World->GetGizmoActor()->ProcessGizmoInteraction(Camera, Viewport, static_cast<float>(X), static_cast<float>(Y));
		}
	}
	else
	{
		bIsMouseRightButtonDown = false;
		PerspectiveCameraInput = false;
	}
}

void FViewportClient::MouseWheel(float DeltaSeconds)
{
	if (!Camera)
	{
		return;
	}

	// ImGui가 마우스를 캡처하려는 경우 (콘솔, 컨텐츠 브라우저, 슬라이더, 버튼 등 UI 위) 무시
	if (ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}

	// 호버링 중이 아니면 처리하지 않음
	if (!bIsHovered)
	{
		return;
	}

	UCameraComponent* CameraComponent = Camera->GetCameraComponent();
	if (!CameraComponent)
	{
		return;
	}
	float WheelDelta = UInputManager::GetInstance().GetMouseWheelDelta();

	if (WheelDelta == 0.0f)
	{
		return;
	}

	// 우클릭 드래그 중일 때: 카메라 속도 스칼라 조절
	if (bIsMouseRightButtonDown || PerspectiveCameraInput)
	{
		// 속도 스칼라 조절 (휠 업: 증가, 휠 다운: 감소)
		float ScalarMultiplier = (WheelDelta > 0) ? 1.15f : (1.0f / 1.15f);
		float OldScalar = CameraSpeedScalar;
		float NewScalar = OldScalar * ScalarMultiplier;

		// 스칼라 범위 제한 (0.25 ~ 128.0)
		NewScalar = std::max(0.25f, std::min(128.0f, NewScalar));
		CameraSpeedScalar = NewScalar;

		// 현재 속도에 비례하여 새 속도 계산
		float CurrentSpeed = Camera->GetCameraSpeed();
		float NewSpeed = CurrentSpeed * (NewScalar / OldScalar);
		NewSpeed = std::max(0.1f, std::min(100.0f, NewSpeed));
		Camera->SetCameraSpeed(NewSpeed);
	}
	else if (ViewportType == EViewportType::Perspective)
	{
		// Perspective 뷰포트: 호버링 중인 뷰포트만 줌 (카메라 앞뒤 이동)
		FVector Forward = Camera->GetForward();
		float MoveSpeed = Camera->GetCameraSpeed() * WheelDelta * 0.5f;
		FVector NewLocation = Camera->GetActorLocation() + Forward * MoveSpeed;
		Camera->SetActorLocation(NewLocation);
	}
	else
	{
		// Orthographic 뷰포트: 공유 OrthoZoom 변경 (모든 Ortho 뷰포트에 적용)
		float orthoZoom = SharedOrthoZoom;
		// 휠 위로 = 줌인 = OrthoZoom 감소, 휠 아래 = 줌아웃 = OrthoZoom 증가
		// 비율 기반 스케일링 (10%씩)
		float zoomMultiplier = 1.0f - WheelDelta * 0.1f;
		orthoZoom *= zoomMultiplier;
		orthoZoom = std::max(0.001f, std::min(10.0f, orthoZoom));  // 범위 제한
		SharedOrthoZoom = orthoZoom;
		// 현재 카메라에 즉시 적용
		CameraComponent->SetOrthoZoom(orthoZoom);
	}
}

