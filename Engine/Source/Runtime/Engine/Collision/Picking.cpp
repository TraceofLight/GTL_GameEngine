#include "pch.h"

#include "Picking.h"
#include "Actor.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "CameraActor.h"
#include "MeshLoader.h"
#include"Vector.h"
#include "SelectionManager.h"
#include <cmath>
#include <algorithm>

#include "Gizmo/GizmoActor.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "GlobalConsole.h"
#include "ObjManager.h"
#include "ResourceManager.h"
#include"stdio.h"
#include "WorldPartitionManager.h"
#include "PlatformTime.h"
#include "World.h"

FRay MakeRayFromMouse(const FMatrix& InView,
	const FMatrix& InProj)
{
	// 1) Mouse to NDC (DirectX viewport convention: origin top-left)
	//    Query current screen size from InputManager
	FVector2D screen = UInputManager::GetInstance().GetScreenSize();
	float viewportW = (screen.X > 1.0f) ? screen.X : 1.0f;
	float viewportH = (screen.Y > 1.0f) ? screen.Y : 1.0f;

	const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
	const float NdcX = (2.0f * MousePosition.X / viewportW) - 1.0f;
	const float NdcY = 1.0f - (2.0f * MousePosition.Y / viewportH);

	// 2) View-space direction using projection scalars (PerspectiveFovLH)
	// InProj.M[0][0] = XScale, InProj.M[1][1] = YScale
	const float XScale = InProj.M[0][0];
	const float YScale = InProj.M[1][1];
	const float ViewDirX = NdcX / (XScale == 0.0f ? 1.0f : XScale);
	const float ViewDirY = NdcY / (YScale == 0.0f ? 1.0f : YScale);
	const float ViewDirZ = 1.0f; // Forward in view space

	// 3) Extract camera basis/position from InView (row-vector convention: basis in rows)
	const FVector Right = FVector(InView.M[0][0], InView.M[0][1], InView.M[0][2]);
	const FVector Up = FVector(InView.M[1][0], InView.M[1][1], InView.M[1][2]);
	const FVector Forward = FVector(InView.M[2][0], InView.M[2][1], InView.M[2][2]);
	const FVector t = FVector(InView.M[3][0], InView.M[3][1], InView.M[3][2]);
	// = (-dot(Right,Eye), -dot(Up,Eye), -dot(Fwd,Eye))
	const FVector Eye = (Right * (-t.X)) + (Up * (-t.Y)) + (Forward * (-t.Z));

	// 4) To world space
	const FVector WorldDirection = (Right * ViewDirX + Up * ViewDirY + Forward * ViewDirZ).GetSafeNormal();

	FRay Ray;
	Ray.Origin = Eye;
	Ray.Direction = WorldDirection;
	return Ray;
}

FRay MakeRayFromMouseWithCamera(const FMatrix& InView,
	const FMatrix& InProj,
	const FVector& CameraWorldPos,
	const FVector& CameraRight,
	const FVector& CameraUp,
	const FVector& CameraForward)
{
	// 1) Mouse to NDC (DirectX viewport convention: origin top-left)
	FVector2D screen = UInputManager::GetInstance().GetScreenSize();
	float viewportW = (screen.X > 1.0f) ? screen.X : 1.0f;
	float viewportH = (screen.Y > 1.0f) ? screen.Y : 1.0f;

	const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
	const float NdcX = (2.0f * MousePosition.X / viewportW) - 1.0f;
	const float NdcY = 1.0f - (2.0f * MousePosition.Y / viewportH);

	// 2) View-space direction using projection scalars
	const float XScale = InProj.M[0][0];
	const float YScale = InProj.M[1][1];
	const float ViewDirX = NdcX / (XScale == 0.0f ? 1.0f : XScale);
	const float ViewDirY = NdcY / (YScale == 0.0f ? 1.0f : YScale);
	const float ViewDirZ = 1.0f; // Forward in view space

	// 3) Use camera's actual world-space orientation vectors
	// Transform view direction to world space using camera's real orientation
	const FVector WorldDirection = (CameraRight * ViewDirX + CameraUp * ViewDirY + CameraForward * ViewDirZ).
		GetSafeNormal();

	FRay Ray;
	Ray.Origin = CameraWorldPos;
	Ray.Direction = WorldDirection;
	return Ray;
}

FRay MakeRayFromViewport(const FMatrix& InView,
	const FMatrix& InProj,
	const FVector& CameraWorldPos,
	const FVector& CameraRight,
	const FVector& CameraUp,
	const FVector& CameraForward,
	const FVector2D& ViewportMousePos,
	const FVector2D& ViewportSize,
	const FVector2D& ViewportOffset)
{
	// 1) Convert global mouse position to viewport-relative position
	float localMouseX = ViewportMousePos.X - ViewportOffset.X;
	float localMouseY = ViewportMousePos.Y - ViewportOffset.Y;

	// 2) Use viewport-specific size for NDC conversion
	float viewportW = (ViewportSize.X > 1.0f) ? ViewportSize.X : 1.0f;
	float viewportH = (ViewportSize.Y > 1.0f) ? ViewportSize.Y : 1.0f;

	const float NdcX = (2.0f * localMouseX / viewportW) - 1.0f;
	const float NdcY = 1.0f - (2.0f * localMouseY / viewportH);

	// Check if this is orthographic projection
	bool bIsOrthographic = std::fabs(InProj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;

	FRay Ray;

	if (bIsOrthographic)
	{
		// Orthographic projection
		// GetInstance orthographic bounds from projection matrix
		float OrthoWidth = 2.0f / InProj.M[0][0];
		float OrthoHeight = 2.0f / InProj.M[1][1];

		// Calculate world space offset from camera center
		float WorldOffsetX = NdcX * OrthoWidth * 0.5f;
		float WorldOffsetY = NdcY * OrthoHeight * 0.5f;

		// Ray origin is offset from camera position on the viewing plane
		Ray.Origin = CameraWorldPos + (CameraRight * WorldOffsetX) + (CameraUp * WorldOffsetY);

		// Ray direction is always forward for orthographic
		Ray.Direction = CameraForward;
	}
	else
	{
		// Perspective projection (existing code)
		const float XScale = InProj.M[0][0];
		const float YScale = InProj.M[1][1];
		const float ViewDirX = NdcX / (XScale == 0.0f ? 1.0f : XScale);
		const float ViewDirY = NdcY / (YScale == 0.0f ? 1.0f : YScale);
		const float ViewDirZ = 1.0f;

		const FVector WorldDirection = (CameraRight * ViewDirX + CameraUp * ViewDirY + CameraForward * ViewDirZ).GetSafeNormal();

		Ray.Origin = CameraWorldPos;
		Ray.Direction = WorldDirection;
	}

	return Ray;
}

bool IntersectRaySphere(const FRay& InRay, const FVector& InCenter, float InRadius, float& OutT)
{
	// Solve ||(RayOrigin + T*RayDir) - Center||^2 = Radius^2
	const FVector OriginToCenter = InRay.Origin - InCenter;
	const float QuadraticA = FVector::FVector::Dot(InRay.Direction, InRay.Direction); // Typically 1 for normalized ray
	const float QuadraticB = 2.0f * FVector::FVector::Dot(OriginToCenter, InRay.Direction);
	const float QuadraticC = FVector::FVector::Dot(OriginToCenter, OriginToCenter) - InRadius * InRadius;

	const float Discriminant = QuadraticB * QuadraticB - 4.0f * QuadraticA * QuadraticC;
	if (Discriminant < 0.0f)
	{
		return false;
	}

	const float SqrtD = std::sqrt(Discriminant >= 0.0f ? Discriminant : 0.0f);
	const float Inv2A = 1.0f / (2.0f * QuadraticA);
	const float T0 = (-QuadraticB - SqrtD) * Inv2A;
	const float T1 = (-QuadraticB + SqrtD) * Inv2A;

	// Pick smallest positive T
	const float ClosestT = (T0 > 0.0f) ? T0 : T1;
	if (ClosestT <= 0.0f)
	{
		return false;
	}

	OutT = ClosestT;
	return true;
}

bool IntersectRayPlane(const FRay& InRay, const FVector& InPlanePoint, const FVector& InPlaneNormal, float& OutT)
{
	// 평면 방정식: FVector::Dot(P - PlanePoint, PlaneNormal) = 0
	// 레이 방정식: P = RayOrigin + t * RayDirection
	// 교차점: t = -FVector::Dot(RayOrigin - PlanePoint, PlaneNormal) / FVector::Dot(RayDirection, PlaneNormal)

	const float Denominator = FVector::Dot(InRay.Direction, InPlaneNormal);

	// 레이가 평면과 평행한 경우 (또는 거의 평행)
	if (std::abs(Denominator) < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector OriginToPlane = InPlanePoint - InRay.Origin;
	const float T = FVector::Dot(OriginToPlane, InPlaneNormal) / Denominator;

	// 레이가 평면의 뒷면을 향하는 경우 (T < 0)
	if (T <= 0.0f)
	{
		return false;
	}

	OutT = T;
	return true;
}

// 삼각형을 이루는 3개의 점 
bool IntersectRayTriangleMT(const FRay& InRay, const FVector& InA, const FVector& InB, const FVector& InC, float& OutT)
{
	const float Epsilon = KINDA_SMALL_NUMBER;

	// 삼각형 한점으로 시작하는 두 벡터 
	const FVector Edge1 = InB - InA;
	const FVector Edge2 = InC - InA;

	// 레이 방향과 , 삼각형 Edge와 수직한 벡터
	const FVector Perpendicular = FVector::Cross(InRay.Direction, Edge2);
	// 내적 했을때 0이라면, 세 벡터는 한 평면 안에 같이 있는 것이다. 
	const float Determinant = FVector::FVector::Dot(Edge1, Perpendicular);

	// 거의 0이면 평행 상태에 있다고 판단 
	if (Determinant > -Epsilon && Determinant < Epsilon)
		return false;

	const float InvDeterminant = 1.0f / Determinant;
	const FVector OriginToA = InRay.Origin - InA;
	const float U = InvDeterminant * FVector::FVector::Dot(OriginToA, Perpendicular);
	if (U < -Epsilon || U > 1.0f + Epsilon)
		return false;

	const FVector CrossQ = FVector::Cross(OriginToA, Edge1);
	const float V = InvDeterminant * FVector::FVector::Dot(InRay.Direction, CrossQ);
	if (V < -Epsilon || (U + V) > 1.0f + Epsilon)
		return false;

	const float Distance = InvDeterminant * FVector::FVector::Dot(Edge2, CrossQ);

	if (Distance > Epsilon) // ray intersection
	{
		OutT = Distance;
		return true;
	}
	return false;
}

// PickingSystem 구현
AActor* CPickingSystem::PerformPicking(const TArray<AActor*>& Actors, ACameraActor* Camera)
{
	if (!Camera) return nullptr;

	// 레이 생성 - 카메라 위치와 방향을 직접 전달
	const FMatrix View = Camera->GetViewMatrix();
	const FMatrix Proj = Camera->GetProjectionMatrix();
	const FVector CameraWorldPos = Camera->GetActorLocation();
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();
	FRay ray = MakeRayFromMouseWithCamera(View, Proj, CameraWorldPos, CameraRight, CameraUp, CameraForward);

	int pickedIndex = -1;
	float pickedT = 1e9f;

	// 모든 액터에 대해 피킹 테스트
	for (int i = 0; i < Actors.Num(); ++i)
	{
		AActor* Actor = Actors[i];
		if (!Actor) continue;

		// Skip hidden actors for picking
		if (Actor->GetActorHiddenInEditor()) continue;

		float hitDistance;
		if (CheckActorPicking(Actor, ray, hitDistance))
		{
			if (hitDistance < pickedT)
			{
				pickedT = hitDistance;
				pickedIndex = i;
			}
		}
	}

	if (pickedIndex >= 0)
	{
		char buf[160];
		sprintf_s(buf, "[Pick] Hit primitive %d at t=%.3f (Speed=NORMAL)\n", pickedIndex, pickedT);
		UE_LOG(buf);
		return Actors[pickedIndex];
	}
	else
	{
		UE_LOG("Pick: Fast: No hit");
		return nullptr;
	}
}

// Ray-Actor 리턴 
AActor* CPickingSystem::PerformViewportPicking(const TArray<AActor*>& Actors,
	ACameraActor* Camera,
	const FVector2D& ViewportMousePos,
	const FVector2D& ViewportSize,
	const FVector2D& ViewportOffset)
{
	if (!Camera) return nullptr;

	// 뷰포트별 레이 생성 - 각 뷰포트의 로컬 마우스 좌표와 크기, 오프셋 사용
	const FMatrix View = Camera->GetViewMatrix();
	const FMatrix Proj = Camera->GetProjectionMatrix();
	const FVector CameraWorldPos = Camera->GetActorLocation();
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();

	FRay ray = MakeRayFromViewport(View, Proj, CameraWorldPos, CameraRight, CameraUp, CameraForward,
		ViewportMousePos, ViewportSize, ViewportOffset);

	int pickedIndex = -1;
	float pickedT = 1e9f;

	// 모든 액터에 대해 피킹 테스트
	for (int i = 0; i < Actors.Num(); ++i)
	{
		AActor* Actor = Actors[i];
		if (!Actor) continue;

		// Skip hidden actors for picking
		if (Actor->GetActorHiddenInEditor()) continue;

		float hitDistance;
		if (CheckActorPicking(Actor, ray, hitDistance))
		{
			if (hitDistance < pickedT)
			{
				pickedT = hitDistance;
				pickedIndex = i;
			}
		}
	}

	if (pickedIndex >= 0)
	{
		char buf[160];
		sprintf_s(buf, "[Viewport Pick] Hit primitive %d at t=%.3f\n", pickedIndex, pickedT);
		UE_LOG(buf);
		return Actors[pickedIndex];
	}
	else
	{
		UE_LOG("Pick: Viewport: No hit");
		return nullptr;
	}
}

uint32 CPickingSystem::TotalPickCount = 0;
uint64 CPickingSystem::LastPickTime = 0;
uint64 CPickingSystem::TotalPickTime = 0;

AActor* CPickingSystem::PerformViewportPicking(const TArray<AActor*>& Actors,
	ACameraActor* Camera,
	const FVector2D& ViewportMousePos,
	const FVector2D& ViewportSize,
	const FVector2D& ViewportOffset,
	float ViewportAspectRatio, FViewport* Viewport)
{
	if (!Camera) return nullptr;
	UWorld* CurrentWorld = Camera->GetWorld();
	if (!CurrentWorld) return nullptr;
	UWorldPartitionManager* Partition = CurrentWorld->GetPartitionManager();
	if (!Partition) return nullptr;

	// 뷰포트별 레이 생성 - 커스텀 aspect ratio 사용
	const FMatrix View = Camera->GetViewMatrix();
	const FMatrix Proj = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
	const FVector CameraWorldPos = Camera->GetActorLocation();
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();

	FRay ray = MakeRayFromViewport(View, Proj, CameraWorldPos, CameraRight, CameraUp, CameraForward,
		ViewportMousePos, ViewportSize, ViewportOffset);

	int PickedIndex = -1;
	float PickedT = 1e9f;

	// 퍼포먼스 측정용 카운터 시작
	FScopeCycleCounter PickCounter;

	// 전체 Picking 횟수 누적
	++TotalPickCount;

	// 베스트 퍼스트 탐색으로 가장 가까운 것을 직접 구한다
	AActor* PickedActor = nullptr;
	Partition->RayQueryClosest(ray, PickedActor, PickedT);
	LastPickTime = static_cast<uint64>(PickCounter.Finish());
	TotalPickTime += LastPickTime;
	double Milliseconds = (static_cast<double>(LastPickTime) * FPlatformTime::GetSecondsPerCycle()) * 1000.0;

	if (PickedActor)
	{
		PickedIndex = 0;
		char buf[160];
		sprintf_s(buf, "[Pick] Hit primitive %d at t=%.3f | time=%.6lf ms\n",
			PickedIndex, PickedT, Milliseconds);
		UE_LOG(buf);
		return PickedActor;
	}
	else
	{
		char buf[160];
		sprintf_s(buf, "[Pick] No hit | time=%.6f ms\n", Milliseconds);
		UE_LOG(buf);
		return nullptr;
	}
}

uint32 CPickingSystem::IsHoveringGizmoForViewport(AGizmoActor* GizmoTransActor, const ACameraActor* Camera,
	const FVector2D& ViewportMousePos,
	const FVector2D& ViewportSize,
	const FVector2D& ViewportOffset, FViewport* Viewport,
	FVector& OutImpactPoint)
{
	if (!GizmoTransActor || !Camera)
		return 0;

	float ViewportAspectRatio = ViewportSize.X / ViewportSize.Y;
	if (ViewportSize.Y == 0)
		ViewportAspectRatio = 1.0f; // 0으로 나누기 방지

	// 뷰포트별 레이 생성 - 전달받은 뷰포트 정보 사용
	const FMatrix View = Camera->GetViewMatrix();
	const FMatrix Proj = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
	const FVector CameraWorldPos = Camera->GetActorLocation();
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();

	FRay Ray = MakeRayFromViewport(View, Proj, CameraWorldPos, CameraRight, CameraUp, CameraForward,
		ViewportMousePos, ViewportSize, ViewportOffset);

	// 가장 가까운 충돌 지점을 찾기 위한 임시 변수
	FVector TempImpactPoint;

	uint32 ClosestAxis = 0;
	float ClosestDistance = 1e9f;
	float HitDistance;

	switch (GizmoTransActor->GetMode())
	{
	case EGizmoMode::Translate:
		if (UStaticMeshComponent* ArrowX = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowX()))
		{
			if (CheckGizmoComponentPicking(ArrowX, Ray, ViewportSize.X, ViewportSize.Y, View, Proj, HitDistance, TempImpactPoint))
			{
				if (HitDistance < ClosestDistance)
				{
					ClosestDistance = HitDistance;
					ClosestAxis = 1;
					OutImpactPoint = TempImpactPoint;
				}
			}
		}

		// Y축 화살표 검사
		if (UStaticMeshComponent* ArrowY = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowY()))
		{
			if (CheckGizmoComponentPicking(ArrowY, Ray, ViewportSize.X, ViewportSize.Y, View, Proj, HitDistance, TempImpactPoint))
			{
				if (HitDistance < ClosestDistance)
				{
					ClosestDistance = HitDistance;
					ClosestAxis = 2;
					OutImpactPoint = TempImpactPoint;
				}
			}
		}

		// Z축 화살표 검사
		if (UStaticMeshComponent* ArrowZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetArrowZ()))
		{
			if (CheckGizmoComponentPicking(ArrowZ, Ray, ViewportSize.X, ViewportSize.Y, View, Proj, HitDistance, TempImpactPoint))
			{
				if (HitDistance < ClosestDistance)
				{
					ClosestDistance = HitDistance;
					ClosestAxis = 3;
					OutImpactPoint = TempImpactPoint;
				}
			}
		}
		break;
	case EGizmoMode::Scale:
		if (UStaticMeshComponent* ScaleX = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleX()))
		{
			if (CheckGizmoComponentPicking(ScaleX, Ray, ViewportSize.X, ViewportSize.Y, View, Proj, HitDistance, TempImpactPoint))
			{
				if (HitDistance < ClosestDistance)
				{
					ClosestDistance = HitDistance;
					ClosestAxis = 1;
					OutImpactPoint = TempImpactPoint;
				}
			}
		}

		// Y축 화살표 검사
		if (UStaticMeshComponent* ScaleY = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleY()))
		{
			if (CheckGizmoComponentPicking(ScaleY, Ray, ViewportSize.X, ViewportSize.Y, View, Proj, HitDistance, TempImpactPoint))
			{
				if (HitDistance < ClosestDistance)
				{
					ClosestDistance = HitDistance;
					ClosestAxis = 2;
					OutImpactPoint = TempImpactPoint;
				}
			}
		}

		// Z축 화살표 검사
		if (UStaticMeshComponent* ScaleZ = Cast<UStaticMeshComponent>(GizmoTransActor->GetScaleZ()))
		{
			if (CheckGizmoComponentPicking(ScaleZ, Ray, ViewportSize.X, ViewportSize.Y, View, Proj, HitDistance, TempImpactPoint))
			{
				if (HitDistance < ClosestDistance)
				{
					ClosestDistance = HitDistance;
					ClosestAxis = 3;
					OutImpactPoint = TempImpactPoint;
				}
			}
		}
		break;
	case EGizmoMode::Rotate:
	{
		// 기즈모 위치
		FVector GizmoLocation = GizmoTransActor->GetActorLocation();

		// Screen-constant scale 계산
		FVector ToGizmo = GizmoLocation - CameraWorldPos;
		float ViewZ = FVector::Dot(ToGizmo, CameraForward);

		if (ViewZ > 0.0f)
		{
			float ProjYY = Proj.M[1][1];
			constexpr float TargetPixels = 128.0f;
			float RenderScale = (TargetPixels * ViewZ) / (ProjYY * ViewportSize.Y * 0.5f);

			// Rotation 링 충돌 설정
			const float InnerRadius = 0.75f * RenderScale;
			const float OuterRadius = 0.85f * RenderScale;

			// Local/World 회전
			FQuat BaseRot = FQuat::Identity();
			if (GizmoTransActor->GetSpace() == EGizmoSpace::Local)
			{
				// SelectionManager에서 선택된 컴포넌트의 회전 가져오기
				if (GWorld && GWorld->GetSelectionManager())
				{
					USceneComponent* SelectedComp = GWorld->GetSelectionManager()->GetSelectedComponent();
					if (SelectedComp)
					{
						BaseRot = SelectedComp->GetWorldRotation();
					}
				}
			}

			// 축 정의 (X=Forward, Y=Right, Z=Up)
			FVector GizmoAxes[3] = {
				BaseRot.RotateVector(FVector(1, 0, 0)),  // X축
				BaseRot.RotateVector(FVector(0, 1, 0)),  // Y축
				BaseRot.RotateVector(FVector(0, 0, 1))   // Z축
			};

			// BaseAxis 정의 (QuarterRing 각도 범위 체크용)
			const FVector BaseAxis0[3] = {
				FVector(0, 0, 1),  // X축 링: Z→Y
				FVector(1, 0, 0),  // Y축 링: X→Z
				FVector(1, 0, 0)   // Z축 링: X→Y
			};
			const FVector BaseAxis1[3] = {
				FVector(0, 1, 0),  // X축 링: Y
				FVector(0, 0, 1),  // Y축 링: Z
				FVector(0, 1, 0)   // Z축 링: Y
			};

			// 각 축 링 피킹
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				// 레이-평면 충돌 검사
				FVector PlaneNormal = GizmoAxes[AxisIndex];
				if (IntersectRayPlane(Ray, GizmoLocation, PlaneNormal, HitDistance))
				{
					FVector CollisionPoint = Ray.Origin + Ray.Direction * HitDistance;
					FVector RadiusVector = CollisionPoint - GizmoLocation;
					float Distance = RadiusVector.Size();

					// 반지름 범위 체크
					if (Distance >= InnerRadius && Distance <= OuterRadius)
					{
						// QuarterRing 각도 범위 체크
						FVector ToHit = CollisionPoint - GizmoLocation;
						FVector Projected = ToHit - (PlaneNormal * FVector::Dot(ToHit, PlaneNormal));
						float ProjLen = Projected.Size();

						if (ProjLen > 0.001f)
						{
							Projected = Projected.GetNormalized();

							// Local 모드면 회전 적용
							FVector WorldBaseAxis0 = BaseRot.RotateVector(BaseAxis0[AxisIndex]);
							FVector WorldBaseAxis1 = BaseRot.RotateVector(BaseAxis1[AxisIndex]);

							// 카메라 방향에 따른 플립 판정
							FVector DirectionToWidget = (GizmoLocation - CameraWorldPos).GetNormalized();
							bool bMirrorAxis0 = (FVector::Dot(WorldBaseAxis0, DirectionToWidget) <= 0.0f);
							bool bMirrorAxis1 = (FVector::Dot(WorldBaseAxis1, DirectionToWidget) <= 0.0f);

							FVector RenderAxis0 = bMirrorAxis0 ? WorldBaseAxis0 : -WorldBaseAxis0;
							FVector RenderAxis1 = bMirrorAxis1 ? WorldBaseAxis1 : -WorldBaseAxis1;

							// QuarterRing 각도 범위: [0, 90도]
							float Dot0 = FVector::Dot(Projected, RenderAxis0);
							float Dot1 = FVector::Dot(Projected, RenderAxis1);

							// 90도 범위 내에 있는지 확인 (둘 다 양수)
							if (Dot0 >= -0.01f && Dot1 >= -0.01f)
							{
								if (HitDistance < ClosestDistance)
								{
									ClosestDistance = HitDistance;
									ClosestAxis = (AxisIndex == 0) ? 1 : (AxisIndex == 1) ? 2 : 4;
									OutImpactPoint = CollisionPoint;
								}
							}
						}
					}
				}
			}
		}
		break;
	}
	default:
		break;
	}

	// ═════════════════════════════════════════════════════════════
	// 평면 기즈모 및 중심 구체 피킹 (Translate/Scale 모드만)
	// ═════════════════════════════════════════════════════════════
	if (GizmoTransActor->GetMode() == EGizmoMode::Translate || GizmoTransActor->GetMode() == EGizmoMode::Scale)
	{
		// 기즈모 위치
		FVector GizmoLocation = GizmoTransActor->GetActorLocation();

		// Screen-constant scale 계산
		FVector CameraPos = CameraWorldPos;
		FVector ToGizmo = GizmoLocation - CameraPos;
		float ViewZ = FVector::Dot(ToGizmo, CameraForward);

		if (ViewZ > 0.0f)
		{
			float ProjYY = Proj.M[1][1];
			constexpr float TargetPixels = 128.0f;
			float RenderScale = (TargetPixels * ViewZ) / (ProjYY * ViewportSize.Y * 0.5f);

			// Local/World 회전
			FQuat BaseRot = FQuat::Identity();
			if (GizmoTransActor->GetSpace() == EGizmoSpace::Local)
			{
				// SelectionManager에서 선택된 컴포넌트의 회전 가져오기
				if (GWorld && GWorld->GetSelectionManager())
				{
					USceneComponent* SelectedComp = GWorld->GetSelectionManager()->GetSelectedComponent();
					if (SelectedComp)
					{
						BaseRot = SelectedComp->GetWorldRotation();
					}
				}
			}

			// ──────────────────────────────────────────────
			// 평면 기즈모 피킹 (중심 구체보다 우선)
			// ──────────────────────────────────────────────
			constexpr float PlaneSize = 0.3f; // 평면 크기 (RenderTranslatePlanes의 CornerPos와 일치)
			const float PlaneExtent = PlaneSize * RenderScale;
			bool bPlaneHit = false;  // 평면이 hit되었는지 추적

			// XY 평면 (Z축 수직)
			{
				FVector PlaneNormal = BaseRot.RotateVector(FVector(0, 0, 1)); // Z축
				if (IntersectRayPlane(Ray, GizmoLocation, PlaneNormal, HitDistance))
				{
					FVector HitPoint = Ray.Origin + Ray.Direction * HitDistance;
					FVector LocalHit = HitPoint - GizmoLocation;

					// 로컬 좌표로 변환
					FVector Axis0 = BaseRot.RotateVector(FVector(1, 0, 0)); // X축
					FVector Axis1 = BaseRot.RotateVector(FVector(0, 1, 0)); // Y축
					float ProjAxis0 = FVector::Dot(LocalHit, Axis0);
					float ProjAxis1 = FVector::Dot(LocalHit, Axis1);

					// 평면 영역 내에 있는지 확인 (코너 영역)
					if (ProjAxis0 >= 0.0f && ProjAxis0 <= PlaneExtent &&
						ProjAxis1 >= 0.0f && ProjAxis1 <= PlaneExtent)
					{
						if (HitDistance < ClosestDistance)
						{
							ClosestDistance = HitDistance;
							ClosestAxis = 8; // XY_Plane
							OutImpactPoint = HitPoint;
							bPlaneHit = true;
						}
					}
				}
			}

			// XZ 평면 (Y축 수직)
			{
				FVector PlaneNormal = BaseRot.RotateVector(FVector(0, 1, 0)); // Y축
				if (IntersectRayPlane(Ray, GizmoLocation, PlaneNormal, HitDistance))
				{
					FVector HitPoint = Ray.Origin + Ray.Direction * HitDistance;
					FVector LocalHit = HitPoint - GizmoLocation;

					FVector Axis0 = BaseRot.RotateVector(FVector(1, 0, 0)); // X축
					FVector Axis2 = BaseRot.RotateVector(FVector(0, 0, 1)); // Z축
					float ProjAxis0 = FVector::Dot(LocalHit, Axis0);
					float ProjAxis2 = FVector::Dot(LocalHit, Axis2);

					if (ProjAxis0 >= 0.0f && ProjAxis0 <= PlaneExtent &&
						ProjAxis2 >= 0.0f && ProjAxis2 <= PlaneExtent)
					{
						if (HitDistance < ClosestDistance)
						{
							ClosestDistance = HitDistance;
							ClosestAxis = 16; // XZ_Plane
							OutImpactPoint = HitPoint;
							bPlaneHit = true;
						}
					}
				}
			}

			// YZ 평면 (X축 수직)
			{
				FVector PlaneNormal = BaseRot.RotateVector(FVector(1, 0, 0)); // X축
				if (IntersectRayPlane(Ray, GizmoLocation, PlaneNormal, HitDistance))
				{
					FVector HitPoint = Ray.Origin + Ray.Direction * HitDistance;
					FVector LocalHit = HitPoint - GizmoLocation;

					FVector Axis1 = BaseRot.RotateVector(FVector(0, 1, 0)); // Y축
					FVector Axis2 = BaseRot.RotateVector(FVector(0, 0, 1)); // Z축
					float ProjAxis1 = FVector::Dot(LocalHit, Axis1);
					float ProjAxis2 = FVector::Dot(LocalHit, Axis2);

					if (ProjAxis1 >= 0.0f && ProjAxis1 <= PlaneExtent &&
						ProjAxis2 >= 0.0f && ProjAxis2 <= PlaneExtent)
					{
						if (HitDistance < ClosestDistance)
						{
							ClosestDistance = HitDistance;
							ClosestAxis = 32; // YZ_Plane
							OutImpactPoint = HitPoint;
							bPlaneHit = true;
						}
					}
				}
			}

			// ──────────────────────────────────────────────
			// 중심 구체 피킹 (평면 영역 밖일 때만)
			// ──────────────────────────────────────────────
			if (!bPlaneHit)
			{
				const float SphereRadius = 0.08f * RenderScale;  // 평면(0.3)보다 작지만 적당한 크기
				if (IntersectRaySphere(Ray, GizmoLocation, SphereRadius, HitDistance))
				{
					if (HitDistance < ClosestDistance)
					{
						ClosestDistance = HitDistance;
						ClosestAxis = 64; // Center
						OutImpactPoint = Ray.Origin + Ray.Direction * HitDistance;
					}
				}
			}
		}
	}

	return ClosestAxis;
}

bool CPickingSystem::CheckGizmoComponentPicking(UStaticMeshComponent* Component, const FRay& Ray,
	float ViewWidth, float ViewHeight, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix,
	float& OutDistance, FVector& OutImpactPoint)
{
	if (!Component) return false;

	if (UGizmoArrowComponent* GizmoComponent = Cast<UGizmoArrowComponent>(Component))
	{
		GizmoComponent->SetDrawScale(ViewWidth, ViewHeight, ViewMatrix, ProjectionMatrix);
	}

	// Gizmo 메시는 FStaticMesh(쿠킹된 데이터)를 사용
	FStaticMesh* StaticMesh = Component->GetStaticMesh()->GetStaticMeshAsset();

	if (!StaticMesh) return false;

	// 피킹 계산에는 컴포넌트의 월드 변환 행렬 사용
	FMatrix WorldMatrix = Component->GetWorldMatrix();

	float ClosestT = 1e9f;
	FVector ClosestImpactPoint = FVector::Zero(); // 가장 가까운 충돌 지점 저장
	bool bHasHit = false;

	// 인덱스가 있는 경우: 인덱스 삼각형 집합 검사
	if (StaticMesh->Indices.Num() >= 3)
	{
		uint32 IndexNum = StaticMesh->Indices.Num();
		for (uint32 Idx = 0; Idx + 2 < IndexNum; Idx += 3)
		{
			const FNormalVertex& V0N = StaticMesh->Vertices[StaticMesh->Indices[Idx + 0]];
			const FNormalVertex& V1N = StaticMesh->Vertices[StaticMesh->Indices[Idx + 1]];
			const FNormalVertex& V2N = StaticMesh->Vertices[StaticMesh->Indices[Idx + 2]];

			FVector A = V0N.pos * WorldMatrix;
			FVector B = V1N.pos * WorldMatrix;
			FVector C = V2N.pos * WorldMatrix;

			float THit;
			if (IntersectRayTriangleMT(Ray, A, B, C, THit))
			{
				if (THit < ClosestT)
				{
					ClosestT = THit;
					bHasHit = true;
					ClosestImpactPoint = Ray.Origin + Ray.Direction * THit; // 충돌 지점 계산
				}
			}
		}
	}
	// 인덱스가 없는 경우: 정점 배열을 순차적 삼각형으로 간주
	else if (StaticMesh->Vertices.Num() >= 3)
	{
		uint32 VertexNum = StaticMesh->Vertices.Num();
		for (uint32 Idx = 0; Idx + 2 < VertexNum; Idx += 3)
		{
			const FNormalVertex& V0N = StaticMesh->Vertices[Idx + 0];
			const FNormalVertex& V1N = StaticMesh->Vertices[Idx + 1];
			const FNormalVertex& V2N = StaticMesh->Vertices[Idx + 2];

			FVector A = V0N.pos * WorldMatrix;
			FVector B = V1N.pos * WorldMatrix;
			FVector C = V2N.pos * WorldMatrix;

			float THit;
			if (IntersectRayTriangleMT(Ray, A, B, C, THit))
			{
				if (THit < ClosestT)
				{
					ClosestT = THit;
					bHasHit = true;
					ClosestImpactPoint = Ray.Origin + Ray.Direction * THit; // 충돌 지점 계산
				}
			}
		}
	}

	// 가장 가까운 교차가 있으면 거리 반환
	if (bHasHit)
	{
		OutDistance = ClosestT;
		OutImpactPoint = ClosestImpactPoint; // 최종 충돌 지점 반환
		return true;
	}

	return false;
}

bool CPickingSystem::CheckActorPicking(const AActor* Actor, const FRay& Ray, float& OutDistance)
{
	if (!Actor) return false;

	// 액터의 모든 SceneComponent 순회
	for (auto SceneComponent : Actor->GetSceneComponents())
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(SceneComponent))
		{
			UStaticMesh* MeshRes = StaticMeshComponent->GetStaticMesh();
			if (!MeshRes) continue;

			FStaticMesh* StaticMesh = MeshRes->GetStaticMeshAsset();
			if (!StaticMesh) continue;

			// 로컬 공간에서의 레이로 변환
			const FMatrix WorldMatrix = StaticMeshComponent->GetWorldMatrix();
			const FMatrix InvWorld = WorldMatrix.InverseAffine();
			const FVector4 RayOrigin4(Ray.Origin.X, Ray.Origin.Y, Ray.Origin.Z, 1.0f);
			const FVector4 RayDir4(Ray.Direction.X, Ray.Direction.Y, Ray.Direction.Z, 0.0f);
			const FVector4 LocalOrigin4 = RayOrigin4 * InvWorld;
			const FVector4 LocalDir4 = RayDir4 * InvWorld;
			const FRay LocalRay{ FVector(LocalOrigin4.X, LocalOrigin4.Y, LocalOrigin4.Z), FVector(LocalDir4.X, LocalDir4.Y, LocalDir4.Z) };

			// 캐시된 BVH 사용 (동일 OBJ 경로는 동일 BVH 공유)
			FMeshBVH* BVH = UResourceManager::GetInstance().GetOrBuildMeshBVH(MeshRes->GetAssetPathFileName(), StaticMesh);
			if (BVH)
			{
				float THitLocal;
				if (BVH->IntersectRay(LocalRay, StaticMesh->Vertices, StaticMesh->Indices, THitLocal))
				{
					const FVector HitLocal = FVector(
						LocalOrigin4.X + LocalDir4.X * THitLocal,
						LocalOrigin4.Y + LocalDir4.Y * THitLocal,
						LocalOrigin4.Z + LocalDir4.Z * THitLocal);
					const FVector4 HitLocal4(HitLocal.X, HitLocal.Y, HitLocal.Z, 1.0f);
					const FVector4 HitWorld4 = HitLocal4 * WorldMatrix;
					const FVector HitWorld(HitWorld4.X, HitWorld4.Y, HitWorld4.Z);
					const float THitWorld = (HitWorld - Ray.Origin).Size();
					OutDistance = THitWorld;
					return true;
				}
			}
		}
	}

	return false;
}
