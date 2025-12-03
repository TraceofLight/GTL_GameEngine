#pragma once
#include "MeshComponent.h"
#include "SkeletalMesh.h"
#include "USkinnedMeshComponent.generated.h"

UCLASS(DisplayName="스킨드 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkinnedMeshComponent : public UMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()

    USkinnedMeshComponent();
    ~USkinnedMeshComponent() override;

    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;

    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
    void DuplicateSubObjects() override;

// Mesh Component Section
public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UPROPERTY(EditAnywhere, Category = "Skeletal Mesh", Tooltip = "Skeletal mesh asset to render")
    USkeletalMesh* SkeletalMesh;
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

    FAABB GetWorldAABB() const override;
    void OnTransformUpdated() override;

// Skeletal Section
public:
    /**
     * @brief 렌더링할 스켈레탈 메시 에셋 설정 (UStaticMeshComponent::SetStaticMesh와 동일한 역할)
     * @param PathFileName 새 스켈레탈 메시 에셋 경로
     */
    virtual void SetSkeletalMesh(const FString& PathFileName);
    /**
     * @brief 이 컴포넌트의 USkeletalMesh 에셋을 반환
     */
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

    /**
     * @brief 현재 설정된/로딩 중인 메시 경로 반환 (비동기 로드 중에도 유효)
     */
    const FString& GetSkeletalMeshPath() const { return SkeletalMeshPath; }

	void SetSkinnedVertices(TArray<FNormalVertex> Vertices) { SkinnedVertices = Vertices;  }
	void SetVertexBuffer(ID3D11Buffer* InVertexBuffer) { VertexBuffer = InVertexBuffer;  }

protected:
    /** @brief 현재 설정된 SkeletalMesh 경로 (비동기 로드 중에도 유지) */
    FString SkeletalMeshPath;
    /**
     * @brief 자식에게서 원본 메시를 받아 CPU 스키닝을 수행
     * @param InSkinningMatrices 스키닝 매트릭스
     */
    void UpdateSkinningMatrices(const TArray<FMatrix>& InSkinningMatrices, const TArray<FMatrix>& InSkinningNormalMatrices);


    /**
     * @brief CPU 스키닝 최종 결과물. 렌더러가 이 데이터를 사용합니다.
     */
    TArray<FNormalVertex> SkinnedVertices;
    /**
     * @brief CPU 스키닝 최종 결과물. 렌더러가 이 데이터를 사용합니다.
     */
    TArray<FNormalVertex> NormalSkinnedVertices;

	/**
	 * @brief CPU 스키닝에서 진행하기 때문에, Component별로 VertexBuffer를 가지고 스키닝 업데이트를 진행해야함
	*/
	ID3D11Buffer* VertexBuffer = nullptr;
	bool bSkinningMatricesDirty = true;



	// GPU Cloth Buffer
	ID3D11Buffer* ClothGPUBuffer = nullptr;				// StructuredBuffer<float3>
	ID3D11ShaderResourceView* ClothGPUSRV = nullptr;	// SRV for t8
	uint32 ClothGPUBufferSize = 0;						// float3 개수


private:
    void PerformSkinning();
    FVector SkinVertexPosition(const FSkinnedVertex& InVertex) const;
    FVector SkinVertexNormal(const FSkinnedVertex& InVertex) const;
    FVector4 SkinVertexTangent(const FSkinnedVertex& InVertex) const;

    /**
     * @brief 자식이 계산해 준, 현재 프레임의 최종 스키닝 행렬
    */
    TArray<FMatrix> FinalSkinningMatrices;
    TArray<FMatrix> FinalSkinningNormalMatrices;

};
