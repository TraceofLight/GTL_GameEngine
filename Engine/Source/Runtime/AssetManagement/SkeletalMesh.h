#pragma once
#include "ResourceBase.h"
#include"USkeletalMesh.generated.h"

class UAnimSequence;
class UPhysicsAsset;

class USkeletalMesh : public UResourceBase
{
public:
   // DECLARE_CLASS(USkeletalMesh, UResourceBase)
	GENERATED_REFLECTION_BODY()
    USkeletalMesh();
    virtual ~USkeletalMesh() override;

    void Load(const FString& InFilePath, ID3D11Device* InDevice);

    const FSkeletalMeshData* GetSkeletalMeshData() const { return Data; }
    const FString& GetPathFileName() const { static const FString EmptyString; if (Data) return Data->PathFileName; return EmptyString; }
    const FSkeleton* GetSkeleton() const { return Data ? &Data->Skeleton : nullptr; }
    uint32 GetBoneCount() const { return Data ? Data->Skeleton.Bones.Num() : 0; }

    // Animation 관리
    void AddAnimation(UAnimSequence* Animation)
    {
        // 중복 체크: 이미 추가된 애니메이션이면 무시
        auto It = std::find(Animations.begin(), Animations.end(), Animation);
        if (It == Animations.end())
        {
            Animations.push_back(Animation);
        }
    }
    const TArray<UAnimSequence*>& GetAnimations() const { return Animations; }
    void RemoveAnimation(UAnimSequence* Animation)
    {
        auto It = std::find(Animations.begin(), Animations.end(), Animation);
        if (It != Animations.end())
        {
            Animations.erase(It);
        }
    }
    void ClearAnimations() { Animations.clear(); }
    
    // ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; } // W10 CPU Skinning이라 Component가 VB 소유

    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; } // GPU Skinning 때만 사용, CPU Skinning 시 Comp의 VB 사용
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }

    uint32 GetVertexCount() const { return VertexCount; }
    uint32 GetIndexCount() const { return IndexCount; }

    uint32 GetVertexStride() const { return VertexStride; }

    const TArray<FGroupInfo>& GetMeshGroupInfo() const { static TArray<FGroupInfo> EmptyGroup; return Data ? Data->GroupInfos : EmptyGroup; }
    bool HasMaterial() const { return Data ? Data->bHasMaterial : false; }

    uint64 GetMeshGroupCount() const { return Data ? Data->GroupInfos.size() : 0; }

    void CreateVertexBufferForComp(ID3D11Buffer** InVertexBuffer);
    void UpdateVertexBuffer(const TArray<FNormalVertex>& SkinnedVertices, ID3D11Buffer* InVertexBuffer);

private:
    void CreateVertexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice);
    void CreateIndexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice);
    void ReleaseResources();

private:
    // GPU 리소스
    ID3D11Buffer* VertexBuffer = nullptr; // GPU Skinning 때만 사용, CPU Skinning 시 Comp의 VB 사용
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수
    uint32 VertexStride = 0;

    // CPU 리소스
    FSkeletalMeshData* Data = nullptr;

    // Animations
    TArray<UAnimSequence*> Animations;

    // Physics asset for this skeletal mesh (auto-generated if not set)
    UPhysicsAsset* PhysicsAsset = nullptr;

public:
    // Get or create physics asset for this skeletal mesh
    UPhysicsAsset* GetPhysicsAsset();

    // Set physics asset (for manual assignment)
    void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset) { PhysicsAsset = InPhysicsAsset; }
};
