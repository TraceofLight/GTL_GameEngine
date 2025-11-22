#pragma once
#include "ParticleTypes.h"
#include "ParticleDataContainer.h"

/**
 * Dynamic particle emitter types
 *
 * NOTE: These are serialized out for particle replay data, so be sure to update all appropriate
 *    when changing anything here.
 */

struct FDynamicEmitterReplayDataBase
{
	/** The type of emitter. */
	EDynamicEmitterType eEmitterType;

	/** The number of particles currently active in this emitter. */
	int32 ActiveParticleCount;

	int32 ParticleStride; // BaseParticle 크기 + payload 크기 + 정렬 패딩
	FParticleDataContainer DataContainer;

	FVector Scale;

	EParticleSortMode SortMode; // EParticleSortMode와 대응

	/** MacroUV (override) data **/
	/*FMacroUVOverride MacroUVOverride;*/

	/** Constructor */
	FDynamicEmitterReplayDataBase()
		: eEmitterType(EDynamicEmitterType::Unknown),
		ActiveParticleCount(0),
		ParticleStride(0),
		Scale(FVector(1.0f)),
		SortMode(EParticleSortMode::None)	// Default to PSORTMODE_None		  
	{
	}

	virtual ~FDynamicEmitterReplayDataBase()
	{
	}

	/** Serialization */
	virtual void Serialize(FArchive& Ar);
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterialInterface* MaterialInterface;
	struct FParticleRequiredModule* RequiredModule;

	FVector						NormalsSphereCenter;
	FVector						NormalsCylinderDirection;
	float						InvDeltaSeconds;
	//FVector						LWCTile;
	int32						MaxDrawCount;
	int32						OrbitModuleOffset;
	int32						DynamicParameterDataOffset;
	int32						LightDataOffset;
	float						LightVolumetricScatteringIntensity;
	int32						CameraPayloadOffset;
	int32						SubUVDataOffset;
	int32						SubImages_Horizontal;
	int32						SubImages_Vertical;
	bool						bUseLocalSpace;
	bool						bLockAxis;
	uint8						ScreenAlignment;
	uint8						LockAxisFlag;
	EEmitterRenderMode			EmitterRenderMode;
	EEmitterNormalsMode			EmitterNormalsMode;
	FVector2D					PivotOffset;
	bool						bUseVelocityForMotionBlur;
	//bool						bRemoveHMDRoll;
	float						MinFacingCameraBlendDistance;
	float						MaxFacingCameraBlendDistance;

	/** Constructor */
	FDynamicSpriteEmitterReplayDataBase()
		: MaterialInterface(nullptr)
		, RequiredModule(nullptr)
		, NormalsSphereCenter(FVector::Zero())
		, NormalsCylinderDirection(FVector::Zero())
		, InvDeltaSeconds(0.0f)
		, MaxDrawCount(0)
		, OrbitModuleOffset(0)
		, DynamicParameterDataOffset(0)
		, LightDataOffset(0)
		, LightVolumetricScatteringIntensity(0)
		, CameraPayloadOffset(0)
		, SubUVDataOffset(0)
		, SubImages_Horizontal(1)
		, SubImages_Vertical(1)
		, bUseLocalSpace(false)
		, bLockAxis(false)
		, ScreenAlignment(0)
		, LockAxisFlag(0)
		, EmitterRenderMode(EEmitterRenderMode::Normal)
		, EmitterNormalsMode(EEmitterNormalsMode::CameraFacing)
		, PivotOffset(-0.5f, -0.5f)
		, bUseVelocityForMotionBlur(false)
		//, bRemoveHMDRoll(false)
		, MinFacingCameraBlendDistance(0.f)
		, MaxFacingCameraBlendDistance(0.f)
	{
	}
	~FDynamicSpriteEmitterReplayDataBase()
	{
		delete RequiredModule;
	}

	/** Serialization */
	virtual void Serialize(FArchive& Ar);
};
