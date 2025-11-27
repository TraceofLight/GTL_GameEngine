#pragma once

class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleColor;
class UParticleModuleLifetime;
class UParticleModuleLocation;
class UParticleModuleSize;
class UParticleModuleVelocity;
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataBeam;
class UParticleModuleBeamSource;
class UParticleModuleBeamTarget;
class UParticleModuleBeamNoise;
class UParticleModuleRotation;
class UParticleModuleRotationRate;
class UParticleModuleMeshRotation;
class UParticleModuleMeshRotationRate;
class UParticleModuleSubUV;
class UParticleEmitter;

/**
 * @brief 파티클 모듈 디테일 렌더러
 * @details 파티클 모듈의 프로퍼티를 ImGui로 렌더링
 *          Cascade 스타일의 섹션별 구성
 */
class UParticleModuleDetailRenderer
{
public:
	// 모듈 타입에 따라 적절한 UI 렌더링
	static void RenderModuleDetails(UParticleModule* Module);

	// Emitter 전체 프로퍼티 렌더링 (모듈이 선택되지 않았을 때)
	static void RenderEmitterDetails(UParticleEmitter* Emitter);

	// 디테일 패널에서 프로퍼티가 변경되었는지 여부 (커브 에디터 동기화용)
	static bool bPropertyChanged;

private:
	// 각 모듈 타입별 렌더링
	static void RenderRequiredModule(UParticleModuleRequired* Module);
	static void RenderSpawnModule(UParticleModuleSpawn* Module);
	static void RenderColorModule(UParticleModuleColor* Module);
	static void RenderLifetimeModule(UParticleModuleLifetime* Module);
	static void RenderLocationModule(UParticleModuleLocation* Module);
	static void RenderSizeModule(UParticleModuleSize* Module);
	static void RenderVelocityModule(UParticleModuleVelocity* Module);
	static void RenderTypeDataMeshModule(UParticleModuleTypeDataMesh* Module);
	static void RenderTypeDataBeamModule(UParticleModuleTypeDataBeam* Module);
	static void RenderBeamSourceModule(UParticleModuleBeamSource* Module);
	static void RenderBeamTargetModule(UParticleModuleBeamTarget* Module);
	static void RenderBeamNoiseModule(UParticleModuleBeamNoise* Module);
	static void RenderRotationModule(UParticleModuleRotation* Module);
	static void RenderRotationRateModule(UParticleModuleRotationRate* Module);
	static void RenderMeshRotationModule(UParticleModuleMeshRotation* Module);
	static void RenderMeshRotationRateModule(UParticleModuleMeshRotationRate* Module);
	static void RenderSubUVModule(UParticleModuleSubUV* Module);

	// Distribution 타입 UI 헬퍼
	static bool RenderFloatDistribution(const char* Label, struct FFloatDistribution& Dist);
	static bool RenderVectorDistribution(const char* Label, struct FVectorDistribution& Dist);
	static bool RenderColorDistribution(const char* Label, struct FColorDistribution& Dist);

	// Enum 콤보박스 헬퍼
	static bool RenderScreenAlignmentCombo(const char* Label, enum class EParticleScreenAlignment& Value);
	static bool RenderSortModeCombo(const char* Label, enum class EParticleSortMode& Value);
	static bool RenderBlendModeCombo(const char* Label, enum class EParticleBlendMode& Value);
	static bool RenderBurstMethodCombo(const char* Label, enum class EParticleBurstMethod& Value);
	static bool RenderSubUVInterpMethodCombo(const char* Label, enum class EParticleSubUVInterpMethod& Value);

	// 섹션 헬퍼
	static bool BeginSection(const char* Label, bool bDefaultOpen = true);
	static void EndSection();
};
