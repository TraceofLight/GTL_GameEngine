#include "pch.h"
#include "ParticleBeamEmitterInstance.h"
#include "DynamicEmitterDataBase.h"
#include "DynamicEmitterReplayDataBase.h"
#include "ParticleSystemComponent.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"
#include "TypeData/ParticleModuleTypeDataBase.h"

// ============== 생성자/소멸자 ==============

FParticleBeamEmitterInstance::FParticleBeamEmitterInstance()
	: FParticleEmitterInstance()
	, BeamTypeData(nullptr)
	, BeamVertexBuffer(nullptr)
	, BeamIndexBuffer(nullptr)
{
}

FParticleBeamEmitterInstance::~FParticleBeamEmitterInstance()
{
	if (BeamVertexBuffer)
	{
		BeamVertexBuffer->Release();
		BeamVertexBuffer = nullptr;
	}

	if (BeamIndexBuffer)
	{
		BeamIndexBuffer->Release();
		BeamIndexBuffer = nullptr;
	}
}

// ============== Init ==============

void FParticleBeamEmitterInstance::Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	// 부모 Init 호출
	FParticleEmitterInstance::Init(InTemplate, InComponent);

	// TypeDataModule에서 BeamTypeData 가져오기
	if (CurrentLODLevel && CurrentLODLevel->TypeDataModule)
	{
		BeamTypeData = Cast<UParticleModuleTypeDataBeam>(CurrentLODLevel->TypeDataModule);
	}

	// GPU 버퍼 생성 (빔 버텍스용)
	if (Component && BeamTypeData)
	{
		ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
		if (Device)
		{
			// 최대 버텍스 수: Segments * 2 * Sheets
			const int32 MaxSegments = BeamTypeData->Segments;
			const int32 MaxSheets = BeamTypeData->Sheets;
			const int32 MaxVertices = (MaxSegments + 1) * 2 * MaxSheets;

			// Vertex Buffer
			D3D11_BUFFER_DESC VBDesc = {};
			VBDesc.ByteWidth = sizeof(FParticleBeamVertex) * MaxVertices;
			VBDesc.Usage = D3D11_USAGE_DYNAMIC;
			VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			HRESULT hr = Device->CreateBuffer(&VBDesc, nullptr, &BeamVertexBuffer);
			if (FAILED(hr))
			{
				UE_LOG("Failed to create beam vertex buffer");
			}

			// Index Buffer
			const int32 MaxIndices = MaxSegments * 6 * MaxSheets;  // 2 triangles per segment
			D3D11_BUFFER_DESC IBDesc = {};
			IBDesc.ByteWidth = sizeof(uint32) * MaxIndices;
			IBDesc.Usage = D3D11_USAGE_DYNAMIC;
			IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			IBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			hr = Device->CreateBuffer(&IBDesc, nullptr, &BeamIndexBuffer);
			if (FAILED(hr))
			{
				UE_LOG("Failed to create beam index buffer");
			}
		}
	}

	// 초기 빔 포인트 계산
	CalculateBeamPoints();
}

// ============== Tick ==============

void FParticleBeamEmitterInstance::Tick(float DeltaTime)
{
	// 빔은 위치가 고정이므로 기본 파티클 물리 시뮬레이션은 스킵
	// 대신 빔 포인트만 업데이트

	// Component 위치가 변경되었을 수 있으므로 빔 포인트 재계산
	CalculateBeamPoints();

	// 빔은 항상 살아있음 (ActiveParticles 관리 안 함)
	// 노이즈 애니메이션은 추후 구현
}

// ============== IsDynamicDataRequired ==============

bool FParticleBeamEmitterInstance::IsDynamicDataRequired() const
{
	// BeamTypeData 유효성 체크
	if (!BeamTypeData)
	{
		return false;
	}

	// 빔 포인트가 최소 2개 이상 있어야 함
	if (BeamPoints.Num() < 2)
	{
		return false;
	}

	// CurrentLODLevel 체크
	return CurrentLODLevel != nullptr;
}

// ============== GetDynamicData ==============

FDynamicEmitterDataBase* FParticleBeamEmitterInstance::GetDynamicData(bool bSelected)
{
	// 매 프레임 빔 포인트 재계산 (TypeDataBeam 값 변경 즉시 반영)
	CalculateBeamPoints();

	if (!IsDynamicDataRequired())
	{
		return nullptr;
	}

	// FDynamicBeamEmitterData 생성
	FDynamicBeamEmitterData* NewEmitterData = new FDynamicBeamEmitterData();

	// Source 데이터 채우기
	if (!FillReplayData(NewEmitterData->Source))
	{
		delete NewEmitterData;
		return nullptr;
	}

	// Init
	NewEmitterData->Init(bSelected);
	NewEmitterData->OwnerInstance = this;

	return NewEmitterData;
}

// ============== FillReplayData ==============

bool FParticleBeamEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	// FDynamicBeamEmitterReplayData로 캐스팅
	FDynamicBeamEmitterReplayData& BeamData = static_cast<FDynamicBeamEmitterReplayData&>(OutData);

	// 에미터 타입 설정
	BeamData.eEmitterType = EDynamicEmitterType::Beam2;

	// 머티리얼 설정
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		BeamData.MaterialInterface = CurrentLODLevel->RequiredModule->GetMaterial();
		BeamData.RequiredModule = CurrentLODLevel->RequiredModule->CreateRendererResource();
		BeamData.BlendMode = CurrentLODLevel->RequiredModule->GetBlendMode();
	}

	// 빔 포인트 복사
	BeamData.BeamPoints = BeamPoints;

	// 빔 설정
	if (BeamTypeData)
	{
		BeamData.Sheets = BeamTypeData->Sheets;
		BeamData.UVTiling = BeamTypeData->UVTiling;
	}
	else
	{
		BeamData.Sheets = 1;
		BeamData.UVTiling = 1.0f;
	}

	return true;
}

// ============== GetReplayData ==============

FDynamicEmitterReplayDataBase* FParticleBeamEmitterInstance::GetReplayData()
{
	if (BeamPoints.Num() < 2)
	{
		return nullptr;
	}

	FDynamicBeamEmitterReplayData* NewData = new FDynamicBeamEmitterReplayData();

	if (!FillReplayData(*NewData))
	{
		delete NewData;
		return nullptr;
	}

	return NewData;
}

// ============== GetAllocatedSize ==============

void FParticleBeamEmitterInstance::GetAllocatedSize(int32& OutNum, int32& OutMax)
{
	int32 Size = sizeof(FParticleBeamEmitterInstance);
	int32 BeamPointsSize = BeamPoints.Num() * sizeof(FBeamPoint);

	OutNum = Size + BeamPointsSize;
	OutMax = Size + BeamPointsSize;
}

// ============== CalculateBeamPoints ==============

void FParticleBeamEmitterInstance::CalculateBeamPoints()
{
	if (!BeamTypeData || !Component)
	{
		return;
	}

	const int32 NumSegments = BeamTypeData->Segments;
	const int32 NumPoints = NumSegments + 1;

	// 빔 포인트 배열 리사이즈
	BeamPoints.SetNum(NumPoints);

	// 월드 공간 Source/Target 위치
	FVector SourcePos = GetSourcePosition();
	FVector TargetPos = GetTargetPosition();

	// 빔 방향 (정규화)
	FVector BeamDirection = (TargetPos - SourcePos);
	float BeamLength = BeamDirection.Size();
	if (BeamLength > 0.0f)
	{
		BeamDirection = BeamDirection / BeamLength;
	}
	else
	{
		BeamDirection = FVector(1.0f, 0.0f, 0.0f);
	}

	// 빔 폭 가져오기
	float BaseWidth = BeamTypeData->BeamWidth.GetValue();

	// 각 포인트 계산
	for (int32 i = 0; i < NumPoints; ++i)
	{
		float Parameter = static_cast<float>(i) / static_cast<float>(NumSegments);

		FBeamPoint& Point = BeamPoints[i];

		// 선형 보간으로 위치 계산
		Point.Position = FMath::Lerp(SourcePos, TargetPos, Parameter);
		Point.Tangent = BeamDirection;
		Point.Parameter = Parameter;

		// Taper 적용
		if (BeamTypeData->bTaperBeam)
		{
			float TaperScale = FMath::Lerp(1.0f, BeamTypeData->TaperFactor, Parameter);
			Point.Width = BaseWidth * TaperScale;
		}
		else
		{
			Point.Width = BaseWidth;
		}

		// 색상 (나중에 모듈에서 설정 가능)
		Point.Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

// ============== GetSourcePosition ==============

FVector FParticleBeamEmitterInstance::GetSourcePosition() const
{
	if (!Component || !BeamTypeData)
	{
		return FVector::Zero();
	}

	// 에미터 월드 위치 + Source 오프셋
	return Component->GetWorldLocation() + BeamTypeData->SourceOffset;
}

// ============== GetTargetPosition ==============

FVector FParticleBeamEmitterInstance::GetTargetPosition() const
{
	if (!Component || !BeamTypeData)
	{
		return FVector(100.0f, 0.0f, 0.0f);
	}

	// 에미터 월드 위치 + Target 오프셋
	return Component->GetWorldLocation() + BeamTypeData->TargetOffset;
}
