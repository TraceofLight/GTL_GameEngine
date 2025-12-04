#pragma once
#include "Window.h"
#include "SplitterH.h"
#include "SplitterV.h"
#include "ImGui/imgui_node_editor.h"

class PhysicsAssetViewerState;
class UWorld;
class UTexture;
class FViewport;
class FViewportClient;
class USkeletalMesh;
class UPhysicsAsset;
class UBodySetup;
struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;

namespace ed = ax::NodeEditor;

// ============================================================================
// Node Graph 구조체 (Body-Constraint 시각화용)
// ============================================================================
// Unreal 스타일 3-Column 레이아웃:
// 각 Constraint마다 독립적인 행 (Row)으로 표시
// [Parent Body] → [Constraint] → [Child Body]
// Body 노드는 중복 표시 가능 (각 행마다 별도 노드)
// ============================================================================

// Body Node (Physics Body를 나타내는 노드) - 행(Row) 내에서 사용
struct FPAEBodyNode
{
    ed::NodeId ID;
    int32 BodyIndex = -1;           // PhysicsAsset의 BodySetup 인덱스
    FString BoneName;               // 연결된 Bone 이름
    ed::PinId OutputPin;            // 출력 핀 (Constraint로 연결, Parent용)
    ed::PinId InputPin;             // 입력 핀 (Constraint에서 연결, Child용)

    FPAEBodyNode() : ID(0), OutputPin(0), InputPin(0) {}
};

// Constraint Node (Physics Constraint를 나타내는 노드)
struct FPAEConstraintNode
{
    ed::NodeId ID;
    int32 ConstraintIndex = -1;     // PhysicsAsset의 Constraint 인덱스
    ed::PinId InputPin;             // 입력 핀 (Parent Body에서 옴)
    ed::PinId OutputPin;            // 출력 핀 (Child Body로 감)
    FString Bone1Name;              // 첫 번째 Bone 이름 (Parent)
    FString Bone2Name;              // 두 번째 Bone 이름 (Child)

    FPAEConstraintNode() : ID(0), InputPin(0), OutputPin(0) {}
};

// Constraint Child (Parent에 연결된 하나의 Constraint와 Child Body)
struct FPAEConstraintChild
{
    int32 ConstraintIndex = -1;     // PhysicsAsset의 Constraint 인덱스

    // Constraint (Column 1 - 중앙)
    ed::NodeId ConstraintNodeID;
    ed::PinId ConstraintInputPin;
    ed::PinId ConstraintOutputPin;

    // Child Body (Column 2 - 오른쪽)
    ed::NodeId ChildBodyNodeID;
    ed::PinId ChildBodyInputPin;
    int32 ChildBodyIndex = -1;
    FString ChildBoneName;

    // Links
    ed::LinkId Link1ID;             // Parent Body → Constraint
    ed::LinkId Link2ID;             // Constraint → Child Body

    FPAEConstraintChild()
        : ConstraintNodeID(0), ConstraintInputPin(0), ConstraintOutputPin(0)
        , ChildBodyNodeID(0), ChildBodyInputPin(0)
        , Link1ID(0), Link2ID(0) {}
};

// Constraint Group (같은 Parent를 가진 Constraint들의 그룹)
// Unreal 스타일:
//               ┌─ [C] → Child1
// Parent ───────┼─ [C] → Child2
//               └─ [C] → Child3
struct FPAEConstraintGroup
{
    int32 GroupIndex = -1;          // 그룹 번호

    // Parent Body (Column 0 - 왼쪽, 그룹당 1개)
    ed::NodeId ParentBodyNodeID;
    ed::PinId ParentBodyOutputPin;
    int32 ParentBodyIndex = -1;
    FString ParentBoneName;

    // 이 Parent에 연결된 Constraint + Child 쌍들
    TArray<FPAEConstraintChild> Children;

    FPAEConstraintGroup()
        : ParentBodyNodeID(0), ParentBodyOutputPin(0) {}
};

// 레거시 Row 구조 (호환성 유지용)
struct FPAEConstraintRow
{
    int32 RowIndex = -1;
    int32 ConstraintIndex = -1;
    ed::NodeId ParentBodyNodeID;
    ed::PinId ParentBodyOutputPin;
    int32 ParentBodyIndex = -1;
    FString ParentBoneName;
    ed::NodeId ConstraintNodeID;
    ed::PinId ConstraintInputPin;
    ed::PinId ConstraintOutputPin;
    ed::NodeId ChildBodyNodeID;
    ed::PinId ChildBodyInputPin;
    int32 ChildBodyIndex = -1;
    FString ChildBoneName;
    ed::LinkId Link1ID;
    ed::LinkId Link2ID;

    FPAEConstraintRow()
        : ParentBodyNodeID(0), ParentBodyOutputPin(0)
        , ConstraintNodeID(0), ConstraintInputPin(0), ConstraintOutputPin(0)
        , ChildBodyNodeID(0), ChildBodyInputPin(0)
        , Link1ID(0), Link2ID(0) {}
};

// Constraint Link (두 Body를 연결하는 Constraint) - 레거시 호환용
struct FPAEConstraintLink
{
    ed::LinkId ID;
    ed::PinId StartPinID;           // 시작 핀
    ed::PinId EndPinID;             // 끝 핀

    FPAEConstraintLink() : ID(0), StartPinID(0), EndPinID(0) {}
};

// Graph State (Node Editor 상태)
struct FPAEGraphState
{
    ed::EditorContext* Context = nullptr;

    // Unreal 스타일 Group 기반 데이터 (주요 데이터)
    // 같은 Parent를 가진 Constraint들을 그룹으로 묶음
    TArray<FPAEConstraintGroup> ConstraintGroups;

    // 레거시 호환용 (Row 기반, 기존 코드에서 참조)
    TArray<FPAEConstraintRow> ConstraintRows;

    // 레거시 호환용 (기존 코드에서 참조)
    TArray<FPAEBodyNode> BodyNodes;
    TArray<FPAEConstraintNode> ConstraintNodes;
    TArray<FPAEConstraintLink> Links;

    ed::NodeId SelectedNodeID;
    ed::LinkId SelectedLinkID;
    ed::NodeId ContextMenuNodeId;  // 우클릭 컨텍스트 메뉴용

    int32 NextNodeID = 1;
    int32 NextPinID = 100000;
    int32 NextLinkID = 200000;

    FPAEGraphState()
        : SelectedNodeID(ed::NodeId::Invalid)
        , SelectedLinkID(ed::LinkId::Invalid)
        , ContextMenuNodeId(ed::NodeId::Invalid)
    {
        ed::Config Config;
        Config.SettingsFile = nullptr;
        Context = ed::CreateEditor(&Config);
    }

    ~FPAEGraphState()
    {
        if (Context)
        {
            ed::DestroyEditor(Context);
            Context = nullptr;
        }
    }

    ed::NodeId GetNextNodeId() { return ed::NodeId(NextNodeID++); }
    ed::PinId GetNextPinId() { return ed::PinId(NextPinID++); }
    ed::LinkId GetNextLinkId() { return ed::LinkId(NextLinkID++); }

    void Clear()
    {
        ConstraintGroups.Empty();
        ConstraintRows.Empty();
        BodyNodes.Empty();
        ConstraintNodes.Empty();
        Links.Empty();
        SelectedNodeID = ed::NodeId::Invalid;
        SelectedLinkID = ed::LinkId::Invalid;
    }
};

// Forward declarations for panel classes
class SPhysicsAssetToolbarPanel;
class SPhysicsAssetViewportPanel;
class SPhysicsAssetBodyListPanel;
class SPhysicsAssetPropertiesPanel;
class SPhysicsAssetGraphPanel;

/**
 * @brief Physics Asset Editor Window
 * @details SSplitter 기반 레이아웃으로 Physics Asset 편집 지원
 *          레이아웃: [Left: SkeletonTree|NodeGraph] | [Center: Viewport] | [Right: Properties]
 *          - Skeleton Tree Panel: 본 계층 + Body 목록
 *          - Node Graph Panel: Body-Constraint 관계 시각화
 *          - Viewport Panel: 3D 프리뷰 (Bodies, Constraints 시각화)
 *          - Properties Panel: 선택된 Body/Constraint 속성 편집
 */
class SPhysicsAssetEditorWindow : public SWindow
{
public:
    SPhysicsAssetEditorWindow();
    virtual ~SPhysicsAssetEditorWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice, bool bEmbedded = false);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    // Embedded 모드 (DynamicEditorWindow 내부에서 사용)
    void RenderEmbedded(const FRect& ContentRect);
    void SetEmbeddedMode(bool bEmbedded) { bIsEmbeddedMode = bEmbedded; }
    bool IsEmbeddedMode() const { return bIsEmbeddedMode; }

    // Viewport rendering
    void OnRenderViewport();

    // Asset 로드/저장
    void LoadSkeletalMesh(const FString& Path);
    void LoadPhysicsAsset(const FString& Path);
    void SavePhysicsAsset(const FString& Path);

    // Accessors
    FViewport* GetViewport() const;
    FViewportClient* GetViewportClient() const;
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }
    bool IsFocused() const { return bIsFocused; }
    bool ShouldBlockEditorInput() const { return bIsOpen && bIsFocused; }

    PhysicsAssetViewerState* GetActiveState() const { return ActiveState; }
    UWorld* GetWorld() const { return World; }
    ID3D11Device* GetDevice() const { return Device; }

    // 뷰포트 영역 (입력 처리용)
    FRect GetViewportRect() const { return ViewportRect; }

    // Render Target
    void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
    void RenderToPreviewRenderTarget();
    ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

    // delete 전 호출하여 리소스 안전하게 해제 (ImGui 지연 렌더링 문제 방지)
    void PrepareForDelete();

    // Physics Simulation
    void StartSimulation();
    void StopSimulation();
    bool IsSimulating() const;

    // Node Graph 관련
    void SyncGraphFromPhysicsAsset();
    FPAEBodyNode* FindBodyNode(ed::NodeId NodeID);
    FPAEBodyNode* FindBodyNodeByPin(ed::PinId PinID);
    FPAEConstraintNode* FindConstraintNode(ed::NodeId NodeID);
    FPAEConstraintNode* FindConstraintNodeByPin(ed::PinId PinID);
    FPAEConstraintLink* FindConstraintLink(ed::LinkId LinkID);
    int32 FindBodyIndexByGroupPin(ed::PinId PinID);  // Group 구조에서 핀으로 BodyIndex 찾기

    // Node Graph State (패널에서 접근 필요)
    FPAEGraphState* GraphState = nullptr;

private:
    // UI Panels (Legacy - 독립 윈도우 모드용)
    void RenderToolbar();
    void RenderViewportPanel();
    void RenderBodyListPanel();
    void RenderSkeletonTreePanel();
    void RenderPropertiesPanel();

    // State
    PhysicsAssetViewerState* ActiveState = nullptr;
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;
    bool bIsOpen = true;
    bool bIsFocused = false;
    bool bInitialPlacementDone = false;
    bool bIsEmbeddedMode = false;

    // SSplitter 레이아웃 (새 구조)
    // MainSplitter (H): Left(LeftSplitter) | Inner(InnerSplitter)
    //   LeftSplitter (V): Top(SkeletonTree) | Bottom(NodeGraph)
    //   InnerSplitter (H): Left(Viewport) | Right(Properties)
    SSplitterH* MainSplitter = nullptr;
    SSplitterV* LeftSplitter = nullptr;
    SSplitterH* InnerSplitter = nullptr;

    // 패널들
    SPhysicsAssetToolbarPanel* ToolbarPanel = nullptr;
    SPhysicsAssetViewportPanel* ViewportPanelWidget = nullptr;
    SPhysicsAssetBodyListPanel* BodyListPanel = nullptr;         // Skeleton Tree (상단)
    SPhysicsAssetPropertiesPanel* PropertiesPanelWidget = nullptr;
    SPhysicsAssetGraphPanel* GraphPanelWidget = nullptr;         // Node Graph (하단)

    // 패널 영역 캐시 (입력 처리용)
    FRect ViewportRect;
    FRect BodyListRect;
    FRect GraphRect;
    FRect PropertiesRect;

    // Render Target
    ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
    ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
    ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
    ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
    ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
    uint32 PreviewRenderTargetWidth = 0;
    uint32 PreviewRenderTargetHeight = 0;

    void CreateRenderTarget(uint32 Width, uint32 Height);
    void ReleaseRenderTarget();
};

// ============================================================================
// Panel Classes
// ============================================================================

/**
 * @brief Physics Asset 툴바 패널
 */
class SPhysicsAssetToolbarPanel : public SWindow
{
public:
    SPhysicsAssetToolbarPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
};

/**
 * @brief Physics Asset 뷰포트 패널
 */
class SPhysicsAssetViewportPanel : public SWindow
{
public:
    SPhysicsAssetViewportPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;

    // 3D 콘텐츠 렌더링 영역
    FRect ContentRect;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
};

// Forward declarations for body list panel
class UPhysicsAsset;
struct FSkeleton;

/**
 * @brief Physics Asset Body/Constraint 목록 패널
 */
class SPhysicsAssetBodyListPanel : public SWindow
{
public:
    SPhysicsAssetBodyListPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
    void RenderBodyTree(PhysicsAssetViewerState* State);
    void RenderConstraintList(PhysicsAssetViewerState* State);
    void RenderSkeletonBodyTree(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset, const FSkeleton* Skeleton);
    void RenderBodySetupList(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset);
    void RenderBodyShapes(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset, int32 BodyIndex);
};

/**
 * @brief Physics Asset Properties 패널
 */
class SPhysicsAssetPropertiesPanel : public SWindow
{
public:
    SPhysicsAssetPropertiesPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
    void RenderBodyProperties(PhysicsAssetViewerState* State);
    void RenderShapeProperties(PhysicsAssetViewerState* State);
    void RenderConstraintProperties(PhysicsAssetViewerState* State);
    void RenderClothPaintProperties(PhysicsAssetViewerState* State);
};

/**
 * @brief Physics Asset Node Graph 패널
 * @details Body-Constraint 관계를 노드 그래프로 시각화
 */
class SPhysicsAssetGraphPanel : public SWindow
{
public:
    SPhysicsAssetGraphPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
    void RenderNodeGraph(PhysicsAssetViewerState* State, FPAEGraphState* GraphState);
};
