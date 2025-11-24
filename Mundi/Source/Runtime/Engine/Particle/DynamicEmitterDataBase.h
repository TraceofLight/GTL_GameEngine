#pragma once
#include "UEContainer.h"
#include "DynamicEmitterReplayDataBase.h"

#include "MeshBatchElement.h"

/**
 * @brief 이미터 렌더 데이터의 기본 구조체
 * @details 렌더링 스레드로 전달되는 파티클 이미터 데이터
 *
 * @param bValid 유효한 데이터인지 여부
 * @param EmitterIndex 이미터 인덱스
 */
struct FDynamicEmitterDataBase
{
	/** true if this emitter is currently selected */
	uint32	bSelected : 1;
	/** true if this emitter has valid rendering data */
	bool bValid;

	int32 EmitterIndex;
	//...

	FDynamicEmitterDataBase()
		: bSelected(false)
		, bValid(false)
		, EmitterIndex(0)
	{
	}

	virtual ~FDynamicEmitterDataBase()
	{
	}

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	//...

	virtual void GetDynamicMeshElementsEmitter(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) const {}
};

class FParticleOrder;
struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	/**
	 *	Sort the given sprite particles
	 *
	 *	@param	SortMode			The sort mode to utilize (EParticleSortMode)
	 *	@param	bLocalSpace			true if the emitter is using local space
	 *	@param	ParticleCount		The number of particles
	 *	@param	ParticleData		The actual particle data
	 *	@param	ParticleStride		The stride between entries in the ParticleData array
	 *	@param	ParticleIndices		Indirect index list into ParticleData
	 *	@param	View				The scene view being rendered
	 *	@param	LocalToWorld		The local to world transform of the component rendering the emitter
	 *	@param	ParticleOrder		The array to fill in with ordered indices
	 */
	void SortSpriteParticles(EParticleSortMode SortMode, bool bLocalSpace,
		int32 ParticleCount, const uint8* ParticleData, int32 ParticleStride, const uint16* ParticleIndices,
		const FSceneView* View, const FMatrix& LocalToWorld, FParticleOrder* ParticleOrder) const;

	virtual int32 GetDynamicVertexStride() const = 0;
	//...

	// 아래는 UE에 존재하지만, 현재 Mundi 엔진에서는 사용하지 않을 것 같은 멤버들
	///** The material render proxy for this emitter */
	//const FMaterialRenderProxy* MaterialResource;
	///** true if the particle emitter utilizes the DynamicParameter module */
	//uint32 bUsesDynamicParameter : 1;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	virtual int32 GetDynamicVertexStride() const override;

	//...

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return Source;
	}

	/**
	 * Initialize the dynamic sprite emitter data
	 * 동적 스프라이트 이미터 데이터 초기화
	 *
	 * @param bInSelected - Whether this emitter is selected in the editor
	 */
	void Init(bool bInSelected)
	{
		bSelected = bInSelected;
		bValid = (Source.ActiveParticleCount > 0);
	}

	virtual void GetDynamicMeshElementsEmitter(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) const override;


	/** The frame source data for this particle system.  This is everything needed to represent this
		this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	// UE에서는 FDynamicSpriteEmitterReplayData지만, 실질적으로 그 타입은 상속만 받고 추가 데이터가 없으므로 그냥 DataBase타입으로 사용
	FDynamicSpriteEmitterReplayDataBase Source;
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	virtual int32 GetDynamicVertexStride() const override;
	//...

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return Source;
	}

	/** The frame source data for this particle system.  This is everything needed to represent this
		this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	FDynamicMeshEmitterReplayData Source;
};
