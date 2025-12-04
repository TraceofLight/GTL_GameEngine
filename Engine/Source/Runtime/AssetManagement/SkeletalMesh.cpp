#include "pch.h"
#include "SkeletalMesh.h"


#include "FbxLoader.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "PathUtils.h"
#include <filesystem>
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAssetUtils.h"

IMPLEMENT_CLASS(USkeletalMesh)

USkeletalMesh::USkeletalMesh()
{
}

USkeletalMesh::~USkeletalMesh()
{
    ReleaseResources();
}

void USkeletalMesh::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    if (Data)
    {
        ReleaseResources();
    }

    // FBXLoader가 캐싱을 내부적으로 처리합니다
    Data = UFbxLoader::GetInstance().LoadFbxMeshAsset(InFilePath);

    if (!Data || Data->Vertices.empty() || Data->Indices.empty())
    {
        UE_LOG("ERROR: Failed to load FBX mesh from '%s'", InFilePath.c_str());
        return;
    }

    // GPU 버퍼 생성
    CreateVertexBuffer(Data, InDevice);
    CreateIndexBuffer(Data, InDevice);
    CreateLocalBound(Data);
    VertexCount = static_cast<uint32>(Data->Vertices.size());
    IndexCount = static_cast<uint32>(Data->Indices.size());
    VertexStride = sizeof(FSkinnedVertexDynamic);
}

void USkeletalMesh::ReleaseResources()
{
    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }

    if (IndexBuffer)
    {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }

    if (Data)
    {
        delete Data;
        Data = nullptr;
    }
}

void USkeletalMesh::CreateVertexBufferForComp(ID3D11Buffer** InVertexBuffer)
{
    if (!Data || Data->Vertices.empty())
    {
        return;
    }
    ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
    HRESULT hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(Device, Data->Vertices, InVertexBuffer, false);
    if (FAILED(hr))
    {
        UE_LOG("SkeletalMesh: CreateVertexBufferForComp failed, hr=0x%08X", hr);
    }
}

void USkeletalMesh::UpdateVertexBuffer(const TArray<FNormalVertex>& SkinnedVertices, ID3D11Buffer* InVertexBuffer)
{
    if (!InVertexBuffer) { return; }

    GEngine.GetRHIDevice()->VertexBufferUpdate(InVertexBuffer, SkinnedVertices);
}

void USkeletalMesh::CreateVertexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateVertexBuffer<FSkinnedVertexDynamic>(InDevice, Data->Vertices, &VertexBuffer, true);
	assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateIndexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InSkeletalMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateLocalBound(const FSkeletalMeshData* InSkeletalMesh)
{
    if (!InSkeletalMesh || InSkeletalMesh->Vertices.empty())
    {
        LocalBound = FAABB();
        return;
    }

    const TArray<FSkinnedVertex>& Verts = InSkeletalMesh->Vertices;
    FVector Min = Verts[0].Position;
    FVector Max = Verts[0].Position;

    for (const FSkinnedVertex& Vertex : Verts)
    {
        Min = Min.ComponentMin(Vertex.Position);
        Max = Max.ComponentMax(Vertex.Position);
    }

    LocalBound = FAABB(Min, Max);
}

UPhysicsAsset* USkeletalMesh::GetPhysicsAsset()
{
    // Return existing physics asset if already created
    if (PhysicsAsset)
        return PhysicsAsset;

    // Need skeleton to create physics asset
    if (!GetSkeleton())
        return nullptr;

    // Auto-generate physics asset from skeleton
    PhysicsAsset = NewObject<UPhysicsAsset>();
    if (PhysicsAsset)
    {
        FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset, this);
        UE_LOG("SkeletalMesh: BuildPhysicsAsset: %d bodies for %s",
               PhysicsAsset->BodySetups.Num(), GetPathFileName().c_str());
    }

    return PhysicsAsset;
}
