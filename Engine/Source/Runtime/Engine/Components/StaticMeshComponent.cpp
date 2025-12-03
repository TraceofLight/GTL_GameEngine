#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "JsonSerializer.h"
#include "CameraComponent.h"
#include "MeshBatchElement.h"
#include "Material.h"
#include "SceneView.h"
#include "LuaBindHelpers.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"

UStaticMeshComponent::UStaticMeshComponent() = default;

UStaticMeshComponent::~UStaticMeshComponent() = default;

void UStaticMeshComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	// 에디터에서 컴포넌트를 추가했을 때 기본 Cube 메시 설정
	if (!StaticMesh)
	{
		StaticMesh = UResourceManager::GetInstance().GetDefaultStaticMesh();
		if (StaticMesh && StaticMesh->GetStaticMeshAsset())
		{
			StaticMesh->EnsureBodySetupBuilt();
			const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();
			MaterialSlots.resize(GroupInfos.size());
			MaterialSlotOverrides.resize(GroupInfos.size(), false);

			for (size_t i = 0; i < GroupInfos.size(); ++i)
			{
				SetMaterialByName(static_cast<int32>(i), GroupInfos[i].InitialMaterialName);
			}
		}
	}
}

void UStaticMeshComponent::OnStaticMeshReleased(UStaticMesh* ReleasedMesh)
{
	// TODO : 왜 this가 없는지 추적 필요!
	if (!this || !StaticMesh || StaticMesh != ReleasedMesh)
	{
		return;
	}

	StaticMesh = nullptr;
}

void UStaticMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!StaticMesh || !StaticMesh->GetStaticMeshAsset())
	{
		return;
	}

	const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

	auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
		{
			UMaterialInterface* Material = GetMaterial(SectionIndex);
			UShader* Shader = nullptr;

			if (Material && Material->GetShader())
			{
				Shader = Material->GetShader();
			}
			else
			{
				UE_LOG("UStaticMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
				Material = UResourceManager::GetInstance().GetDefaultMaterial();
				if (Material)
				{
					Shader = Material->GetShader();
				}
				if (!Material || !Shader)
				{
					UE_LOG("UStaticMeshComponent: 기본 머티리얼이 없습니다.");
					return { nullptr, nullptr };
				}
			}
			return { Material, Shader };
		};

	const bool bHasSections = !MeshGroupInfos.IsEmpty();
	const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

	for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
	{
		uint32 IndexCount = 0;
		uint32 StartIndex = 0;

		if (bHasSections)
		{
			const FGroupInfo& Group = MeshGroupInfos[SectionIndex];

			// Cloth 섹션은 ClothComponent에서 처리하므로 스킵
			if (Group.bEnableCloth)
			{
				continue;
			}

			IndexCount = Group.IndexCount;
			StartIndex = Group.StartIndex;
		}
		else
		{
			IndexCount = StaticMesh->GetIndexCount();
			StartIndex = 0;
		}

		if (IndexCount == 0)
		{
			continue;
		}

		auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
		if (!MaterialToUse || !ShaderToUse)
		{
			continue;
		}

		FMeshBatchElement BatchElement;
		// View 모드 전용 매크로와 머티리얼 개인 매크로를 결합한다
		TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
		if (0 < MaterialToUse->GetShaderMacros().Num())
		{
			ShaderMacros.Append(MaterialToUse->GetShaderMacros());
		}
		FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

		if (ShaderVariant)
		{
			BatchElement.VertexShader = ShaderVariant->VertexShader;
			BatchElement.PixelShader = ShaderVariant->PixelShader;
			BatchElement.InputLayout = ShaderVariant->InputLayout;
		}

		// UMaterialInterface를 UMaterial로 캐스팅해야 할 수 있음. 렌더러가 UMaterial을 기대한다면.
		// 지금은 Material.h 구조상 UMaterialInterface에 필요한 정보가 다 있음.
		BatchElement.Material = MaterialToUse;
		BatchElement.VertexBuffer = StaticMesh->GetVertexBuffer();
		BatchElement.IndexBuffer = StaticMesh->GetIndexBuffer();
		BatchElement.VertexStride = StaticMesh->GetVertexStride();
		BatchElement.IndexCount = IndexCount;
		BatchElement.StartIndex = StartIndex;
		BatchElement.BaseVertexIndex = 0;
		BatchElement.WorldMatrix = GetWorldMatrix();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		OutMeshBatchElements.Add(BatchElement);
	}
}

void UStaticMeshComponent::SetStaticMesh(const FString& PathFileName)
{
	ClearDynamicMaterials();

	auto& RM = UResourceManager::GetInstance();

	// Default 메쉬 즉시 표시 → 로드 완료 시 교체
    StaticMesh = RM.GetDefaultStaticMesh();
    if (StaticMesh && StaticMesh->GetStaticMeshAsset())
    {
        StaticMesh->EnsureBodySetupBuilt();
		const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();
		MaterialSlots.resize(GroupInfos.size());
		MaterialSlotOverrides.resize(GroupInfos.size(), false);

		for (size_t i = 0; i < GroupInfos.size(); ++i)
		{
			SetMaterialByName(static_cast<int32>(i), GroupInfos[i].InitialMaterialName);
		}
		MarkWorldPartitionDirty();
	}

	RM.AsyncLoad<UStaticMesh>(PathFileName, [this, PathFileName](UStaticMesh* LoadedMesh)
	{
		// 비동기 로드 완료 전에 컴포넌트가 파괴되었을 수 있음
		if (!IsValidObject(this))
		{
			return;
		}

        if (LoadedMesh && LoadedMesh->GetStaticMeshAsset())
        {
            this->StaticMesh = LoadedMesh;
            this->StaticMesh->EnsureBodySetupBuilt();

			const TArray<FGroupInfo>& GroupInfos = LoadedMesh->GetMeshGroupInfo();
			const size_t NewSize = GroupInfos.size();

			// 기존 오버라이드 플래그 보존
			TArray<bool> OldOverrides = this->MaterialSlotOverrides;

			this->MaterialSlots.resize(NewSize);
			this->MaterialSlotOverrides.resize(NewSize, false);

			for (size_t i = 0; i < NewSize; ++i)
			{
				// 사용자가 오버라이드한 슬롯은 건너뜀
				bool bWasOverridden = (i < OldOverrides.size()) && OldOverrides[i];
				if (!bWasOverridden)
				{
					this->SetMaterialByName(static_cast<int32>(i), GroupInfos[i].InitialMaterialName);
				}
				else
				{
					// 오버라이드 플래그 복원
					this->MaterialSlotOverrides[i] = true;
				}
			}

			this->MarkWorldPartitionDirty();
		}
		else
		{
			UE_LOG("[warning] StaticMeshComponent: Failed to load %s, keeping default cube", PathFileName.c_str());
        }
    }, EAssetLoadPriority::Normal);
}

FAABB UStaticMeshComponent::GetWorldAABB() const
{
	const FTransform WorldTransform = GetWorldTransform();
	const FMatrix WorldMatrix = GetWorldMatrix();

	if (!StaticMesh)
	{
		const FVector Origin = WorldTransform.TransformPosition(FVector());
		return FAABB(Origin, Origin);
	}

	const FAABB LocalBound = StaticMesh->GetLocalBound();
	const FVector LocalMin = LocalBound.Min;
	const FVector LocalMax = LocalBound.Max;

	const FVector LocalCorners[8] = {
		FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
	};

	FVector4 WorldMin4 = FVector4(LocalCorners[0].X, LocalCorners[0].Y, LocalCorners[0].Z, 1.0f) * WorldMatrix;
	FVector4 WorldMax4 = WorldMin4;

	for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
	{
		const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X
			, LocalCorners[CornerIndex].Y
			, LocalCorners[CornerIndex].Z
			, 1.0f)
			* WorldMatrix;
		WorldMin4 = WorldMin4.ComponentMin(WorldPos);
		WorldMax4 = WorldMax4.ComponentMax(WorldPos);
	}

	FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
	FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
	return FAABB(WorldMin, WorldMax);
}

void UStaticMeshComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	MarkWorldPartitionDirty();
}

void UStaticMeshComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	// SMC에서 PhysX(physics) 범위를 정한다. (기본은 AABB의 정보로 Box Collider)

	if (!StaticMesh)
	{
		return;
	}
	if (!PHYSICS.GetPhysics())
	{
		return;
	}

	// Ensure collision data is built
	StaticMesh->EnsureBodySetupBuilt();

	UBodySetup* Setup = StaticMesh->GetBodySetup();
	if (!Setup)
	{
		UE_LOG("Physics: OnCreatePhysicsState: No BodySetup for %s", GetName().c_str());
		return;
	}

	// Now that PhysX is ready, try to cook the mesh if we have source data
	// This is deferred from asset loading time when PhysX wasn't initialized yet
	if (!Setup->HasCookedData() && !Setup->CookSourceVertices.IsEmpty())
	{
		if (Setup->EnsureCooked())
		{
			// Cooking succeeded - clear the fallback box since we have real mesh collision
			Setup->AggGeom.BoxElems.Empty();
			UE_LOG("Physics: OnCreatePhysicsState: Cooked mesh collision for %s", GetName().c_str());
		}
	}

	// Assign BodySetup to BodyInstance and create shapes from it
	BodyInstance.BodySetup = Setup;
	BodyInstance.CreateShapesFromBodySetup();

	if (Setup->HasCookedData())
	{
		UE_LOG("Physics: OnCreatePhysicsState: Using cooked mesh collision for %s", GetName().c_str());
	}
	else
	{
		UE_LOG("Physics: OnCreatePhysicsState: Using simple collision (%d shapes) for %s",
			Setup->GetElementCount(), GetName().c_str());
	}
}

void UStaticMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UStaticMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}

void UStaticMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UStaticMeshComponent::EndPlay()
{
    Super::EndPlay();
}
