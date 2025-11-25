#pragma once

/**
 * @brief 파티클 시스템에서 사용하는 열거형 및 기본 타입 정의
 */

 /*-----------------------------------------------------------------------------
	 Particle State Flags
 -----------------------------------------------------------------------------*/
enum EParticleStates
{
	/** Ignore updates to the particle						*/
	STATE_Particle_JustSpawned = 0x02000000,
	/** Ignore updates to the particle						*/
	STATE_Particle_Freeze = 0x04000000,
	/** Ignore collision updates to the particle			*/
	STATE_Particle_IgnoreCollisions = 0x08000000,
	/**	Stop translations of the particle					*/
	STATE_Particle_FreezeTranslation = 0x10000000,
	/**	Stop rotations of the particle						*/
	STATE_Particle_FreezeRotation = 0x20000000,
	/** Combination for a single check of 'ignore' flags	*/
	STATE_Particle_CollisionIgnoreCheck = STATE_Particle_Freeze | STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation,
	/** Delay collision updates to the particle				*/
	STATE_Particle_DelayCollisions = 0x40000000,
	/** Flag indicating the particle has had at least one collision	*/
	STATE_Particle_CollisionHasOccurred = 0x80000000,
	/** State mask. */
	STATE_Mask = 0xFE000000,
	/** Counter mask. */
	STATE_CounterMask = (~STATE_Mask)
};

// 동적 이미터 타입
enum class EDynamicEmitterType : uint8
{
	Unknown = 0,
	Sprite,
	Mesh,
	Beam2,
	Ribbon,
	AnimTrail,
	Custom
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

// 파티클 블렌드 모드
enum class EParticleBlendMode : uint8
{
	None,           // 불투명
	Translucent,    // 반투명 (알파 블렌딩) - 먼지, 연기 등
	Additive        // 가산 블렌딩 - 불, 발광 효과 등
};

// 이미터 렌더링 모드
enum class EEmitterRenderMode : uint8
{
	Normal,         // 일반 렌더링
	Point,          // 포인트 렌더링
	Cross,          // 십자가 렌더링
	None            // 렌더링 안 함
};

// 파티클 시스템 LOD 방식
enum class EParticleSystemLODMethod : uint8
{
	Automatic,          // 자동 (거리 기반)
	DirectSet,          // 직접 설정
	ActivateAutomatic   // 활성화 시 자동
};

// 파티클 시스템 업데이트 모드
enum class EParticleSystemUpdateMode : uint8
{
	RealTime,       // 실시간
	FixedTime       // 고정 시간
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

// Particle을 렌더링하기 위한 모든 정보
struct FParticleDynamicData
{
	TArray<struct FDynamicEmitterDataBase*> DynamicEmitterDataArray;
};

//=============================================================================
// Distribution 직렬화 헬퍼
//=============================================================================

/**
 * @brief FFloatDistribution JSON 직렬화
 */
inline JSON FloatDistributionToJson(const FFloatDistribution& Dist)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	Obj["Min"] = Dist.Min;
	Obj["Max"] = Dist.Max;
	Obj["bIsUniform"] = Dist.bIsUniform;
	return Obj;
}

inline FFloatDistribution JsonToFloatDistribution(JSON& Obj)
{
	FFloatDistribution Dist;
	if (Obj.hasKey("Min")) Dist.Min = static_cast<float>(Obj["Min"].ToFloat());
	if (Obj.hasKey("Max")) Dist.Max = static_cast<float>(Obj["Max"].ToFloat());
	if (Obj.hasKey("bIsUniform")) Dist.bIsUniform = Obj["bIsUniform"].ToBool();
	return Dist;
}

/**
 * @brief FVectorDistribution JSON 직렬화
 */
inline JSON VectorDistributionToJson(const FVectorDistribution& Dist)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	JSON MinArr = JSON::Make(JSON::Class::Array);
	MinArr.append(Dist.Min.X, Dist.Min.Y, Dist.Min.Z);
	JSON MaxArr = JSON::Make(JSON::Class::Array);
	MaxArr.append(Dist.Max.X, Dist.Max.Y, Dist.Max.Z);
	Obj["Min"] = MinArr;
	Obj["Max"] = MaxArr;
	Obj["bIsUniform"] = Dist.bIsUniform;
	return Obj;
}

inline FVectorDistribution JsonToVectorDistribution(JSON& Obj)
{
	FVectorDistribution Dist;
	if (Obj.hasKey("Min") && Obj["Min"].JSONType() == JSON::Class::Array && Obj["Min"].size() == 3)
	{
		Dist.Min.X = static_cast<float>(Obj["Min"][0].ToFloat());
		Dist.Min.Y = static_cast<float>(Obj["Min"][1].ToFloat());
		Dist.Min.Z = static_cast<float>(Obj["Min"][2].ToFloat());
	}
	if (Obj.hasKey("Max") && Obj["Max"].JSONType() == JSON::Class::Array && Obj["Max"].size() == 3)
	{
		Dist.Max.X = static_cast<float>(Obj["Max"][0].ToFloat());
		Dist.Max.Y = static_cast<float>(Obj["Max"][1].ToFloat());
		Dist.Max.Z = static_cast<float>(Obj["Max"][2].ToFloat());
	}
	if (Obj.hasKey("bIsUniform")) Dist.bIsUniform = Obj["bIsUniform"].ToBool();
	return Dist;
}

/**
 * @brief FColorDistribution JSON 직렬화
 */
inline JSON ColorDistributionToJson(const FColorDistribution& Dist)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	JSON MinArr = JSON::Make(JSON::Class::Array);
	MinArr.append(Dist.Min.R, Dist.Min.G, Dist.Min.B, Dist.Min.A);
	JSON MaxArr = JSON::Make(JSON::Class::Array);
	MaxArr.append(Dist.Max.R, Dist.Max.G, Dist.Max.B, Dist.Max.A);
	Obj["Min"] = MinArr;
	Obj["Max"] = MaxArr;
	Obj["bIsUniform"] = Dist.bIsUniform;
	return Obj;
}

inline FColorDistribution JsonToColorDistribution(JSON& Obj)
{
	FColorDistribution Dist;
	if (Obj.hasKey("Min") && Obj["Min"].JSONType() == JSON::Class::Array && Obj["Min"].size() == 4)
	{
		Dist.Min.R = static_cast<float>(Obj["Min"][0].ToFloat());
		Dist.Min.G = static_cast<float>(Obj["Min"][1].ToFloat());
		Dist.Min.B = static_cast<float>(Obj["Min"][2].ToFloat());
		Dist.Min.A = static_cast<float>(Obj["Min"][3].ToFloat());
	}
	if (Obj.hasKey("Max") && Obj["Max"].JSONType() == JSON::Class::Array && Obj["Max"].size() == 4)
	{
		Dist.Max.R = static_cast<float>(Obj["Max"][0].ToFloat());
		Dist.Max.G = static_cast<float>(Obj["Max"][1].ToFloat());
		Dist.Max.B = static_cast<float>(Obj["Max"][2].ToFloat());
		Dist.Max.A = static_cast<float>(Obj["Max"][3].ToFloat());
	}
	if (Obj.hasKey("bIsUniform")) Dist.bIsUniform = Obj["bIsUniform"].ToBool();
	return Dist;
}

/**
 * @brief FParticleBurst JSON 직렬화
 */
inline JSON ParticleBurstToJson(const FParticleBurst& Burst)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	Obj["Count"] = Burst.Count;
	Obj["CountLow"] = Burst.CountLow;
	Obj["Time"] = Burst.Time;
	return Obj;
}

inline FParticleBurst JsonToParticleBurst(JSON& Obj)
{
	FParticleBurst Burst;
	if (Obj.hasKey("Count")) Burst.Count = static_cast<int32>(Obj["Count"].ToInt());
	if (Obj.hasKey("CountLow")) Burst.CountLow = static_cast<int32>(Obj["CountLow"].ToInt());
	if (Obj.hasKey("Time")) Burst.Time = static_cast<float>(Obj["Time"].ToFloat());
	return Burst;
}
