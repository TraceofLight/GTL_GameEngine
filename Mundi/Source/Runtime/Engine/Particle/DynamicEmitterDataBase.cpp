#include "pch.h"
#include "DynamicEmitterDataBase.h"

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
