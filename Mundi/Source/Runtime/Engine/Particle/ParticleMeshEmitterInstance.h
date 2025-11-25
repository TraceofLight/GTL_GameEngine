#pragma once
#include "ParticleEmitterInstance.h"

// Forward declarations
class UStaticMesh;
class UParticleModuleTypeDataMesh;
struct FDynamicEmitterDataBase;
struct FDynamicEmitterReplayDataBase;

/**
 * 메시 파티클 에미터 인스턴스
 *
 * @note GPU 인스턴싱을 통해 메시 파티클을 효율적으로 렌더링
 * @note 스프라이트 파티클과 달리 각 파티클이 3D 메시로 렌더링됨
 */
struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
	// ============== 멤버 변수 ==============

	/** TypeData 모듈 참조 (메시 에셋 및 설정 포함) */
	// FillReplayData에서 메시 에셋(MeshTypeData->Mesh, MeshTypeData->MeshAligned)과 정렬 방식을 가져와야 하기 때문에 필요
	UParticleModuleTypeDataMesh* MeshTypeData;

	/** GPU 인스턴싱용 인스턴스 버퍼 */
	ID3D11Buffer* InstanceBuffer;

	// ============== 생성자/소멸자 ==============

	FParticleMeshEmitterInstance();
	virtual ~FParticleMeshEmitterInstance();

	// ============== 핵심 오버라이드 ==============

	/**
	 * 에미터 인스턴스 초기화
	 *
	 * @param InTemplate - 사용할 에미터 템플릿
	 * @param InComponent - 소유자 컴포넌트
	 */
	virtual void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;

	/**
	 * 렌더링용 동적 데이터 가져오기
	 *
	 * @param bSelected - 에디터에서 선택되었는지 여부
	 * @return FDynamicMeshEmitterData* 또는 nullptr
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;

	/**
	 * 리플레이 데이터 가져오기
	 *
	 * @return FDynamicMeshEmitterReplayData* 또는 nullptr
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData() override;

	/**
	 * 할당된 메모리 크기 반환
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax) override;

protected:
	/**
	 * 리플레이 데이터 채우기
	 *
	 * @param OutData - 채울 리플레이 데이터
	 * @return 성공 여부
	 */
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};
