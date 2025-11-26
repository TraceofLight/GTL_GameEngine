#include "pch.h"
#include "DynamicEmitterDataBase.h"

#include "ParticleTypes.h"
#include "VertexData.h"
#include "ParticleHelper.h"
#include "SceneView.h"
#include "ParticleEmitterInstance.h"
#include "ParticleMeshEmitterInstance.h"
#include "ParticleBeamEmitterInstance.h"
#include "Source/Runtime/AssetManagement/StaticMesh.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/Renderer/Material.h"

void FDynamicSpriteEmitterDataBase::SortSpriteParticles(EParticleSortMode SortMode, bool bLocalSpace,
	int32 ParticleCount, const uint8* ParticleData, int32 ParticleStride, const uint16* ParticleIndices,
	const FSceneView* View, const FMatrix& LocalToWorld, FParticleOrder* ParticleOrder) const
{

	if (SortMode == EParticleSortMode::ViewProjDepth)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			float InZ;
			if (bLocalSpace)
			{
				InZ = View->GetViewProjectionMatrix().TransformPositionVector4(LocalToWorld.TransformPosition(Particle.Location)).W;
			}
			else
			{
				InZ = View->GetViewProjectionMatrix().TransformPositionVector4(Particle.Location).W;
			}
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;

			ParticleOrder[ParticleIndex].Z = InZ;
		}
		std::sort(ParticleOrder, ParticleOrder + ParticleCount, [](const FParticleOrder& A, const FParticleOrder& B)
		{
			return A.Z > B.Z;
		}); // 내림차순 정렬
	}
	else if (SortMode == EParticleSortMode::DistanceToView)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			float InZ;
			FVector Position;
			if (bLocalSpace)
			{
				Position = LocalToWorld.TransformPosition(Particle.Location);
			}
			else
			{
				Position = Particle.Location;
			}
			InZ = (View->ViewLocation - Position).SizeSquared();
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].Z = InZ;
		}
		std::sort(ParticleOrder, ParticleOrder + ParticleCount, [](const FParticleOrder& A, const FParticleOrder& B)
		{
			return A.Z > B.Z;
		}); // 내림차순 정렬
	}
	else if (SortMode == EParticleSortMode::Age_OldestFirst)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].C = Particle.Flags & STATE_CounterMask;
		}
		std::sort(ParticleOrder, ParticleOrder + ParticleCount, [](const FParticleOrder& A, const FParticleOrder& B)
		{
			return A.C > B.C;
		}); // 내림차순 정렬
	}
	else if (SortMode == EParticleSortMode::Age_NewestFirst)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].C = (~Particle.Flags) & STATE_CounterMask;
		}
		std::sort(ParticleOrder, ParticleOrder + ParticleCount, [](const FParticleOrder& A, const FParticleOrder& B)
		{
			return A.C > B.C;
		}); // 내림차순 정렬
	}
}

int32 FDynamicSpriteEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

void FDynamicSpriteEmitterData::GetDynamicMeshElementsEmitter(
	TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) const
{
	// 1. 유효성 검사
	if (!bValid || Source.ActiveParticleCount <= 0)
	{
		return;
	}

	// 2. Source 데이터 가져오기
	const FDynamicSpriteEmitterReplayDataBase& SourceData = Source;
	const int32 ParticleCount = SourceData.ActiveParticleCount;
	const uint8* ParticleData = SourceData.DataContainer.ParticleData;
	const uint16* ParticleIndices = SourceData.DataContainer.ParticleIndices;
	const int32 ParticleStride = SourceData.ParticleStride;

	if (!ParticleData || !ParticleIndices)
	{
		return;
	}

	// 3. 파티클 정렬 (필요한 경우)
	TArray<FParticleOrder> ParticleOrder;
	ParticleOrder.reserve(ParticleCount);

	// 기본 순서로 초기화
	for (int32 i = 0; i < ParticleCount; ++i)
	{
		ParticleOrder.Add(FParticleOrder(i, 0.0f));
	}

	// 정렬 모드에 따라 정렬 수행
	if (SourceData.SortMode != EParticleSortMode::None)
	{
		FMatrix LocalToWorld = FMatrix::Identity(); // TODO: Get actual LocalToWorld from component
		bool bLocalSpace = false; // TODO: Get from emitter settings
		SortSpriteParticles(SourceData.SortMode, bLocalSpace, ParticleCount, 
			ParticleData, ParticleStride, ParticleIndices, View, LocalToWorld, ParticleOrder.GetData());
	}

	// 4. 정점 데이터 생성 (각 파티클당 4개의 정점)
	const int32 VertexCount = ParticleCount * 4;
	const int32 IndexCount = ParticleCount * 6; // 2 triangles per quad

	TArray<FParticleSpriteVertex> Vertices;
	Vertices.reserve(VertexCount);

	TArray<uint32> Indices;
	Indices.reserve(IndexCount);

	// 5. 각 파티클에 대해 쿼드 생성
	for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
	{
		// 정렬된 인덱스 사용
		// 주의: FillReplayData에서 ActiveParticles만큼만 연속으로 복사했으므로
		// ParticleIndices를 사용하지 않고 SortedParticleIndex를 직접 사용해야 함
		const int32 SortedParticleIndex = ParticleOrder[ParticleIndex].ParticleIndex;
		const int32 CurrentIndex = ParticleIndices[SortedParticleIndex];
		const uint8* ParticlePtr = ParticleData + (CurrentIndex * ParticleStride);
		//const uint8* ParticlePtr = ParticleData + (SortedParticleIndex * ParticleStride);
		const FBaseParticle& Particle = *reinterpret_cast<const FBaseParticle*>(ParticlePtr);

		// 정점 인덱스 계산
		const int32 BaseVertexIndex = ParticleIndex * 4;

		// UV 좌표 (쿼드의 4개 코너)
		const FVector2D UVs[4] = {
			FVector2D(0.0f, 0.0f), // 좌상단
			FVector2D(1.0f, 0.0f), // 우상단
			FVector2D(0.0f, 1.0f), // 좌하단
			FVector2D(1.0f, 1.0f)  // 우하단
		};

		// 각 코너에 대해 정점 생성
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			FParticleSpriteVertex Vertex;

			// 파티클 기본 정보 복사
			Vertex.Position = Particle.Location;
			Vertex.OldPosition = Particle.OldLocation;
			Vertex.RelativeTime = Particle.RelativeTime;
			Vertex.ParticleId = static_cast<float>(SortedParticleIndex);
			Vertex.Size = FVector2D(Particle.Size.X, Particle.Size.Y);
			Vertex.Rotation = Particle.Rotation;
			Vertex.SubImageIndex = 0.0f; // TODO: SubUV support
			Vertex.Color = Particle.Color;
			Vertex.TexCoord = UVs[CornerIndex];

			Vertices.Add(Vertex);
		}

		// 인덱스 생성 (2개의 삼각형)
		Indices.Add(BaseVertexIndex + 0); // Triangle 1
		Indices.Add(BaseVertexIndex + 1);
		Indices.Add(BaseVertexIndex + 2);
		Indices.Add(BaseVertexIndex + 2); // Triangle 2
		Indices.Add(BaseVertexIndex + 1);
		Indices.Add(BaseVertexIndex + 3);
	}

	// 6. GPU 버퍼 업데이트 (동적 버퍼)
	// VertexBuffer 업데이트 (OwnerInstance의 버퍼에 새로 생성한 정점 데이터를 업데이트)
	if (OwnerInstance && OwnerInstance->VertexBuffer && !Vertices.empty())
	{
		GEngine.GetRHIDevice()->VertexBufferUpdate(OwnerInstance->VertexBuffer, Vertices);
	}
	
	// 7. FMeshBatchElement 생성
	FMeshBatchElement BatchElement;

	// 머티리얼과 셰이더는 렌더러에서 설정됨 (RenderParticlesPass)
	// 여기서는 기하 데이터만 설정
	
	BatchElement.VertexBuffer = OwnerInstance ? OwnerInstance->VertexBuffer : nullptr;
	BatchElement.IndexBuffer = OwnerInstance ? OwnerInstance->IndexBuffer : nullptr;

	BatchElement.VertexStride = sizeof(FParticleSpriteVertex);
	BatchElement.IndexCount = IndexCount;
	BatchElement.StartIndex = 0;
	BatchElement.BaseVertexIndex = 0;
	BatchElement.WorldMatrix = FMatrix::Identity(); // TODO: Get from component transform

	assert(OwnerInstance != nullptr);
	assert(OwnerInstance->Component != nullptr);
	BatchElement.ObjectID = OwnerInstance->Component->UUID; 
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 머티리얼 설정
	if (SourceData.MaterialInterface)
	{
		BatchElement.Material = SourceData.MaterialInterface;
	}
	//BatchElement.Material = nullptr; // for test

	// 출력 배열에 추가
	OutMeshBatchElements.Add(BatchElement);
}

int32 FDynamicMeshEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FMeshParticleInstanceVertex);
}

void FDynamicMeshEmitterData::GetDynamicMeshElementsEmitter(
	TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) const
{
	// 1. 유효성 검사
	if (!bValid || Source.ActiveParticleCount <= 0 || !Source.MeshAsset)
	{
		return;
	}

	// 2. Source 데이터 가져오기
	const FDynamicMeshEmitterReplayData& SourceData = Source;
	const int32 ParticleCount = SourceData.ActiveParticleCount;
	const uint8* ParticleData = SourceData.DataContainer.ParticleData;
	const uint16* ParticleIndices = SourceData.DataContainer.ParticleIndices;
	const int32 ParticleStride = SourceData.ParticleStride;

	if (!ParticleData || !ParticleIndices)
	{
		return;
	}

	// 3. 인스턴스 데이터 생성 (각 파티클당 1개의 인스턴스)
	TArray<FMeshParticleInstanceVertex> InstanceData;
	InstanceData.reserve(ParticleCount);

	for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
	{
		const int32 CurrentIndex = ParticleIndices[ParticleIndex];
		const uint8* ParticlePtr = ParticleData + (CurrentIndex * ParticleStride);
		const FBaseParticle& Particle = *reinterpret_cast<const FBaseParticle*>(ParticlePtr);

		FMeshParticleInstanceVertex Instance;

		// 색상
		Instance.Color = Particle.Color;

		// Transform 행렬 구성 (Scale * Rotation * Translation)
		// FVector4 Transform[3]에 3x4 행렬 저장 (W에 Translation)
		FVector Scale = Particle.Size;
		FVector Location = Particle.Location;

		// Payload에서 3D 회전 읽기
		FVector MeshRotation = FVector::Zero();
		if (SourceData.bMeshRotationActive && SourceData.MeshRotationOffset > 0)
		{
			const FMeshRotationPayloadData* MeshRotPayload =
				reinterpret_cast<const FMeshRotationPayloadData*>(ParticlePtr + SourceData.MeshRotationOffset);
			MeshRotation = MeshRotPayload->Rotation;
		}

		// 회전 행렬 계산 (라디안 → 도 변환 후 쿼터니언)
		FVector RotDeg(RadiansToDegrees(MeshRotation.X),
		               RadiansToDegrees(MeshRotation.Y),
		               RadiansToDegrees(MeshRotation.Z));
		FQuat RotQuat = FQuat::MakeFromEulerZYX(RotDeg);
		FMatrix RotMatrix = RotQuat.ToMatrix();

		// Scale * Rotation 행렬 구성 후 Translation 적용
		// Row 0: Scale.X * RotMatrix Row 0, Location.X
		Instance.Transform[0] = FVector4(
			RotMatrix.M[0][0] * Scale.X, RotMatrix.M[0][1] * Scale.Y, RotMatrix.M[0][2] * Scale.Z, Location.X);
		// Row 1: Scale.Y * RotMatrix Row 1, Location.Y
		Instance.Transform[1] = FVector4(
			RotMatrix.M[1][0] * Scale.X, RotMatrix.M[1][1] * Scale.Y, RotMatrix.M[1][2] * Scale.Z, Location.Y);
		// Row 2: Scale.Z * RotMatrix Row 2, Location.Z
		Instance.Transform[2] = FVector4(
			RotMatrix.M[2][0] * Scale.X, RotMatrix.M[2][1] * Scale.Y, RotMatrix.M[2][2] * Scale.Z, Location.Z);

		// Velocity
		FVector Velocity = Particle.Velocity;
		float Speed = Velocity.Size();
		Instance.Velocity = FVector4(Velocity.X, Velocity.Y, Velocity.Z, Speed);

		// SubUV (기본값)
		Instance.SubUVParams[0] = 0;
		Instance.SubUVParams[1] = 0;
		Instance.SubUVParams[2] = 0;
		Instance.SubUVParams[3] = 0;
		Instance.SubUVLerp = 0.0f;

		// RelativeTime
		Instance.RelativeTime = Particle.RelativeTime;

		InstanceData.Add(Instance);
	}

	// 4. MeshEmitterInstance로 캐스팅 (InstanceBuffer 접근용)
	FParticleMeshEmitterInstance* MeshInstance =
		static_cast<FParticleMeshEmitterInstance*>(OwnerInstance);

	// 5. 인스턴스 버퍼 업데이트
	if (MeshInstance && MeshInstance->InstanceBuffer && !InstanceData.empty())
	{
		GEngine.GetRHIDevice()->VertexBufferUpdate(MeshInstance->InstanceBuffer, InstanceData);
	}

	// 6. 메시 에셋 정보 가져오기
	UStaticMesh* MeshAsset = SourceData.MeshAsset;
	if (!MeshAsset || !MeshAsset->GetStaticMeshAsset())
	{
		return;
	}

	const TArray<FGroupInfo>& MeshGroupInfos = MeshAsset->GetMeshGroupInfo();

	// 7. 그룹별로 FMeshBatchElement 생성 (UStaticMeshComponent::CollectMeshBatches 로직 참고)
	const bool bHasSections = !MeshGroupInfos.IsEmpty();
	const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

	for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
	{
		uint32 IndexCount = 0;
		uint32 StartIndex = 0;
		UMaterialInterface* SectionMaterial = SourceData.MaterialInterface; // 기본 머티리얼

		if (bHasSections)
		{
			const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
			IndexCount = Group.IndexCount;
			StartIndex = Group.StartIndex;

			// 그룹별 머티리얼 로드 (있는 경우)
			if (!Group.InitialMaterialName.empty())
			{
				UMaterialInterface* GroupMaterial = UResourceManager::GetInstance().Load<UMaterial>(Group.InitialMaterialName);
				if (GroupMaterial)
				{
					SectionMaterial = GroupMaterial;
				}
			}
		}
		else
		{
			IndexCount = MeshAsset->GetIndexCount();
			StartIndex = 0;
		}

		if (IndexCount == 0)
		{
			continue;
		}

		// FMeshBatchElement 생성
		FMeshBatchElement BatchElement;

		// 메시 에셋의 버퍼 사용
		BatchElement.VertexBuffer = MeshAsset->GetVertexBuffer();
		BatchElement.IndexBuffer = MeshAsset->GetIndexBuffer();
		BatchElement.VertexStride = MeshAsset->GetVertexStride();
		BatchElement.IndexCount = IndexCount;
		BatchElement.StartIndex = StartIndex;
		BatchElement.BaseVertexIndex = 0;

		// 인스턴싱 데이터
		BatchElement.InstanceBuffer = MeshInstance ? MeshInstance->InstanceBuffer : nullptr;
		BatchElement.InstanceCount = ParticleCount;
		BatchElement.InstanceStride = sizeof(FMeshParticleInstanceVertex);

		BatchElement.WorldMatrix = FMatrix::Identity();
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		assert(OwnerInstance != nullptr);
		assert(OwnerInstance->Component != nullptr);
		BatchElement.ObjectID = OwnerInstance->Component->UUID;

		// 섹션별 머티리얼 설정
		BatchElement.Material = SectionMaterial;

		OutMeshBatchElements.Add(BatchElement);
	}
}

// ============== FDynamicBeamEmitterData ==============

int32 FDynamicBeamEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleBeamVertex);
}

void FDynamicBeamEmitterData::GetDynamicMeshElementsEmitter(
	TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) const
{
	// 1. 유효성 검사
	if (!bValid || Source.BeamPoints.Num() < 2)
	{
		return;
	}

	// 2. Source 데이터 가져오기
	const FDynamicBeamEmitterReplayData& SourceData = Source;
	const TArray<FBeamPoint>& BeamPoints = SourceData.BeamPoints;
	const int32 NumPoints = BeamPoints.Num();
	const int32 NumSegments = NumPoints - 1;
	const int32 Sheets = FMath::Max(1, SourceData.Sheets);
	const float UVTiling = SourceData.UVTiling;

	// 3. 버텍스 및 인덱스 데이터 생성
	// 각 시트마다: (NumPoints * 2) 버텍스, (NumSegments * 6) 인덱스
	const int32 VertexCountPerSheet = NumPoints * 2;
	const int32 IndexCountPerSheet = NumSegments * 6;
	const int32 TotalVertexCount = VertexCountPerSheet * Sheets;
	const int32 TotalIndexCount = IndexCountPerSheet * Sheets;

	TArray<FParticleBeamVertex> Vertices;
	Vertices.reserve(TotalVertexCount);

	TArray<uint32> Indices;
	Indices.reserve(TotalIndexCount);

	// 카메라 방향 (빌보딩용)
	// ViewRotation에서 Forward 방향 계산
	FVector CameraForward = View->ViewRotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
	FVector CameraLocation = View->ViewLocation;

	// 4. 각 시트에 대해 버텍스 생성
	for (int32 SheetIndex = 0; SheetIndex < Sheets; ++SheetIndex)
	{
		// 시트 회전 각도 (180도씩 분할)
		float SheetAngle = (PI / Sheets) * SheetIndex;

		int32 BaseVertexIndex = Vertices.Num();

		// 각 빔 포인트에 대해 2개의 버텍스 생성 (상단, 하단)
		for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			const FBeamPoint& Point = BeamPoints[PointIndex];

			// 빔 방향 (Tangent)
			FVector BeamDirection = Point.Tangent;

			// 카메라를 향하는 방향 계산
			FVector ToCamera = (CameraLocation - Point.Position);
			ToCamera.Normalize();

			// 빔과 수직이면서 카메라를 향하는 방향
			FVector RightVector = FVector::Cross(BeamDirection, ToCamera);
			if (RightVector.SizeSquared() < 0.0001f)
			{
				// 빔이 카메라를 직접 향하는 경우, 대체 벡터 사용
				RightVector = FVector(0.0f, 1.0f, 0.0f);
			}
			RightVector.Normalize();

			// 시트 회전 적용
			if (SheetIndex > 0)
			{
				// BeamDirection을 축으로 RightVector 회전
				FQuat RotQuat = FQuat::FromAxisAngle(BeamDirection, SheetAngle);
				RightVector = RotQuat.RotateVector(RightVector);
			}

			// 빔 폭의 절반
			float HalfWidth = Point.Width * 0.5f;

			// UV 계산
			float U = Point.Parameter * UVTiling;

			// 상단 버텍스
			FParticleBeamVertex TopVertex;
			TopVertex.Position = Point.Position + RightVector * HalfWidth;
			TopVertex.Color = Point.Color;
			TopVertex.Tex_U = U;
			TopVertex.Tex_V = 0.0f;
			Vertices.Add(TopVertex);

			// 하단 버텍스
			FParticleBeamVertex BottomVertex;
			BottomVertex.Position = Point.Position - RightVector * HalfWidth;
			BottomVertex.Color = Point.Color;
			BottomVertex.Tex_U = U;
			BottomVertex.Tex_V = 1.0f;
			Vertices.Add(BottomVertex);
		}

		// 인덱스 생성 (삼각형 스트립을 삼각형 리스트로 변환)
		for (int32 SegIndex = 0; SegIndex < NumSegments; ++SegIndex)
		{
			int32 V0 = BaseVertexIndex + (SegIndex * 2) + 0; // 현재 상단
			int32 V1 = BaseVertexIndex + (SegIndex * 2) + 1; // 현재 하단
			int32 V2 = BaseVertexIndex + (SegIndex * 2) + 2; // 다음 상단
			int32 V3 = BaseVertexIndex + (SegIndex * 2) + 3; // 다음 하단

			// 삼각형 1: V0 - V2 - V1
			Indices.Add(V0);
			Indices.Add(V2);
			Indices.Add(V1);

			// 삼각형 2: V1 - V2 - V3
			Indices.Add(V1);
			Indices.Add(V2);
			Indices.Add(V3);
		}
	}

	// 5. GPU 버퍼 업데이트
	FParticleBeamEmitterInstance* BeamInstance =
		static_cast<FParticleBeamEmitterInstance*>(OwnerInstance);

	if (BeamInstance && BeamInstance->BeamVertexBuffer && !Vertices.empty())
	{
		ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();
		if (Context)
		{
			// Vertex Buffer 업데이트
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			HRESULT hr = Context->Map(BeamInstance->BeamVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			if (SUCCEEDED(hr))
			{
				memcpy(MappedResource.pData, Vertices.GetData(), Vertices.Num() * sizeof(FParticleBeamVertex));
				Context->Unmap(BeamInstance->BeamVertexBuffer, 0);
			}

			// Index Buffer 업데이트
			hr = Context->Map(BeamInstance->BeamIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			if (SUCCEEDED(hr))
			{
				memcpy(MappedResource.pData, Indices.GetData(), Indices.Num() * sizeof(uint32));
				Context->Unmap(BeamInstance->BeamIndexBuffer, 0);
			}
		}
	}

	// 6. FMeshBatchElement 생성
	FMeshBatchElement BatchElement;

	BatchElement.VertexBuffer = BeamInstance ? BeamInstance->BeamVertexBuffer : nullptr;
	BatchElement.IndexBuffer = BeamInstance ? BeamInstance->BeamIndexBuffer : nullptr;

	BatchElement.VertexStride = sizeof(FParticleBeamVertex);
	BatchElement.IndexCount = TotalIndexCount;
	BatchElement.StartIndex = 0;
	BatchElement.BaseVertexIndex = 0;
	BatchElement.WorldMatrix = FMatrix::Identity();

	assert(OwnerInstance != nullptr);
	assert(OwnerInstance->Component != nullptr);
	BatchElement.ObjectID = OwnerInstance->Component->UUID;
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 머티리얼 설정
	if (SourceData.MaterialInterface)
	{
		BatchElement.Material = SourceData.MaterialInterface;
	}

	OutMeshBatchElements.Add(BatchElement);
}
