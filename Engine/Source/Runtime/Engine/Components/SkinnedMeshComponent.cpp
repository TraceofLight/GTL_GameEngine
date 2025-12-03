#include "pch.h"
#include "SkinnedMeshComponent.h"
#include "MeshBatchElement.h"
#include "PlatformTime.h"
#include "SceneView.h"

USkinnedMeshComponent::USkinnedMeshComponent() : SkeletalMesh(nullptr)
{
   bCanEverTick = true;
}

USkinnedMeshComponent::~USkinnedMeshComponent()
{
   if (VertexBuffer)
   {
      VertexBuffer->Release();
      VertexBuffer = nullptr;
   }
}

void USkinnedMeshComponent::BeginPlay()
{
   Super::BeginPlay();
}

void USkinnedMeshComponent::TickComponent(float DeltaTime)
{
   UMeshComponent::TickComponent(DeltaTime);
}

void USkinnedMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
   // SkeletalMeshPath를 먼저 처리 (비동기 로드 중에도 경로가 유지되도록)
   if (bInIsLoading)
   {
      // 로드 시: SkeletalMeshPath 읽기
      FJsonSerializer::ReadString(InOutHandle, "SkeletalMeshPath", SkeletalMeshPath);
   }
   else
   {
      // 저장 시: SkeletalMeshPath 저장 (메시가 로드 중이어도 경로 유지)
      if (!SkeletalMeshPath.empty())
      {
         InOutHandle["SkeletalMeshPath"] = SkeletalMeshPath.c_str();
      }
      else if (SkeletalMesh)
      {
         InOutHandle["SkeletalMeshPath"] = SkeletalMesh->GetPathFileName().c_str();
      }
   }

   Super::Serialize(bInIsLoading, InOutHandle);

   if (bInIsLoading)
   {
      // 로드 시: 저장된 경로로 메시 로드
      if (!SkeletalMeshPath.empty())
      {
         SetSkeletalMesh(SkeletalMeshPath);
      }
      else if (SkeletalMesh)
      {
         // 이전 버전 호환: SkeletalMesh 프로퍼티에서 경로 가져오기
         SetSkeletalMesh(SkeletalMesh->GetPathFileName());
      }
   }
}

void USkinnedMeshComponent::DuplicateSubObjects()
{
   Super::DuplicateSubObjects();
   if (SkeletalMesh)
   {
      SkeletalMesh->CreateVertexBufferForComp(&VertexBuffer);
   }
}

void USkinnedMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData()) { return; }

	bool bIsGPUSkinning = View->RenderSettings->GetSkinningMode() == ESkinningMode::GPU;

	if (!bIsGPUSkinning && bSkinningMatricesDirty)
	{
		TIME_PROFILE(SKINNING_CPU_TASK)

		PerformSkinning();
		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, VertexBuffer);
		bSkinningMatricesDirty = false;
	}

    const TArray<FGroupInfo>& MeshGroupInfos = SkeletalMesh->GetMeshGroupInfo();
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
          // UE_LOG("USkinnedMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
          Material = UResourceManager::GetInstance().GetDefaultMaterial();
          if (Material)
          {
             Shader = Material->GetShader();
          }
          if (!Material || !Shader)
          {
             UE_LOG("USkinnedMeshComponent: 기본 머티리얼이 없습니다.");
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
          IndexCount = Group.IndexCount;
          StartIndex = Group.StartIndex;
       }
       else
       {
          IndexCount = SkeletalMesh->GetIndexCount();
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
       TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
       if (0 < MaterialToUse->GetShaderMacros().Num())
       {
          ShaderMacros.Append(MaterialToUse->GetShaderMacros());
       }

    	if (bIsGPUSkinning)
    	{
    		ShaderMacros.Append({{"GPU_SKINNING", "1"}});
    		BatchElement.SkinningMatrices = &FinalSkinningMatrices;
    	}

       FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

       if (ShaderVariant)
       {
          BatchElement.VertexShader = ShaderVariant->VertexShader;
          BatchElement.PixelShader = ShaderVariant->PixelShader;
          BatchElement.InputLayout = ShaderVariant->InputLayout;
       }

       BatchElement.Material = MaterialToUse;

       BatchElement.VertexBuffer = bIsGPUSkinning ? SkeletalMesh->GetVertexBuffer() : VertexBuffer;
       BatchElement.IndexBuffer = SkeletalMesh->GetIndexBuffer();
       BatchElement.VertexStride = bIsGPUSkinning ? SkeletalMesh->GetVertexStride() : sizeof(FVertexDynamic);

       BatchElement.IndexCount = IndexCount;
       BatchElement.StartIndex = StartIndex;
       BatchElement.BaseVertexIndex = 0;
       BatchElement.WorldMatrix = GetWorldMatrix();
       BatchElement.ObjectID = InternalIndex;
       BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

       OutMeshBatchElements.Add(BatchElement);
    }
}

FAABB USkinnedMeshComponent::GetWorldAABB() const
{
   const FTransform WorldTransform = GetWorldTransform();
   const FMatrix WorldMatrix = GetWorldMatrix();

   if (!SkeletalMesh)
   {
      const FVector Origin = WorldTransform.TransformPosition(FVector());
      return FAABB(Origin, Origin);
   }

   const FAABB LocalBound = SkeletalMesh->GetLocalBound();
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
      const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X,
         LocalCorners[CornerIndex].Y,
         LocalCorners[CornerIndex].Z,
         1.0f) * WorldMatrix;
      WorldMin4 = WorldMin4.ComponentMin(WorldPos);
      WorldMax4 = WorldMax4.ComponentMax(WorldPos);
   }

   FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
   FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
   return FAABB(WorldMin, WorldMax);
}

void USkinnedMeshComponent::OnTransformUpdated()
{
   Super::OnTransformUpdated();
   MarkWorldPartitionDirty();
}

void USkinnedMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
   ClearDynamicMaterials();

   if (VertexBuffer)
   {
      VertexBuffer->Release();
      VertexBuffer = nullptr;
   }

   auto& RM = UResourceManager::GetInstance();

   // 경로 저장 (비동기 로드 중에도 Details 패널에서 확인 가능)
   SkeletalMeshPath = PathFileName;

   // 로드될 때까지 null
   SkeletalMesh = nullptr;

   RM.AsyncLoad<USkeletalMesh>(PathFileName, [this, PathFileName](USkeletalMesh* LoadedMesh)
   {
      // 비동기 로드 완료 전에 컴포넌트가 파괴되었을 수 있음
      if (!IsValidObject(this))
      {
         return;
      }

      if (LoadedMesh && LoadedMesh->GetSkeletalMeshData())
      {
         this->SkeletalMesh = LoadedMesh;

         if (this->VertexBuffer)
         {
            this->VertexBuffer->Release();
            this->VertexBuffer = nullptr;
         }
         LoadedMesh->CreateVertexBufferForComp(&this->VertexBuffer);

         const TArray<FMatrix> IdentityMatrices(LoadedMesh->GetBoneCount(), FMatrix::Identity());
         this->UpdateSkinningMatrices(IdentityMatrices, IdentityMatrices);

         const TArray<FGroupInfo>& GroupInfos = LoadedMesh->GetMeshGroupInfo();
         this->MaterialSlots.resize(GroupInfos.size());
         for (size_t i = 0; i < GroupInfos.size(); ++i)
         {
            this->SetMaterialByName(static_cast<int32>(i), GroupInfos[i].InitialMaterialName);
         }

         this->MarkWorldPartitionDirty();
      }
      else
      {
         UE_LOG("[warning] SkinnedMeshComponent: Failed to load %s", PathFileName.c_str());
      }
   }, EAssetLoadPriority::Normal);
}

void USkinnedMeshComponent::PerformSkinning()
{
   if (!SkeletalMesh || FinalSkinningMatrices.IsEmpty()) { return; }

   const TArray<FSkinnedVertex>& SrcVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
   const int32 NumVertices = SrcVertices.Num();
   SkinnedVertices.SetNum(NumVertices);

   for (int32 Idx = 0; Idx < NumVertices; ++Idx)
   {
      const FSkinnedVertex& SrcVert = SrcVertices[Idx];
      FNormalVertex& DstVert = SkinnedVertices[Idx];

      DstVert.pos = SkinVertexPosition(SrcVert);
      DstVert.normal = SkinVertexNormal(SrcVert);
      DstVert.Tangent = SkinVertexTangent(SrcVert);
      DstVert.tex = SrcVert.UV;
   }
}

void USkinnedMeshComponent::UpdateSkinningMatrices(const TArray<FMatrix>& InSkinningMatrices, const TArray<FMatrix>& InSkinningNormalMatrices)
{
   FinalSkinningMatrices = InSkinningMatrices;
   FinalSkinningNormalMatrices = InSkinningNormalMatrices;
   bSkinningMatricesDirty = true;
}

FVector USkinnedMeshComponent::SkinVertexPosition(const FSkinnedVertex& InVertex) const
{
   FVector BlendedPosition(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         const FMatrix& SkinMatrix = FinalSkinningMatrices[BoneIndex];
         FVector TransformedPosition = SkinMatrix.TransformPosition(InVertex.Position);
         BlendedPosition += TransformedPosition * Weight;
      }
   }

   return BlendedPosition;
}

FVector USkinnedMeshComponent::SkinVertexNormal(const FSkinnedVertex& InVertex) const
{
   FVector BlendedNormal(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         const FMatrix& SkinMatrix = FinalSkinningNormalMatrices[BoneIndex];
         FVector TransformedNormal = SkinMatrix.TransformVector(InVertex.Normal);
         BlendedNormal += TransformedNormal * Weight;
      }
   }

   return BlendedNormal.GetSafeNormal();
}

FVector4 USkinnedMeshComponent::SkinVertexTangent(const FSkinnedVertex& InVertex) const
{
   const FVector OriginalTangentDir(InVertex.Tangent.X, InVertex.Tangent.Y, InVertex.Tangent.Z);
   const float OriginalSignW = InVertex.Tangent.W;

   FVector BlendedTangentDir(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         const FMatrix& SkinMatrix = FinalSkinningMatrices[BoneIndex];
         FVector TransformedTangentDir = SkinMatrix.TransformVector(OriginalTangentDir);
         BlendedTangentDir += TransformedTangentDir * Weight;
      }
   }

   const FVector FinalTangentDir = BlendedTangentDir.GetSafeNormal();
   return { FinalTangentDir.X, FinalTangentDir.Y, FinalTangentDir.Z, OriginalSignW };
}
