#pragma once
#include "ResourceBase.h"
#include "VertexData.h"

/**
 * @brief 동적 프리미티브 메시 클래스
 * @details 반투명 Physics Body 시각화를 위한 동적 메시
 *          삼각형 리스트로 렌더링됨
 */
class UPrimitiveDynamicMesh : public UResourceBase
{
public:
    DECLARE_CLASS(UPrimitiveDynamicMesh, UResourceBase)

    UPrimitiveDynamicMesh() = default;
    virtual ~UPrimitiveDynamicMesh() override;

    /**
     * @brief ResourceManager용 Load 함수 (실제로는 아무 작업 안 함)
     */
    void Load(const FString& InFilePath, ID3D11Device* InDevice) { Device = InDevice; }

    /**
     * @brief 메시 초기화
     * @param MaxVertices 최대 버텍스 수
     * @param MaxIndices 최대 인덱스 수
     * @param InDevice D3D11 디바이스
     * @return 성공 여부
     */
    bool Initialize(uint32 MaxVertices, uint32 MaxIndices, ID3D11Device* InDevice);

    /**
     * @brief 메시 데이터 업데이트
     * @param InData 메시 데이터
     * @param InContext D3D11 컨텍스트
     * @return 성공 여부
     */
    bool UpdateData(FMeshData* InData, ID3D11DeviceContext* InContext);

    /**
     * @brief 버퍼 리소스 해제
     */
    void ReleaseResources();

    // Getters
    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; }
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }
    uint32 GetCurrentVertexCount() const { return CurrentVertexCount; }
    uint32 GetCurrentIndexCount() const { return CurrentIndexCount; }
    uint32 GetMaxVertices() const { return MaxVertices; }
    uint32 GetMaxIndices() const { return MaxIndices; }
    bool IsInitialized() const { return bIsInitialized; }

private:
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;

    uint32 MaxVertices = 0;
    uint32 MaxIndices = 0;

    uint32 CurrentVertexCount = 0;
    uint32 CurrentIndexCount = 0;

    bool bIsInitialized = false;
    ID3D11Device* Device = nullptr;
};
