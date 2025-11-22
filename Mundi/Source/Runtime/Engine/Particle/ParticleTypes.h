#pragma once

/**
 * @brief 파티클 시스템에서 사용하는 열거형 및 기본 타입 정의
 */

// 동적 이미터 타입
enum class EDynamicEmitterType : uint8
{
	Sprite,
	Mesh,
	Beam,
	Trail,
	AnimTrail
};

// 파티클 축 잠금
enum class EParticleAxisLock : uint8
{
	None,
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
	XYZ
};

// 파티클 화면 정렬 방식
enum class EParticleScreenAlignment : uint8
{
	Square,           // 정사각형
	Rectangle,        // 직사각형
	Velocity,         // 속도 방향
	TypeSpecific,     // 타입별 고유
	FacingCameraPosition,  // 카메라 위치를 향함
	FacingCameraDistanceBlend  // 카메라 거리 블렌드
};

// 파티클 정렬 모드
enum class EParticleSortMode : uint8
{
	None,
	ViewProjDepth,      // 뷰 투영 깊이
	DistanceToView,     // 뷰까지 거리
	Age_OldestFirst,    // 나이순 (오래된 것 먼저)
	Age_NewestFirst     // 나이순 (최신 것 먼저)
};

// SubUV 보간 방식
enum class EParticleSubUVInterpMethod : uint8
{
	None,
	Linear,
	Random,
	RandomBlend
};

// 이미터 노멀 모드
enum class EEmitterNormalsMode : uint8
{
	CameraFacing,   // 카메라를 향함
	Spherical,      // 구형
	Cylindrical     // 원통형
};

// 버스트 생성 방식
enum class EParticleBurstMethod : uint8
{
	Instant,        // 즉시
	Interpolated    // 보간
};

/**
 * @brief 파티클 버스트 데이터
 * @details 특정 시간에 한 번에 생성할 파티클 정보
 *
 * @param Count 생성할 파티클 수
 * @param CountLow 최소 생성 수 (랜덤 범위)
 * @param Time 버스트 발생 시간
 */
struct FParticleBurst
{
	int32 Count;
	int32 CountLow;
	float Time;

	FParticleBurst()
		: Count(0)
		, CountLow(-1)
		, Time(0.0f)
	{
	}

	FParticleBurst(int32 InCount, float InTime, int32 InCountLow = -1)
		: Count(InCount)
		, CountLow(InCountLow)
		, Time(InTime)
	{
	}
};

/**
 * @brief 간단한 Float 분포 (Distribution)
 * @details Min~Max 범위의 랜덤 값 또는 상수 값 제공
 *
 * @param Min 최소값
 * @param Max 최대값
 * @param bIsUniform Min과 Max가 같은지 여부
 */
struct FFloatDistribution
{
	float Min;
	float Max;
	bool bIsUniform;

	FFloatDistribution()
		: Min(0.0f)
		, Max(0.0f)
		, bIsUniform(true)
	{
	}

	FFloatDistribution(float InValue)
		: Min(InValue)
		, Max(InValue)
		, bIsUniform(true)
	{
	}

	FFloatDistribution(float InMin, float InMax)
		: Min(InMin)
		, Max(InMax)
		, bIsUniform(InMin == InMax)
	{
	}

	float GetValue() const
	{
		if (bIsUniform)
		{
			return Min;
		}
		// 간단한 랜덤 (나중에 더 나은 랜덤으로 교체 가능)
		float Alpha = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return Min + Alpha * (Max - Min);
	}
};

/**
 * @brief 간단한 Vector 분포 (Distribution)
 * @details Min~Max 범위의 랜덤 벡터 또는 상수 벡터 제공
 *
 * @param Min 최소 벡터
 * @param Max 최대 벡터
 * @param bIsUniform Min과 Max가 같은지 여부
 */
struct FVectorDistribution
{
	FVector Min;
	FVector Max;
	bool bIsUniform;

	FVectorDistribution()
		: Min(FVector::Zero())
		, Max(FVector::Zero())
		, bIsUniform(true)
	{
	}

	FVectorDistribution(const FVector& InValue)
		: Min(InValue)
		, Max(InValue)
		, bIsUniform(true)
	{
	}

	FVectorDistribution(const FVector& InMin, const FVector& InMax)
		: Min(InMin)
		, Max(InMax)
		, bIsUniform(InMin == InMax)
	{
	}

	FVector GetValue() const
	{
		if (bIsUniform)
		{
			return Min;
		}
		float AlphaX = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float AlphaY = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float AlphaZ = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return {
			Min.X + AlphaX * (Max.X - Min.X),
			Min.Y + AlphaY * (Max.Y - Min.Y),
			Min.Z + AlphaZ * (Max.Z - Min.Z)
		};
	}
};

/**
 * @brief 간단한 Color 분포 (Distribution)
 * @details Min~Max 범위의 랜덤 색상 또는 상수 색상 제공
 *
 * @param Min 최소 색상
 * @param Max 최대 색상
 * @param bIsUniform Min과 Max가 같은지 여부
 */
struct FColorDistribution
{
	FLinearColor Min;
	FLinearColor Max;
	bool bIsUniform;

	FColorDistribution()
		: Min(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		, Max(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		, bIsUniform(true)
	{
	}

	FColorDistribution(const FLinearColor& InValue)
		: Min(InValue)
		, Max(InValue)
		, bIsUniform(true)
	{
	}

	FColorDistribution(const FLinearColor& InMin, const FLinearColor& InMax)
		: Min(InMin)
		, Max(InMax)
		, bIsUniform(InMin == InMax)
	{
	}

	FLinearColor GetValue() const
	{
		if (bIsUniform)
		{
			return Min;
		}
		float Alpha = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return FLinearColor(
			Min.R + Alpha * (Max.R - Min.R),
			Min.G + Alpha * (Max.G - Min.G),
			Min.B + Alpha * (Max.B - Min.B),
			Min.A + Alpha * (Max.A - Min.A)
		);
	}
};
