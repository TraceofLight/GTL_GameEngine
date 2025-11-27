#include "pch.h"
#include "ParticleSystemActor.h"
#include "Source/Runtime/Engine/Particle/ParticleSystemComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"

AParticleSystemActor::AParticleSystemActor()
	: BoundsLineComponent(nullptr)
{
	ObjectName = "Particle System Actor";
	ParticleSystemComponent = CreateDefaultSubobject<UParticleSystemComponent>("ParticleSystemComponent");

	RootComponent = ParticleSystemComponent;

	// 바운딩 시각화용 라인 컴포넌트 생성
	BoundsLineComponent = CreateDefaultSubobject<ULineComponent>("BoundsLineComponent");
	BoundsLineComponent->SetLineVisible(false);  // 기본적으로 숨김
	BoundsLineComponent->SetAlwaysOnTop(true);   // 항상 최상위 렌더링
}

AParticleSystemActor::~AParticleSystemActor()
{
}

void AParticleSystemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AParticleSystemActor::SetParticleSystem(UParticleSystem* InTemplate)
{
	if (ParticleSystemComponent)
	{
		ParticleSystemComponent->SetTemplate(InTemplate);
		ParticleSystemComponent->ActivateSystem(true);
	}
}

void AParticleSystemActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Component))
		{
			ParticleSystemComponent = PSC;
			break;
		}
	}
}

void AParticleSystemActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		ParticleSystemComponent = Cast<UParticleSystemComponent>(RootComponent);
	}
}

// ============================================================================
// Bounds Visualization
// ============================================================================

void AParticleSystemActor::UpdateBoundsVisualization(bool bShowBounds)
{
	if (!BoundsLineComponent || !ParticleSystemComponent)
	{
		return;
	}

	if (bShowBounds)
	{
		CreateBoundsLines();
		BoundsLineComponent->SetLineVisible(true);
	}
	else
	{
		BoundsLineComponent->SetLineVisible(false);
	}
}

void AParticleSystemActor::CreateBoundsLines()
{
	if (!BoundsLineComponent || !ParticleSystemComponent)
	{
		return;
	}

	// 기존 라인 제거
	BoundsLineComponent->ClearLines();

	// 바운딩 박스 계산
	FVector Min, Max;
	ParticleSystemComponent->GetBoundingBox(Min, Max);

	FVector Center = (Min + Max) * 0.5f;
	float SphereRadius = ParticleSystemComponent->GetBoundingSphereRadius();


	// === AABB 박스 (주황색) ===
	// 8개의 코너
	FVector Corners[8] =
	{
		FVector(Min.X, Min.Y, Min.Z),  // 0: left-bottom-front
		FVector(Max.X, Min.Y, Min.Z),  // 1: right-bottom-front
		FVector(Max.X, Max.Y, Min.Z),  // 2: right-top-front
		FVector(Min.X, Max.Y, Min.Z),  // 3: left-top-front
		FVector(Min.X, Min.Y, Max.Z),  // 4: left-bottom-back
		FVector(Max.X, Min.Y, Max.Z),  // 5: right-bottom-back
		FVector(Max.X, Max.Y, Max.Z),  // 6: right-top-back
		FVector(Min.X, Max.Y, Max.Z),  // 7: left-top-back
	};

	FVector4 OrangeColor(1.0f, 0.5f, 0.0f, 1.0f);  // 주황색

	// Bottom face (4 lines)
	BoundsLineComponent->AddLine(Corners[0], Corners[1], OrangeColor);
	BoundsLineComponent->AddLine(Corners[1], Corners[2], OrangeColor);
	BoundsLineComponent->AddLine(Corners[2], Corners[3], OrangeColor);
	BoundsLineComponent->AddLine(Corners[3], Corners[0], OrangeColor);

	// Top face (4 lines)
	BoundsLineComponent->AddLine(Corners[4], Corners[5], OrangeColor);
	BoundsLineComponent->AddLine(Corners[5], Corners[6], OrangeColor);
	BoundsLineComponent->AddLine(Corners[6], Corners[7], OrangeColor);
	BoundsLineComponent->AddLine(Corners[7], Corners[4], OrangeColor);

	// Vertical edges (4 lines)
	BoundsLineComponent->AddLine(Corners[0], Corners[4], OrangeColor);
	BoundsLineComponent->AddLine(Corners[1], Corners[5], OrangeColor);
	BoundsLineComponent->AddLine(Corners[2], Corners[6], OrangeColor);
	BoundsLineComponent->AddLine(Corners[3], Corners[7], OrangeColor);

	// === Bounding Sphere (노란색, 3개의 원) ===
	FVector4 YellowColor(1.0f, 1.0f, 0.0f, 1.0f);  // 노란색
	const int32 CircleSegments = 32;

	// XY plane circle
	for (int32 i = 0; i < CircleSegments; ++i)
	{
		float Angle1 = (float)i / CircleSegments * 2.0f * 3.14159265f;
		float Angle2 = (float)((i + 1) % CircleSegments) / CircleSegments * 2.0f * 3.14159265f;

		FVector P1 = Center + FVector(cosf(Angle1) * SphereRadius, sinf(Angle1) * SphereRadius, 0.0f);
		FVector P2 = Center + FVector(cosf(Angle2) * SphereRadius, sinf(Angle2) * SphereRadius, 0.0f);

		BoundsLineComponent->AddLine(P1, P2, YellowColor);
	}

	// XZ plane circle
	for (int32 i = 0; i < CircleSegments; ++i)
	{
		float Angle1 = (float)i / CircleSegments * 2.0f * 3.14159265f;
		float Angle2 = (float)((i + 1) % CircleSegments) / CircleSegments * 2.0f * 3.14159265f;

		FVector P1 = Center + FVector(cosf(Angle1) * SphereRadius, 0.0f, sinf(Angle1) * SphereRadius);
		FVector P2 = Center + FVector(cosf(Angle2) * SphereRadius, 0.0f, sinf(Angle2) * SphereRadius);

		BoundsLineComponent->AddLine(P1, P2, YellowColor);
	}

	// YZ plane circle
	for (int32 i = 0; i < CircleSegments; ++i)
	{
		float Angle1 = (float)i / CircleSegments * 2.0f * 3.14159265f;
		float Angle2 = (float)((i + 1) % CircleSegments) / CircleSegments * 2.0f * 3.14159265f;

		FVector P1 = Center + FVector(0.0f, cosf(Angle1) * SphereRadius, sinf(Angle1) * SphereRadius);
		FVector P2 = Center + FVector(0.0f, cosf(Angle2) * SphereRadius, sinf(Angle2) * SphereRadius);

		BoundsLineComponent->AddLine(P1, P2, YellowColor);
	}
}
