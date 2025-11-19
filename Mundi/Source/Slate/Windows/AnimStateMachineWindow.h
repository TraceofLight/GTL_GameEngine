#pragma once
#include "ImGui/imgui_node_editor.h"
#include "AnimStateMachine.h"
#include "SWindow.h"

class UAnimStateMachine;
class UAnimSequence;

namespace ed = ax::NodeEditor;

// Pin 타입
enum class EPinType
{
    Flow,       // State 간 연결
    Animation   // 애니메이션 데이터
};

// Graph용 Pin 구조체
struct FGraphPin
{
    ed::PinId ID;
    std::string Name;
    EPinType Type;
    ed::PinKind Kind;

    FGraphPin(ed::PinId id, const char* name, EPinType type, ed::PinKind kind)
        : ID(id), Name(name), Type(type), Kind(kind) {}
};

// Graph용 Node 구조체
struct FGraphNode
{
    ed::NodeId ID;
    std::string Name;
    std::vector<FGraphPin> Inputs;
    std::vector<FGraphPin> Outputs;

    // State 관련 데이터
    UAnimSequence* AnimSequence = nullptr;
    bool bLoop = true;

    FGraphNode() : ID(0) {}
};

// Graph용 Link 구조체 (Transition)
struct FGraphLink
{
    ed::LinkId ID;
    ed::PinId StartPinID;
    ed::PinId EndPinID;

    // Transition 데이터
    float BlendTime = 0.2f;
    TArray<FAnimCondition> Conditions;

    FGraphLink() : ID(0), StartPinID(0), EndPinID(0) {}
};

// Graph State (각 탭의 상태)
struct FGraphState
{
    std::string Name;
    ed::EditorContext* Context;
    UAnimStateMachine* StateMachine;
	FWideString AssetPath;

    TArray<FGraphNode> Nodes;
    TArray<FGraphLink> Links;

    ed::NodeId SelectedNodeID;
    ed::LinkId SelectedLinkID;

    int32 NextNodeID = 1;
    int32 NextPinID = 100000;  // Pin은 큰 숫자부터 시작

    int32 NextLinkID = 200000; // Link는 더 큰 숫자부터 시작
    int32 StateCounter = 0;

    FGraphState(const char* name)
        : Name(name), StateMachine(nullptr),
          SelectedNodeID(ed::NodeId::Invalid),
          SelectedLinkID(ed::LinkId::Invalid)
    {
    	ed::Config Config;
    	Config.SettingsFile = nullptr;
    	Context = ed::CreateEditor(&Config);
    }

    ~FGraphState()
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
};

// Main Window Class
class SAnimStateMachineWindow : public SWindow
{
public:
    SAnimStateMachineWindow();
    ~SAnimStateMachineWindow() override;

    bool Initialize(float StartX, float StartY, float Width, float Height);
    void OnRender() override;

    void SetOpen(bool bOpen) { bIsOpen = bOpen; }
    bool IsOpen() const { return bIsOpen; }

    // External API to open a file
    void LoadStateMachineFile(const char* FilePath);

    // Create a new empty tab
    void CreateNewEmptyTab();

private:
    // Tab Management
	void CreateNewGraphTab(const char* Name, UAnimStateMachine* InAsset, const FWideString& InPath = L"");
    void CloseTab(int Index);

    // Panel Rendering
    void RenderLeftPanel(float width, float height);
    void RenderCenterPanel(float width, float height);
    void RenderRightPanel(float width, float height);

    // Node Creation
    void CreateNode_State(FGraphState* State);

    // Helper Functions
    FGraphPin* FindPin(FGraphState* State, ed::PinId PinID);
    FGraphNode* FindNode(FGraphState* State, ed::NodeId NodeID);
    FGraphNode* FindNodeByPin(FGraphState* State, ed::PinId PinID);
    FGraphLink* FindLink(FGraphState* State, ed::LinkId LinkID);
    const char* GetConditionOpString(EAnimConditionOp Op);
    const char* GetConditionTypeString(EAnimConditionType Type);
    void SaveNodePositions(FGraphState* State);

    // Sync Functions
    void SyncGraphFromStateMachine(FGraphState* State);

    // Notify UI Helper Functions
    void RefreshNotifyClassList();
    void RenderNotifyCombo(const char* Label, struct FAnimNotifyEvent& Notify);

private:
    bool bIsOpen = true;
    bool bInitialPlacementDone = false;

    FRect Rect;

    // Tabs
    std::vector<FGraphState*> Tabs;
    FGraphState* ActiveState = nullptr;
    int ActiveTabIndex = -1;

    // Panel Ratios
    float LeftPanelRatio = 0.15f;   // 15%
    float RightPanelRatio = 0.25f;  // 25%

    // Notify Class Registry
    TArray<FString> AvailableNotifyClasses;
    bool bNotifyClassListDirty = true;
};
