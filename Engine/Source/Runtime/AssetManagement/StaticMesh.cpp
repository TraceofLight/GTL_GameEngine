#include "pch.h"
#include "StaticMesh.h"
#include "StaticMeshComponent.h"
#include "ObjManager.h"
#include "ResourceManager.h"
#include "FBXLoader.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/BoxElem.h"
#include <filesystem>

IMPLEMENT_CLASS(UStaticMesh)

namespace
{
    // FSkeletalMeshData를 FStaticMesh로 변환 (본 정보 제거)
    FStaticMesh* ConvertSkeletalToStaticMesh(const FSkeletalMeshData& SkeletalData)
    {
        FStaticMesh* StaticMesh = new FStaticMesh();

        // 정점 데이터 변환 (FSkinnedVertex -> FNormalVertex)
        StaticMesh->Vertices.reserve(SkeletalData.Vertices.size());
        for (const auto& SkinnedVtx : SkeletalData.Vertices)
        {
            FNormalVertex NormalVtx;
            NormalVtx.pos = SkinnedVtx.Position;
            NormalVtx.normal = SkinnedVtx.Normal;
            NormalVtx.tex = SkinnedVtx.UV;
            NormalVtx.Tangent = SkinnedVtx.Tangent;
            NormalVtx.color = SkinnedVtx.Color;
            StaticMesh->Vertices.push_back(NormalVtx);
        }

        // 인덱스 복사
        StaticMesh->Indices = SkeletalData.Indices;

        // 그룹 정보 복사
        StaticMesh->GroupInfos = SkeletalData.GroupInfos;
        StaticMesh->bHasMaterial = SkeletalData.bHasMaterial;

        // 캐시 경로 복사
        StaticMesh->CacheFilePath = SkeletalData.CacheFilePath;

        return StaticMesh;
    }
}

UStaticMesh::~UStaticMesh()
{
    ReleaseResources();
}

void UStaticMesh::EnsureBodySetupBuilt()
{
    if (BodySetup)
        return;

    BodySetup = NewObject<UBodySetup>();
    if (!BodySetup)
        return;

    // If we have mesh data, prepare for convex mesh cooking (mesh-accurate collision)
    // Actual cooking is deferred until PhysX is ready (in EnsureCooked)
    if (StaticMeshAsset && !StaticMeshAsset->Vertices.empty() && !StaticMeshAsset->Indices.empty())
    {
        // Extract vertex positions for cooking
        BodySetup->CookSourceVertices.Empty();
        BodySetup->CookSourceVertices.Reserve(StaticMeshAsset->Vertices.size());
        for (const FNormalVertex& V : StaticMeshAsset->Vertices)
        {
            BodySetup->CookSourceVertices.Add(V.pos);
        }

        // Copy indices
        BodySetup->CookSourceIndices.Empty();
        BodySetup->CookSourceIndices.Reserve(StaticMeshAsset->Indices.size());
        for (uint32 Idx : StaticMeshAsset->Indices)
        {
            BodySetup->CookSourceIndices.Add(Idx);
        }

        // Use convex mesh for dynamic actors (approximate but works with dynamics)
        // Change to UseSimpleAndComplex for exact triangle mesh (static only)
        BodySetup->CollisionTraceFlag = ECollisionTraceFlag::UseComplexAsSimple;

        // Also add a fallback box in case cooking fails later
        const FAABB& LB = GetLocalBound();
        const FVector Center = LB.GetCenter();
        const FVector Half = LB.GetHalfExtent();

        FKBoxElem Box;
        Box.Center = Center;
        Box.Rotation = FVector(0, 0, 0);
        Box.X = Half.X;
        Box.Y = Half.Y;
        Box.Z = Half.Z;
        BodySetup->AggGeom.BoxElems.Add(Box);

        UE_LOG("[StaticMesh] Prepared collision data for mesh: %s (%zu verts, cooking deferred)",
               GetAssetPathFileName().c_str(), BodySetup->CookSourceVertices.Num());
    }
    else
    {
        // No mesh data available, use simple box from bounds
        const FAABB& LB = GetLocalBound();
        const FVector Center = LB.GetCenter();
        const FVector Half = LB.GetHalfExtent();

        FKBoxElem Box;
        Box.Center = Center;
        Box.Rotation = FVector(0, 0, 0);
        Box.X = Half.X;
        Box.Y = Half.Y;
        Box.Z = Half.Z;
        BodySetup->AggGeom.BoxElems.Add(Box);
    }
}

void UStaticMesh::Load(const FString& InFilePath, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    assert(InDevice);

    SetVertexType(InVertexType);

    // 파일 확장자 확인
    std::filesystem::path FilePath(InFilePath);
    FString Extension = FilePath.extension().string();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

    if (Extension == ".fbx")
    {
        // FBX 파일 로드
        FSkeletalMeshData* SkeletalData = UFbxLoader::GetInstance().LoadFbxMeshAsset(InFilePath);

        if (SkeletalData->Vertices.empty() || SkeletalData->Indices.empty())
        {
            UE_LOG("ERROR: Failed to load FBX mesh from '%s'", InFilePath.c_str());
        	delete SkeletalData;
        	SkeletalData = nullptr;
            return;
        }

        // SkeletalMeshData를 StaticMesh로 변환
        StaticMeshAsset = ConvertSkeletalToStaticMesh(*SkeletalData);
        StaticMeshAsset->PathFileName = InFilePath;

        // FBX 메시를 ObjManager 캐시에 등록 (메모리 관리)
        FObjManager::RegisterStaticMeshAsset(InFilePath, StaticMeshAsset);
        delete SkeletalData;
    }
    else
    {
        // OBJ 파일 로드 (기존 방식)
        StaticMeshAsset = FObjManager::LoadObjStaticMeshAsset(InFilePath);
    }

    // 빈 버텍스, 인덱스로 버퍼 생성 방지
    if (StaticMeshAsset && 0 < StaticMeshAsset->Vertices.size() && 0 < StaticMeshAsset->Indices.size())
    {
        CacheFilePath = StaticMeshAsset->CacheFilePath;
        CreateVertexBuffer(StaticMeshAsset, InDevice, InVertexType);
        CreateIndexBuffer(StaticMeshAsset, InDevice);
        CreateLocalBound(StaticMeshAsset);
        VertexCount = static_cast<uint32>(StaticMeshAsset->Vertices.size());
        IndexCount = static_cast<uint32>(StaticMeshAsset->Indices.size());

        // Build default collision once we have a bound
        EnsureBodySetupBuilt();
    }
}

void UStaticMesh::Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    SetVertexType(InVertexType);

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

    CreateVertexBuffer(InData, InDevice, InVertexType);
    CreateIndexBuffer(InData, InDevice);
    CreateLocalBound(InData);

    VertexCount = static_cast<uint32>(InData->Vertices.size());
    IndexCount = static_cast<uint32>(InData->Indices.size());
}

void UStaticMesh::SetVertexType(EVertexLayoutType InVertexType)
{
    VertexType = InVertexType;

    uint32 Stride = 0;
    switch (InVertexType)
    {
    case EVertexLayoutType::PositionColor:
        Stride = sizeof(FVertexSimple);
        break;
    case EVertexLayoutType::PositionColorTexturNormal:
        Stride = sizeof(FVertexDynamic);
        break;
    case EVertexLayoutType::PositionTextBillBoard:
        Stride = sizeof(FBillboardVertexInfo_GPU);
        break;
    case EVertexLayoutType::PositionBillBoard:
        Stride = sizeof(FBillboardVertex);
        break;
    default:
        assert(false && "Unknown vertex type!");
    }

    VertexStride = Stride;
}

void UStaticMesh::CreateVertexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    HRESULT hr;
    hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(InDevice, *InMeshData, &VertexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::CreateVertexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice, EVertexLayoutType InVertexType)
{
    HRESULT hr;
    hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(InDevice, InStaticMesh->Vertices, &VertexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::CreateIndexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InMeshData, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::CreateIndexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InStaticMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void UStaticMesh::CreateLocalBound(const FMeshData* InMeshData)
{
    TArray<FVector> Verts = InMeshData->Vertices;
    FVector Min = Verts[0];
    FVector Max = Verts[0];
    for (FVector Vertex : Verts)
    {
        Min = Min.ComponentMin(Vertex);
        Max = Max.ComponentMax(Vertex);
    }
    LocalBound = FAABB(Min, Max);
}

void UStaticMesh::CreateLocalBound(const FStaticMesh* InStaticMesh)
{
    TArray<FNormalVertex> Verts = InStaticMesh->Vertices;
    FVector Min = Verts[0].pos;
    FVector Max = Verts[0].pos;
    for (FNormalVertex Vertex : Verts)
    {
        FVector Pos = Vertex.pos;
        Min = Min.ComponentMin(Pos);
        Max = Max.ComponentMax(Pos);
    }
    LocalBound = FAABB(Min, Max);
}

void UStaticMesh::ReleaseResources()
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
}
