#pragma once
#include "UEContainer.h"

class FDynamicEmitterReplayDataBase;

/**
 * @brief 이미터 렌더 데이터의 기본 구조체
 * @details 렌더링 스레드로 전달되는 파티클 이미터 데이터
 *
 * @param bValid 유효한 데이터인지 여부
 * @param EmitterIndex 이미터 인덱스
 */
struct FDynamicEmitterDataBase
{
	bool bValid;
	int32 EmitterIndex;
	//...

	FDynamicEmitterDataBase()
		: bValid(false)
		, EmitterIndex(0)
	{
	}

	virtual ~FDynamicEmitterDataBase()
	{
	}

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	//...
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	void SortSpriteParticles(...);
	virtual int32 GetDynamicVertexStride() const = 0;
	//...
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	virtual int32 GetDynamicVertexStride() const override;

	//...
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	virtual int32 GetDynamicVertexStride() const override;
	//...
};
