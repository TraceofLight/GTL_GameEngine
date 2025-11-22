#include "pch.h"
#include "FDynamicEmitterDataBase.h"

#include "FDynamicEmitterReplayDataBase.h"
#include "ParticleTypes.h"
#include "VertexData.h"

int32 FDynamicSpriteEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

int32 FDynamicMeshEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FMeshParticleInstanceVertex);
}
