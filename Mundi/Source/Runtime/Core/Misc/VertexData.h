#pragma once
#include "Archive.h"
#include "Vector.h"
#include "Color.h"

// 직렬화 포맷 (FVertexDynamic와 역할이 달라서 분리됨)
struct FNormalVertex
{
    FVector pos;
    FVector normal;
    FVector2D tex;
    FVector4 Tangent;
    FVector4 color;

    friend FArchive& operator<<(FArchive& Ar, FNormalVertex& Vtx)
    {
        Ar << Vtx.pos;
        Ar << Vtx.normal;
        Ar << Vtx.Tangent;
        Ar << Vtx.color;
        Ar << Vtx.tex;
        return Ar;
    }
};

struct FMeshData
{
    TArray<FVector> Vertices;
    TArray<uint32> Indices;
    TArray<FVector4> Color;
    TArray<FVector2D> UV;
    TArray<FVector> Normal;
};

struct FVertexSimple
{
    FVector Position;
    FVector4 Color;

    void FillFrom(const FMeshData& mesh, size_t i);
    void FillFrom(const FNormalVertex& src);
};

/**
* 파티클용 정점 구조체
*/
struct FParticleSpriteVertex
{
	/** The position of the particle. */
	FVector Position;
	/** The relative time of the particle. */
	float RelativeTime;
	/** The previous position of the particle. */
	FVector	OldPosition;
	/** Value that remains constant over the lifetime of a particle. */
	float ParticleId;
	/** The size of the particle. */
	FVector2D Size;
	/** The rotation of the particle. */
	float Rotation;
	/** The sub-image index for the particle. */
	float SubImageIndex;
	/** The color of the particle. */
	FLinearColor Color;

	FVector2D TexCoord;
};

struct FMeshParticleInstanceVertex
{
	/** The color of the particle. */
	FLinearColor Color;

	/** The instance to world transform of the particle. Translation vector is packed into W components. */
	FVector4 Transform[3];

	/** The velocity of the particle, XYZ: direction, W: speed. */
	FVector4 Velocity;

	/** The sub-image texture offsets for the particle. */
	int16 SubUVParams[4];

	/** The sub-image lerp value for the particle. */
	float SubUVLerp;

	/** The relative time of the particle. */
	float RelativeTime;
};


/**
* 스키닝용 정점 구조체
*/
struct FSkinnedVertex
{
	FVector Position{}; // 정점 위치
	FVector Normal{}; // 법선 벡터
	FVector2D UV{}; // 텍스처 좌표
	FVector4 Tangent{}; // 탄젠트 (w는 binormal 방향)
	FVector4 Color{}; // 정점 컬러
	uint32 BoneIndices[4]{}; // 영향을 주는 본 인덱스 (최대 4개)
	float BoneWeights[4]{}; // 각 본의 가중치 (합이 1.0)

	friend FArchive& operator<<(FArchive& Ar, FSkinnedVertex& Vertex)
	{
		Ar << Vertex.Position;
		Ar << Vertex.Normal;
		Ar << Vertex.UV;
		Ar << Vertex.Tangent;
		Ar << Vertex.Color;

		for (int i = 0; i < 4; ++i)
		{
			Ar << Vertex.BoneIndices[i];
		}

		for (int i = 0; i < 4; ++i)
		{
			Ar << Vertex.BoneWeights[i];
		}

		return Ar;
	}
};

// 런타임 포맷 (FNormalVertex와 역할이 달라서 분리됨)
struct FVertexDynamic
{
    FVector Position;
    FVector Normal;
    FVector2D UV;
    FVector4 Tangent;
    FVector4 Color;

    void FillFrom(const FMeshData& mesh, size_t i);
    void FillFrom(const FNormalVertex& src);
	void FillFrom(const FSkinnedVertex& SkinnedVertex);
};
// 런타임 포맷
struct FSkinnedVertexDynamic
{
    FVector Position;
    FVector Normal;
    FVector2D UV;
    FVector4 Tangent;
    FVector4 Color;
	uint32 BoneIndices[4]{};
	float BoneWeights[4]{};

	void FillFrom(const FSkinnedVertex& SkinnedVertex);
};

// 같은 Position인데 Normal이나 UV가 다른 vertex가 존재할 수 있음, 그래서 SkinnedVertex를 키로 구별해야해서 hash함수 정의함
template <class T>
inline void CombineHash(size_t& InSeed, const T& Vertex)
{
    std::hash<T> Hasher;

    InSeed ^= Hasher(Vertex) + 0x9e3779b9 + (InSeed << 6) + (InSeed >> 2);
}
inline bool operator==(const FSkinnedVertex& Vertex1, const FSkinnedVertex& Vertex2)
{
    if (Vertex1.Position != Vertex2.Position ||
        Vertex1.Normal != Vertex2.Normal ||
        Vertex1.UV != Vertex2.UV ||
        Vertex1.Tangent != Vertex2.Tangent ||
        Vertex1.Color != Vertex2.Color)
    {
        return false;
    }

    if (std::memcmp(Vertex1.BoneIndices, Vertex2.BoneIndices, sizeof(Vertex1.BoneIndices)) != 0)
    {
        return false;
    }

    if (std::memcmp(Vertex1.BoneWeights, Vertex2.BoneWeights, sizeof(Vertex1.BoneWeights)) != 0)
    {
        return false;
    }

    // 모든 게 같음
    return true;
}
namespace std
{
    template<>
    struct hash<FSkinnedVertex>
    {

        size_t operator()(const FSkinnedVertex& Vertex) const
        {
            size_t Seed = 0;

            CombineHash(Seed, Vertex.Position.X); CombineHash(Seed, Vertex.Position.Y); CombineHash(Seed, Vertex.Position.Z);
            CombineHash(Seed, Vertex.Normal.X); CombineHash(Seed, Vertex.Normal.Y); CombineHash(Seed, Vertex.Normal.Z);
            CombineHash(Seed, Vertex.Tangent.X); CombineHash(Seed, Vertex.Tangent.Y); CombineHash(Seed, Vertex.Tangent.Z); CombineHash(Seed, Vertex.Tangent.W);
            CombineHash(Seed, Vertex.Color.X); CombineHash(Seed, Vertex.Color.Y); CombineHash(Seed, Vertex.Color.Z); CombineHash(Seed, Vertex.Color.W);
            CombineHash(Seed, Vertex.UV.X); CombineHash(Seed, Vertex.UV.Y);

            for (int Index = 0; Index < 4; Index++)
            {
                CombineHash(Seed, Vertex.BoneIndices[Index]);
                CombineHash(Seed, Vertex.BoneWeights[Index]);
            }
            return Seed;
        }
    };
}

struct FBillboardVertexInfo {
    FVector WorldPosition;
    FVector2D CharSize;//char scale
    FVector4 UVRect;//uv start && uv size
};

struct FBillboardVertexInfo_GPU
{
    float Position[3];
    float CharSize[2];
    float UVRect[4];

    void FillFrom(const FMeshData& mesh, size_t i);
    void FillFrom(const FNormalVertex& src);
};

// 빌보드 전용: 위치 + UV만 있으면 충분
struct FBillboardVertex
{
    FVector WorldPosition;  // 정점 위치 (로컬 좌표, -0.5~0.5 기준 쿼드)
    FVector2D UV;        // 텍스처 좌표 (0~1)

    void FillFrom(const FMeshData& mesh, size_t i);
    void FillFrom(const FNormalVertex& src);
};

struct FGroupInfo
{
    uint32 StartIndex = 0;
    uint32 IndexCount = 0;
    FString InitialMaterialName; // obj 파일 자체에 맵핑된 material 이름

    friend FArchive& operator<<(FArchive& Ar, FGroupInfo& Info)
    {
        Ar << Info.StartIndex;
        Ar << Info.IndexCount;

        if (Ar.IsSaving())
            Serialization::WriteString(Ar, Info.InitialMaterialName);
        else if (Ar.IsLoading())
            Serialization::ReadString(Ar, Info.InitialMaterialName);

        return Ar;
    }
};

struct FStaticMesh
{
    FString PathFileName;
    FString CacheFilePath;  // 캐시된 소스 경로 (예: DerivedDataCache/cube.obj.bin)

    TArray<uint32> Indices;
    TArray<FNormalVertex> Vertices;
    TArray<FGroupInfo> GroupInfos; // 각 group을 render 하기 위한 정보

    bool bHasMaterial;

    friend FArchive& operator<<(FArchive& Ar, FStaticMesh& Mesh)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Mesh.PathFileName);
            Serialization::WriteArray(Ar, Mesh.Vertices);
            Serialization::WriteArray(Ar, Mesh.Indices);

            uint32_t gCount = (uint32_t)Mesh.GroupInfos.size();
            Ar << gCount;
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Mesh.PathFileName);
            Serialization::ReadArray(Ar, Mesh.Vertices);
            Serialization::ReadArray(Ar, Mesh.Indices);

            uint32_t gCount;
            Ar << gCount;
            Mesh.GroupInfos.resize(gCount);
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        return Ar;
    }
};

struct FBone
{
    FString Name; // 본 이름
    int32 ParentIndex; // 부모 본 인덱스 (-1이면 루트)
    FMatrix BindPose; // Bind Pose 변환 행렬
    FMatrix InverseBindPose; // Inverse Bind Pose (스키닝용)

    friend FArchive& operator<<(FArchive& Ar, FBone& Bone)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Bone.Name);
            Ar << Bone.ParentIndex;
            Ar << Bone.BindPose;
            Ar << Bone.InverseBindPose;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Bone.Name);
            Ar << Bone.ParentIndex;
            Ar << Bone.BindPose;
            Ar << Bone.InverseBindPose;
        }
        return Ar;
    }
};

struct FSkeleton
{
    FString Name; // 스켈레톤 이름
    TArray<FBone> Bones; // 본 배열
    TMap <FString, int32> BoneNameToIndex; // 이름으로 본 검색

    // Phase 1.5 최적화: 캐시된 데이터
    TArray<FTransform> RefLocalPose;      // 로컬 스페이스 RefPose 캐시 (성능 최적화)
    TArray<int32> ParentIndices;          // 부모 인덱스 배열 캐시 (캐시 친화적)
    bool bCacheInitialized;               // 캐시 초기화 여부

    FSkeleton()
        : bCacheInitialized(false)
    {
    }

    // 스켈레톤 호환성 체크 (본 이름과 계층 구조가 동일한지 확인)
    bool IsCompatibleWith(const FSkeleton& Other) const
    {
        // 1. 본 개수 체크
        if (Bones.Num() != Other.Bones.Num())
        {
            return false;
        }

        // 2. 모든 본 이름이 존재하는지 체크 (순서 무관)
        for (const FBone& Bone : Bones)
        {
            auto It = Other.BoneNameToIndex.find(Bone.Name);
            if (It == Other.BoneNameToIndex.end())
            {
                // 본 이름이 존재하지 않음
                return false;
            }
        }

        // 3. 계층 구조(부모-자식 관계) 체크 (순서 무관)
        for (const FBone& Bone : Bones)
        {
            // 이 Skeleton에서 본의 부모 이름 찾기
            FString ParentName = "";
            if (Bone.ParentIndex >= 0 && Bone.ParentIndex < Bones.Num())
            {
                ParentName = Bones[Bone.ParentIndex].Name;
            }

            // Other Skeleton에서 같은 이름의 본 찾기
            auto It = Other.BoneNameToIndex.find(Bone.Name);
            int32 OtherBoneIndex = It->second;
            const FBone& OtherBone = Other.Bones[OtherBoneIndex];

            // Other Skeleton에서 해당 본의 부모 이름 찾기
            FString OtherParentName = "";
            if (OtherBone.ParentIndex >= 0 && OtherBone.ParentIndex < Other.Bones.Num())
            {
                OtherParentName = Other.Bones[OtherBone.ParentIndex].Name;
            }

            // 부모 이름이 다르면 계층 구조가 다른 것
            if (ParentName != OtherParentName)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 캐시 데이터 초기화 (FBX 로드 후 한 번만 호출)
     *
     * RefLocalPose와 ParentIndices를 미리 계산해서 캐시.
     * 매 프레임 ResetToRefPose() 호출 시 성능 향상.
     */
    void InitializeCachedData()
    {
        const int32 NumBones = Bones.Num();

        // 1. ParentIndices 캐시 생성
        ParentIndices.SetNum(NumBones);
        for (int32 i = 0; i < NumBones; ++i)
        {
            ParentIndices[i] = Bones[i].ParentIndex;
        }

        // 2. RefLocalPose 캐시 생성
        RefLocalPose.SetNum(NumBones);
        for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
        {
            const FBone& ThisBone = Bones[BoneIndex];
            const int32 ParentIndex = ThisBone.ParentIndex;

            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }

            // 계산된 로컬 행렬을 FTransform으로 변환
            RefLocalPose[BoneIndex] = FTransform(LocalBindMatrix);
        }

        bCacheInitialized = true;
    }

    friend FArchive& operator<<(FArchive& Ar, FSkeleton& Skeleton)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Skeleton.Name);

            uint32 boneCount = static_cast<uint32>(Skeleton.Bones.size());
            Ar << boneCount;
            for (auto& bone : Skeleton.Bones)
            {
                Ar << bone;
            }
            // BoneNameToIndex는 로드 시 재구축 가능하므로 저장 안 함
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Skeleton.Name);

            uint32 boneCount;
            Ar << boneCount;
            Skeleton.Bones.resize(boneCount);
            for (auto& bone : Skeleton.Bones)
            {
                Ar << bone;
            }

            // BoneNameToIndex 재구축
            Skeleton.BoneNameToIndex.clear();
            for (int32 i = 0; i < static_cast<int32>(Skeleton.Bones.size()); ++i)
            {
                Skeleton.BoneNameToIndex[Skeleton.Bones[i].Name] = i;
            }
        }
        return Ar;
    }
};

struct FVertexWeight
{
    uint32 VertexIndex; // 정점 인덱스
    float Weight; // 가중치
};

struct FSkeletalMeshData
{
    FString PathFileName;
    FString CacheFilePath;

    TArray<FSkinnedVertex> Vertices; // 정점 배열
    TArray<uint32> Indices; // 인덱스 배열
    FSkeleton Skeleton; // 스켈레톤 정보
    TArray<FGroupInfo> GroupInfos; // 머티리얼 그룹 (기존 시스템 재사용)
    bool bHasMaterial = false;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshData& Data)
    {
        if (Ar.IsSaving())
        {
            // 1. Vertices 저장
            Serialization::WriteArray(Ar, Data.Vertices);

            // 2. Indices 저장
            Serialization::WriteArray(Ar, Data.Indices);

            // 3. Skeleton 저장
            Ar << Data.Skeleton;

            // 4. GroupInfos 저장
            uint32 gCount = static_cast<uint32>(Data.GroupInfos.size());
            Ar << gCount;
            for (auto& g : Data.GroupInfos)
            {
                Ar << g;
            }

            // 5. Material 플래그 저장
            Ar << Data.bHasMaterial;

            // 6. CacheFilePath 저장
            Serialization::WriteString(Ar, Data.CacheFilePath);
        }
        else if (Ar.IsLoading())
        {
            // 1. Vertices 로드
            Serialization::ReadArray(Ar, Data.Vertices);

            // 2. Indices 로드
            Serialization::ReadArray(Ar, Data.Indices);

            // 3. Skeleton 로드
            Ar << Data.Skeleton;

            // 3-1. Skeleton 캐시 초기화 (CRITICAL FIX)
            Data.Skeleton.InitializeCachedData();

            // 4. GroupInfos 로드
            uint32 gCount;
            Ar << gCount;
            Data.GroupInfos.resize(gCount);
            for (auto& g : Data.GroupInfos)
            {
                Ar << g;
            }

            // 5. Material 플래그 로드
            Ar << Data.bHasMaterial;

            // 6. CacheFilePath 로드
            Serialization::ReadString(Ar, Data.CacheFilePath);
        }
        return Ar;
    }
};
