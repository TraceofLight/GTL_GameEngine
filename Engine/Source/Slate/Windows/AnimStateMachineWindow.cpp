#include "pch.h"
#include "AnimStateMachineWindow.h"
#include "AnimStateMachine.h"
#include "AnimSequence.h"
#include "BlendSpace2D.h"
#include "FBXLoader.h"
#include "LuaManager.h"
#include "PlatformProcess.h"
#include "USlateManager.h"
#include "SplitterH.h"
#include <commdlg.h>  // Windows 파일 다이얼로그
#include "ImGui/utilities/widgets.h"

namespace ed = ax::NodeEditor;

// ============================================================================
// Panel Wrapper (SSplitter와 함께 사용)
// SSplitter가 설정한 Rect의 위치에 ImGui 커서를 이동시킨 후 콜백 호출
// ============================================================================
class SStateMachinePanelWrapper : public SWindow
{
public:
	using RenderFunc = std::function<void()>;
	RenderFunc RenderCallback;

	void SetRenderCallback(RenderFunc Callback) { RenderCallback = Callback; }

	virtual void OnRender() override
	{
		if (RenderCallback)
		{
			// SSplitter가 설정한 Rect 위치로 ImGui 커서 이동
			FRect PanelRect = GetRect();
			ImGui::SetCursorScreenPos(ImVec2(PanelRect.Left, PanelRect.Top));
			RenderCallback();
		}
	}

	virtual void OnUpdate(float DeltaSeconds) override {}
	virtual void OnMouseMove(FVector2D MousePos) override {}
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override {}
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override {}
};

SAnimStateMachineWindow::SAnimStateMachineWindow()
{
    Rect = { 0, 0, 0, 0 };
}

SAnimStateMachineWindow::~SAnimStateMachineWindow()
{
    // SSplitter 해제
    if (MainSplitter)
    {
        // Splitter의 자식들은 Splitter가 소유하지 않으므로 별도 삭제 필요
        SWindow* Left = MainSplitter->GetLeftOrTop();
        SWindow* Right = MainSplitter->GetRightOrBottom();

        // InnerSplitter (Right)의 자식들 삭제
        if (SSplitterH* InnerSplitter = dynamic_cast<SSplitterH*>(Right))
        {
            delete InnerSplitter->GetLeftOrTop();
            delete InnerSplitter->GetRightOrBottom();
        }
        delete Left;
        delete Right;
        delete MainSplitter;
        MainSplitter = nullptr;
    }

    for (auto* State : Tabs)
    {
        delete State;
    }
    Tabs.clear();
    ActiveState = nullptr;
}

bool SAnimStateMachineWindow::Initialize(float StartX, float StartY, float Width, float Height)
{
    Rect.Left = StartX;
    Rect.Top = StartY;
    Rect.Right = StartX + Width;
    Rect.Bottom = StartY + Height;

    // SSplitter 레이아웃 생성 (3패널: Left | Center | Right)
    // 구조: MainSplitter(Left | InnerSplitter(Center | Right))

    // 패널 래퍼 생성
    SStateMachinePanelWrapper* LeftPanel = new SStateMachinePanelWrapper();
    SStateMachinePanelWrapper* CenterPanel = new SStateMachinePanelWrapper();
    SStateMachinePanelWrapper* RightPanel = new SStateMachinePanelWrapper();

    // 콜백 설정 - SSplitter가 자식 rect를 먼저 설정하므로 패널의 GetRect() 사용
    LeftPanel->SetRenderCallback([this, LeftPanel]() {
        FRect r = LeftPanel->GetRect();
        RenderLeftPanel(r.GetWidth(), r.GetHeight());
    });
    CenterPanel->SetRenderCallback([this, CenterPanel]() {
        FRect r = CenterPanel->GetRect();
        RenderCenterPanel(r.GetWidth(), r.GetHeight());
    });
    RightPanel->SetRenderCallback([this, RightPanel]() {
        FRect r = RightPanel->GetRect();
        RenderRightPanel(r.GetWidth(), r.GetHeight());
    });

    // InnerSplitter: Center | Right (좌우 분할)
    SSplitterH* InnerSplitter = new SSplitterH();
    InnerSplitter->SetLeftOrTop(CenterPanel);
    InnerSplitter->SetRightOrBottom(RightPanel);
    InnerSplitter->SetSplitRatio(0.75f);  // Center 75%, Right 25%
    InnerSplitter->LoadFromConfig("StateMachine_Inner");

    // MainSplitter: Left | InnerSplitter (좌우 분할)
    MainSplitter = new SSplitterH();
    MainSplitter->SetLeftOrTop(LeftPanel);
    MainSplitter->SetRightOrBottom(InnerSplitter);
    MainSplitter->SetSplitRatio(0.15f);  // Left 15%, Rest 85%
    MainSplitter->LoadFromConfig("StateMachine_Main");

    return true;
}

void SAnimStateMachineWindow::CreateNewGraphTab(const char* Name, UAnimStateMachine* InAsset, const FWideString& InPath)
{
	FGraphState* NewState = new FGraphState(Name);

	// 1. 에셋 할당
	if (InAsset)
	{
		// 로드된 에셋 사용 (리소스 매니저가 관리)
		NewState->StateMachine = InAsset;
		NewState->AssetPath = InPath; // 저장 경로 기억
	}
	else
	{
		// [New] 새로 만들기: 아직 파일이 없으므로 임시 객체 생성
		// 나중에 Save 시 파일로 저장되고 리소스로 등록될 것임
		NewState->StateMachine = NewObject<UAnimStateMachine>();
		NewState->AssetPath = L"";
	}

	// 2. 노드 에디터 컨텍스트 설정
	ed::SetCurrentEditor(NewState->Context);

	// 기존 데이터가 있다면 그래프(노드) 복원
	if (NewState->StateMachine)
	{
		SyncGraphFromStateMachine(NewState);
	}

	ed::SetCurrentEditor(nullptr);

	// 3. 탭 리스트에 추가 및 활성화
	Tabs.push_back(NewState);
	ActiveState = NewState;
	ActiveTabIndex = static_cast<int32>(Tabs.size()) - 1;
}

void SAnimStateMachineWindow::CloseTab(int Index)
{
    if (Index < 0 || Index >= Tabs.size()) return;

    delete Tabs[Index];
    Tabs.erase(Tabs.begin() + Index);

    if (Tabs.empty())
    {
        ActiveState = nullptr;
        ActiveTabIndex = -1;
    	SLATE.CloseAnimStateMachineWindow();
    }
    else
    {
        ActiveTabIndex = std::min(Index, (int)Tabs.size() - 1);
        ActiveState = Tabs[ActiveTabIndex];
    }
}

void SAnimStateMachineWindow::LoadStateMachineFile(const char* FilePath)
{
    if (!FilePath || FilePath[0] == '\0')
        return;

    FString FilePathStr = FilePath;
    FWideString WideFilePath = UTF8ToWide(FilePathStr);

    // Embedded 모드에서 이미 같은 파일이 로드되어 있으면 스킵
    if (bIsEmbeddedMode && ActiveState && ActiveState->AssetPath == WideFilePath)
    {
        return;
    }

    // Load the asset
    UAnimStateMachine* LoadedAsset = RESOURCE.Load<UAnimStateMachine>(FilePathStr);

    if (LoadedAsset)
    {
        // Extract filename for tab name
        std::filesystem::path fsPath(FilePath);
        std::string FileName = fsPath.stem().string();

        // Embedded 모드에서는 항상 현재 ActiveState에 로드 (탭은 DynamicEditorWindow에서 관리)
        if (bIsEmbeddedMode && ActiveState)
        {
            // 현재 탭에 로드된 에셋 적용
            ActiveState->Name = FileName;
            ActiveState->StateMachine = LoadedAsset;
            ActiveState->AssetPath = WideFilePath;

            // 노드 초기화
            ActiveState->Nodes.Empty();
            ActiveState->Links.Empty();
            ActiveState->NextNodeID = 1;
            ActiveState->NextPinID = 100000;
            ActiveState->NextLinkID = 200000;
            ActiveState->StateCounter = 0;

            // 노드 에디터 컨텍스트 설정 후 StateMachine 데이터를 그래프 노드로 변환
            ed::SetCurrentEditor(ActiveState->Context);
            SyncGraphFromStateMachine(ActiveState);
            ed::SetCurrentEditor(nullptr);
        }
        else
        {
            // 독립 모드: 새 탭 생성
            CreateNewGraphTab(FileName.c_str(), LoadedAsset, WideFilePath);
        }
    }
    else
    {
        UE_LOG("StateMachine: Load: Failed %s", FilePath);
    }
}

void SAnimStateMachineWindow::CreateNewEmptyTab()
{
    CreateNewGraphTab("New State Machine", nullptr);
}

void SAnimStateMachineWindow::SaveCurrentStateMachine()
{
    if (!ActiveState || !ActiveState->StateMachine)
    {
        UE_LOG("No active state machine to save");
        return;
    }

    // AssetPath가 없으면 Save As 다이얼로그 열기
    if (ActiveState->AssetPath.empty())
    {
        std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
            L"Data/StateMachine",
            L".statemachine",
            L"State Machine Files (*.statemachine)"
        );

        if (!SavePath.empty())
        {
            ActiveState->AssetPath = SavePath.wstring();
        }
        else
        {
            return;  // 취소됨
        }
    }

    // 노드 위치 저장 후 파일로 저장
    SaveNodePositions(ActiveState);
    if (ActiveState->StateMachine->SaveToFile(ActiveState->AssetPath))
    {
        RESOURCE.Reload<UAnimStateMachine>(ActiveState->AssetPath);
        UE_LOG("StateMachine saved: %s", WideToUTF8(ActiveState->AssetPath).c_str());
    }
    else
    {
        UE_LOG("StateMachine: Save: Failed %s", WideToUTF8(ActiveState->AssetPath).c_str());
    }
}

void SAnimStateMachineWindow::OnRender()
{
    if (!bIsOpen) return;

    if (Tabs.empty())
    {
        bIsOpen = false;
        return;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }

    if (ImGui::Begin("Animation State Machine Editor", &bIsOpen, flags))
    {
        // === Toolbar (File 메뉴 없이 버튼으로 직접 노출) ===
        const FWideString BaseDir = UTF8ToWide(GDataDir) + L"/StateMachine";
        const FWideString Extension = L".statemachine";
        const FWideString Description = L"State Machines";
        FWideString DefaultFileName = L"NewStateMachine";

        // New 버튼
        if (ImGui::Button("New", ImVec2(60, 0)))
        {
            char label[64];
            sprintf_s(label, "State Machine %d", static_cast<int32>(Tabs.size()) + 1);
            CreateNewGraphTab(label, nullptr);
        }

        ImGui::SameLine();

        // Save 버튼
        if (ImGui::Button("Save", ImVec2(60, 0)))
        {
            if (ActiveState && ActiveState->StateMachine)
            {
                FWideString SavePath = ActiveState->AssetPath;

                // 경로가 없으면(새로 만든 경우) 다이얼로그 띄움
                if (SavePath.empty())
                {
                    std::filesystem::path SelectedPath = FPlatformProcess::OpenSaveFileDialog(BaseDir, Extension, Description, DefaultFileName);
                    if (!SelectedPath.empty())
                    {
                        SavePath = SelectedPath.wstring();
                        ActiveState->AssetPath = SavePath; // 경로 저장

                        // 탭 이름도 파일명으로 변경해주면 좋음
                        ActiveState->Name = SelectedPath.stem().string();
                    }
                }

                if (!SavePath.empty())
                {
                    try
                    {
                        // 노드 위치 저장
                        SaveNodePositions(ActiveState);

                        if (ActiveState->StateMachine->SaveToFile(SavePath))
                        {
                        	RESOURCE.Reload<UAnimStateMachine>(SavePath);
                            UE_LOG("StateMachine saved: %s", WideToUTF8(SavePath).c_str());
                        }
                        else
                        {
                            UE_LOG("StateMachine: Save: Failed");
                        }
                    }
                    catch (const std::exception& Exception)
                    {
                        UE_LOG("StateMachine: Save: Exception %s", Exception.what());
                    }
                }
            }
        }

        ImGui::SameLine();

        // Load 버튼
        if (ImGui::Button("Load", ImVec2(60, 0)))
        {
            std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialog(BaseDir, Extension, Description);

            if (!SelectedPath.empty())
            {
            	FString FinalPathStr = ResolveAssetRelativePath(WideToUTF8(SelectedPath.wstring()), "");
            	UAnimStateMachine* LoadedAsset = RESOURCE.Load<UAnimStateMachine>(FinalPathStr);

                if (LoadedAsset)
                {
                    // @TODO - 이미 열려있는지 확인
                    // 로드된 에셋으로 새 탭 생성
                    std::string FileName = SelectedPath.stem().string();
                    CreateNewGraphTab(FileName.c_str(), LoadedAsset, UTF8ToWide(FinalPathStr));
                }
                else
                {
                    UE_LOG("StateMachine: Load: Failed %s", FinalPathStr.c_str());
                }
            }
        }

        ImGui::Separator();

        // === Tab Bar ===
        bool bTabClosed = false;
        if (ImGui::BeginTabBar("AnimGraphTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        {
            for (int i = 0; i < Tabs.size(); ++i)
            {
                FGraphState* State = Tabs[i];
                bool open = true;
                if (ImGui::BeginTabItem(State->Name.c_str(), &open))
                {
                    ActiveState = State;
                    ActiveTabIndex = i;
                    ImGui::EndTabItem();
                }
                if (!open)
                {
                    CloseTab(i);
                    bTabClosed = true;
                    break;
                }
            }

            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
            {
                // 기존 탭 중 가장 높은 넘버 찾기
                const std::string BaseName = "State Machine ";
                int32 MaxNumber = 0;
                for (const FGraphState* Tab : Tabs)
                {
                    if (Tab->Name.find(BaseName) == 0)
                    {
                        std::string Suffix = Tab->Name.substr(BaseName.length());
                        try
                        {
                            int32 Num = std::stoi(Suffix);
                            if (Num > MaxNumber)
                            {
                                MaxNumber = Num;
                            }
                        }
                        catch (...) {}
                    }
                }
                char label[64];
                sprintf_s(label, "State Machine %d", MaxNumber + 1);
                CreateNewGraphTab(label, nullptr);
            }
            ImGui::EndTabBar();
        }

        if (bTabClosed)
        {
            ImGui::End();
            return;
        }

        // === Layout Calculation ===
        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // 1. Left Panel (Palette)
        RenderLeftPanel(leftWidth, totalHeight);
        ImGui::SameLine();

        // 2. Center Panel (Node Editor)
        RenderCenterPanel(centerWidth, totalHeight);
        ImGui::SameLine();

        // 3. Right Panel (Details)
        RenderRightPanel(rightWidth, totalHeight);

        ImGui::PopStyleVar();
    }
    ImGui::End();

	if (!bIsOpen)
	{
		SLATE.CloseAnimStateMachineWindow();
	}
}

// ============================================================================
// RenderEmbedded - DynamicEditorWindow 내장 모드용 렌더링
// ============================================================================
void SAnimStateMachineWindow::RenderEmbedded(const FRect& ContentRect)
{
    if (Tabs.empty())
    {
        return;
    }

    if (!ActiveState)
    {
        if (!Tabs.empty())
        {
            ActiveState = Tabs[0];
            ActiveTabIndex = 0;
        }
        else
        {
            return;
        }
    }

    // === 3패널 레이아웃 (SSplitter 기반) ===
    if (MainSplitter)
    {
        // 현재 커서 위치에서 남은 영역 계산
        ImVec2 contentMin = ImGui::GetCursorScreenPos();
        ImVec2 contentAvail = ImGui::GetContentRegionAvail();

        FRect PanelRect;
        PanelRect.Left = contentMin.x;
        PanelRect.Top = contentMin.y;
        PanelRect.Right = contentMin.x + contentAvail.x;
        PanelRect.Bottom = contentMin.y + contentAvail.y;
        PanelRect.UpdateMinMax();

        MainSplitter->SetRect(PanelRect.Left, PanelRect.Top, PanelRect.Right, PanelRect.Bottom);
        MainSplitter->OnRender();

        // 패널 Rect 캐시 (입력 처리용)
        if (MainSplitter->GetLeftOrTop())
        {
            LeftPanelRect = MainSplitter->GetLeftOrTop()->GetRect();
        }

        if (SSplitterH* InnerSplitter = dynamic_cast<SSplitterH*>(MainSplitter->GetRightOrBottom()))
        {
            if (InnerSplitter->GetLeftOrTop())
            {
                CenterPanelRect = InnerSplitter->GetLeftOrTop()->GetRect();
            }
            if (InnerSplitter->GetRightOrBottom())
            {
                RightPanelRect = InnerSplitter->GetRightOrBottom()->GetRect();
            }
        }
    }
}

// ============================================================================
// 마우스/업데이트 이벤트 핸들러 - SSplitter 드래그 지원
// ============================================================================
void SAnimStateMachineWindow::OnUpdate(float DeltaSeconds)
{
    if (MainSplitter)
    {
        MainSplitter->OnUpdate(DeltaSeconds);
    }
}

void SAnimStateMachineWindow::OnMouseMove(FVector2D MousePos)
{
    if (MainSplitter)
    {
        MainSplitter->OnMouseMove(MousePos);
    }
}

void SAnimStateMachineWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (MainSplitter)
    {
        MainSplitter->OnMouseDown(MousePos, Button);
    }
}

void SAnimStateMachineWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (MainSplitter)
    {
        MainSplitter->OnMouseUp(MousePos, Button);
    }
}

void SAnimStateMachineWindow::RenderLeftPanel(float width, float height)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("LeftPanel", ImVec2(width, height), true);

    ImGui::Spacing();
    ImGui::Indent(5.0f);
    ImGui::TextDisabled("STATE PALETTE");
    ImGui::Unindent(5.0f);
    ImGui::Separator();

    if (!ActiveState)
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    // 상태 추가 버튼
    ImGui::Spacing();
    if (ImGui::Button("Add State", ImVec2(-1, 35)))
    {
        CreateNode_State(ActiveState);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Entry State 설정
	ImGui::TextDisabled("Entry State");
	ImGui::Spacing();

	if (ActiveState->StateMachine)
	{
		// 현재 설정된 Entry State 이름 가져오기
		FString CurrentEntryStr = ActiveState->StateMachine->GetEntryStateName().ToString();

		// 만약 이름이 비어있거나 없으면 "None" 혹은 "Select State" 등으로 표시
		const char* PreviewValue = CurrentEntryStr.empty() || CurrentEntryStr == "None" ?
								   "Select State" : CurrentEntryStr.c_str();

		// ComboBox 시작
		if (ImGui::BeginCombo("##EntryStateCombo", PreviewValue))
		{
			// 현재 그래프에 있는 모든 노드들을 순회하며 리스트에 추가
			for (const auto& Node : ActiveState->Nodes)
			{
				const bool bIsSelected = (Node.Name == CurrentEntryStr);

				// 선택 가능한 항목 생성
				if (ImGui::Selectable(Node.Name.c_str(), bIsSelected))
				{
					// 클릭 시 StateMachine의 EntryState 업데이트
					ActiveState->StateMachine->SetEntryState(FName(Node.Name));
				}

				// 현재 선택된 항목이 있다면 포커스를 맞춰줌 (콤보박스 열릴 때 스크롤 위치)
				if (bIsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
}

// 아이콘 그리기 헬퍼 함수
void DrawPinIcon(const ImVec2& size, bool filled, ImU32 color)
{
	// 1. 색상 변환 (ImU32 -> ImColor)
	ImColor pinColor(color);

	// 2. 내부 색상 (Inner Color) 계산
	// (예제 코드처럼 투명도를 사용하여 배경색과 섞거나, 어두운 색으로 지정)
	int alpha = static_cast<int>(pinColor.Value.w * 255);
	ImColor innerColor(32, 32, 32, alpha); // 예제 코드의 (32,32,32) 배경색 유지

	// 3. 아이콘 타입 (일단 Flow로 고정하겠다고 하셨으므로)
	ax::Widgets::IconType iconType = ax::Widgets::IconType::Flow;

	// 4. [핵심] 요청하신 함수 호출
	// m_PinIconSize 대신 매개변수로 받은 size를 넘깁니다.
	ax::Widgets::Icon(
		size,          // 크기
		iconType,      // 아이콘 모양 (Flow)
		filled,     // 연결 여부 (내부를 채울지 말지 결정)
		pinColor,      // 외곽선 색상
		innerColor     // 내부 색상
	);
}

void SAnimStateMachineWindow::RenderCenterPanel(float width, float height)
{
    ImGui::BeginChild("GraphArea", ImVec2(width, height), true);

    if (ActiveState)
    {
        ed::SetCurrentEditor(ActiveState->Context);
        ed::Begin("StateGraph", ImVec2(0.0, 0.0f));

    	if (ed::BeginDelete())
        {
             ed::LinkId deletedLinkId;
             while (ed::QueryDeletedLink(&deletedLinkId))
             {
                 if (ed::AcceptDeletedItem())
                 {
                     // ... 링크 삭제 ...
                     auto& Links = ActiveState->Links;
                     FGraphLink* Link = FindLink(ActiveState, deletedLinkId);
                     if (Link && ActiveState->StateMachine)
                     {
                         FGraphNode* FromNode = FindNodeByPin(ActiveState, Link->StartPinID);
                         FGraphNode* ToNode = FindNodeByPin(ActiveState, Link->EndPinID);
                         if(FromNode && ToNode) ActiveState->StateMachine->RemoveTransition(FName(FromNode->Name), FName(ToNode->Name));
                     }
                     std::erase_if(Links, [deletedLinkId](const FGraphLink& L) { return L.ID == deletedLinkId; });
                 }
             }
             ed::NodeId deletedNodeId;
             while (ed::QueryDeletedNode(&deletedNodeId))
             {
                 if (ed::AcceptDeletedItem())
                 {
                     // ... 노드 삭제 ...
                     FGraphNode* Node = FindNode(ActiveState, deletedNodeId);
                     if (Node && ActiveState->StateMachine) ActiveState->StateMachine->RemoveNode(FName(Node->Name));

                     // 관련 링크 삭제
                     std::erase_if(ActiveState->Links, [&](const FGraphLink& L) {
                        FGraphNode* S = FindNodeByPin(ActiveState, L.StartPinID);
                        FGraphNode* E = FindNodeByPin(ActiveState, L.EndPinID);
                        return (S && S->ID == deletedNodeId) || (E && E->ID == deletedNodeId);
                     });

                     auto& Nodes = ActiveState->Nodes;
                     std::erase_if(Nodes, [deletedNodeId](const FGraphNode& N) { return N.ID == deletedNodeId; });
                 }
             }
        }
        ed::EndDelete();

        // 스타일 설정: 둥근 모서리, 패딩 제거 (직접 그리기 위해)
        ed::PushStyleVar(ed::StyleVar_NodeRounding, 12.0f);
        ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));

        // 1. Draw State Nodes
        for (auto& Node : ActiveState->Nodes)
        {
            // -------------------------------------------------------
            // [Step 1] 색상 결정 (Entry는 초록색, 일반은 짙은 회색/파란색)
            // -------------------------------------------------------
            bool bIsEntry = false;
            if (ActiveState->StateMachine)
            {
                bIsEntry = (ActiveState->StateMachine->GetEntryStateName().ToString() == Node.Name);
            }

            // 언리얼 스타일 헤더 색상
            ImU32 HeaderColor = bIsEntry ?
                IM_COL32(80, 120, 80, 255) : IM_COL32(45, 50, 60, 255);

            // 선택되었을 때 약간 밝게
            if (ed::IsNodeSelected(Node.ID))
            {
                HeaderColor = bIsEntry ?
                    IM_COL32(100, 150, 100, 255) : IM_COL32(60, 70, 80, 255);
            }

            ed::BeginNode(Node.ID);

            // -------------------------------------------------------
            // [Step 2] 헤더 영역 그리기 (제목)
            // -------------------------------------------------------
            ImGui::PushID(static_cast<int>(Node.ID.Get()));

            ImGui::BeginGroup(); // Header Group
            {
                ImGui::Dummy(ImVec2(0, 4)); // 상단 여백

                // 제목 중앙 정렬 느낌을 위해 약간 들여쓰기 또는 중앙 계산
                ImGui::Indent(10.0f);
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", Node.Name.c_str());

                // 서브 타이틀 (애니메이션 또는 BlendSpace2D 이름)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

                // StateMachine에서 실제 타입 확인
                bool bHasAsset = false;
                if (ActiveState->StateMachine)
                {
                    FName NodeName(Node.Name);
                    if (const FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(NodeName))
                    {
                        if (StateNode->AnimAssetType == EAnimAssetType::AnimSequence && StateNode->AnimationAsset)
                        {
                            // AnimSequence
                            std::string AnimName = std::filesystem::path(StateNode->AnimationAsset->GetFilePath()).stem().string();
                            ImGui::Text("Anim: %s", AnimName.c_str());
                            bHasAsset = true;
                        }
                        else if (StateNode->AnimAssetType == EAnimAssetType::BlendSpace2D && StateNode->BlendSpaceAsset)
                        {
                            // BlendSpace2D
                            std::string BSName = std::filesystem::path(StateNode->BlendSpaceAsset->GetFilePath()).stem().string();
                            ImGui::Text("BS2D: %s", BSName.c_str());
                            bHasAsset = true;
                        }
                    }
                }

                if (!bHasAsset)
                {
                    ImGui::Text("(None)");
                }

                ImGui::PopStyleColor();
                ImGui::Unindent(10.0f);

                ImGui::Dummy(ImVec2(0, 4)); // 헤더 하단 여백
            }
            ImGui::EndGroup();

            // 헤더 영역의 크기를 가져옴 (나중에 배경 그릴 때 사용)
            ImRect HeaderRect;
            HeaderRect.Min = ImGui::GetItemRectMin();
            HeaderRect.Max = ImGui::GetItemRectMax();

            // 헤더가 너무 좁으면 최소 너비 보장
            float minWidth = 150.0f;
            if (HeaderRect.GetWidth() < minWidth)
            {
                HeaderRect.Max.x = HeaderRect.Min.x + minWidth;
                // 더미로 공간 확보
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(minWidth - ImGui::GetItemRectSize().x, 0));
            }

			// -------------------------------------------------------
            // [Step 3] 바디 영역 (핀 레이아웃 개선)
            // -------------------------------------------------------
            ImGui::Dummy(ImVec2(0, 8)); // 헤더와 바디 사이 넉넉한 간격

            ImGui::BeginGroup(); // Body Group Start
            {
                // 노드의 최소 너비를 보장하여 핀 사이 간격을 벌림
                float minNodeWidth = 180.0f;
                float currentHeaderWidth = HeaderRect.GetWidth();
                float bodyWidth = std::max(minNodeWidth, currentHeaderWidth);

                // =================================================
                // 1. 왼쪽: Input Pin (Entry)
                // =================================================
                ImGui::BeginGroup();
                for (auto& Pin : Node.Inputs)
                {
                    ed::BeginPin(Pin.ID, Pin.Kind);

                    // [아이콘]
                    bool connected = false;
                    for (const auto& Link : ActiveState->Links)
                        if (Link.EndPinID == Pin.ID) { connected = true; break; }

                    DrawPinIcon(ImVec2(16, 16), connected, IM_COL32(100, 255, 100, 255)); // 입력은 초록색 느낌

                    // [텍스트] 아이콘 바로 옆에 라벨 표시
                    ImGui::SameLine();
                    ImGui::TextDisabled("In");

                    ed::EndPin();
                }
                ImGui::EndGroup();

                // =================================================
                // 2. 중간: 스프링 (Spring) 공간
                // 왼쪽 그룹과 오른쪽 그룹 사이를 강제로 벌림
                // =================================================
                ImGui::SameLine();

                float leftSize = ImGui::GetItemRectSize().x;
                float rightSize = 60.0f; // 오른쪽 핀 그룹 대략적인 크기 예상치

                // 남은 공간 계산 (최소 너비 보장)
                float spacerSize = bodyWidth - leftSize - rightSize;
                if (spacerSize < 20.0f) spacerSize = 20.0f; // 최소 간격

                ImGui::Dummy(ImVec2(spacerSize, 0));

                ImGui::SameLine();

                // =================================================
                // 3. 오른쪽: Output Pin (Transition)
                // =================================================
                ImGui::BeginGroup();
                for (auto& Pin : Node.Outputs)
                {
                    ed::BeginPin(Pin.ID, Pin.Kind);

                    // [텍스트] 출력은 텍스트가 먼저 나오고
                    ImGui::TextDisabled("Out");
                    ImGui::SameLine();

                    // [아이콘] 그 뒤에 아이콘이 나옴 (오른쪽 정렬 효과)
                    bool connected = false;
                    for (const auto& Link : ActiveState->Links)
                        if (Link.StartPinID == Pin.ID) { connected = true; break; }

                    DrawPinIcon(ImVec2(16, 16), connected, IM_COL32(255, 200, 100, 255)); // 출력은 주황색

                    ed::EndPin();
                }
                ImGui::EndGroup();
            }
            ImGui::EndGroup(); // Body Group End

            ImGui::Dummy(ImVec2(0, 8)); // 하단 여백
            ImGui::PopID();

            ed::EndNode();

            // -------------------------------------------------------
            // [Step 4] 배경 그리기 (Node Editor의 DrawList 사용)
            // 노드가 그려진 위치(Content) 뒤에 배경을 덧그림
            // -------------------------------------------------------
            if (ImGui::IsItemVisible())
            {
                ImDrawList* drawList = ed::GetNodeBackgroundDrawList(Node.ID);

                // 노드 전체 영역
                ImVec2 nodeMin = ed::GetNodePosition(Node.ID);
                ImVec2 nodeSize = ed::GetNodeSize(Node.ID);
                ImVec2 nodeMax = ImVec2(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);

                // 헤더 영역 계산 (노드 상단부터 헤더 텍스트 그룹 높이까지)
                // ImGui 아이템 좌표는 스크린 좌표이므로 Editor 좌표계 고려 필요 없지만
                // NodeBackgroundDrawList는 노드 로컬 좌표계가 아님.

                // 헤더 사각형 (위쪽 둥근 모서리)
                float headerHeight = HeaderRect.GetHeight();
                ImVec2 headerMax = ImVec2(nodeMax.x, nodeMin.y + headerHeight);

                // 1. 헤더 배경 (색상 띠)
                // 상단 좌우만 둥글게 (ImDrawFlags_RoundCornersTop)
                drawList->AddRectFilled(
                    nodeMin,
                    headerMax,
                    HeaderColor,
                    ed::GetStyle().NodeRounding,
                    ImDrawFlags_RoundCornersTop
                );

                // 2. 바디 배경 (반투명 검정)
                // 헤더 바로 아래부터 끝까지, 하단 좌우만 둥글게
                drawList->AddRectFilled(
                    ImVec2(nodeMin.x, headerMax.y),
                    nodeMax,
                    IM_COL32(20, 20, 20, 200), // 약간 투명한 검정
                    ed::GetStyle().NodeRounding,
                    ImDrawFlags_RoundCornersBottom
                );

                // 3. 구분선 (헤더와 바디 사이)
                drawList->AddLine(
                    ImVec2(nodeMin.x, headerMax.y),
                    ImVec2(nodeMax.x, headerMax.y),
                    IM_COL32(255, 255, 255, 30), // 희미한 선
                    1.0f
                );

                // 4. 테두리 (선택되었을 때)
                if (ed::IsNodeSelected(Node.ID))
                {
                    drawList->AddRect(
                        nodeMin,
                        nodeMax,
                        IM_COL32(255, 176, 50, 255), // 언리얼 오렌지색 선택 테두리
                        ed::GetStyle().NodeRounding,
                        0,
                        2.0f
                    );
                }
                else
                {
                     // 평소 테두리 (검정)
                    drawList->AddRect(
                        nodeMin,
                        nodeMax,
                        IM_COL32(10, 10, 10, 255),
                        ed::GetStyle().NodeRounding,
                        0,
                        1.0f
                    );
                }
            }
        }

        ed::PopStyleVar(2); // Rounding, Padding 복구

        // 2. Draw Transitions (Links) - 곡선 스타일 개선
    	// 링크를 직선으로 만들기 위해 스타일을 푸시
    	ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f); // 곡률을 0으로 설정하여 직선으로 만듦
    	ed::PushStyleVar(ed::StyleVar_SourceDirection, ImVec2(1.0f, 0.0f)); // 핀 방향성 무시 (직선 강제)
    	ed::PushStyleVar(ed::StyleVar_TargetDirection, ImVec2(-1.0f, 0.0f));

    	for (auto& Link : ActiveState->Links)
    	{
    		ImColor LinkColor = Link.Conditions.empty() ? ImColor(200, 200, 200, 255) : ImColor(100, 200, 255, 255);
    		ed::Link(Link.ID, Link.StartPinID, Link.EndPinID, LinkColor, 1.0f);
    		ed::Flow(Link.ID);
    	}
    	ed::PopStyleVar(3); // LinkStrength, SourceDirection, TargetDirection 복구

        // Handle Creation 부분 예시 (기존 코드 유지)
        if (ed::BeginCreate())
        {
             ed::PinId startPinId, endPinId;
             if (ed::QueryNewLink(&startPinId, &endPinId))
             {
                 FGraphPin* StartPin = FindPin(ActiveState, startPinId);
                 FGraphPin* EndPin = FindPin(ActiveState, endPinId);
                 if (StartPin && EndPin && StartPin->Kind == ed::PinKind::Output && EndPin->Kind == ed::PinKind::Input)
                 {
                     if (ed::AcceptNewItem())
                     {
                         // ... 링크 생성 로직 (기존 동일) ...
                         FGraphLink NewLink;
                         NewLink.ID = ActiveState->GetNextLinkId();
                         NewLink.StartPinID = startPinId;
                         NewLink.EndPinID = endPinId;
                         NewLink.BlendTime = 0.2f;

                         FGraphNode* FromNode = FindNodeByPin(ActiveState, startPinId);
                         FGraphNode* ToNode = FindNodeByPin(ActiveState, endPinId);
                         if (FromNode && ToNode && ActiveState->StateMachine)
                         {
                             ActiveState->StateMachine->AddTransition(FName(FromNode->Name), FName(ToNode->Name), NewLink.BlendTime);
                         }
                         ActiveState->Links.push_back(NewLink);
                     }
                 }
             }
        }
        ed::EndCreate();


        // Selection Logic (기존 동일)
        if (ed::GetSelectedObjectCount() > 0)
        {
            ed::NodeId selectedNodeId;
            if (ed::GetSelectedNodes(&selectedNodeId, 1) > 0) {
                ActiveState->SelectedNodeID = selectedNodeId;
                ActiveState->SelectedLinkID = ed::LinkId::Invalid;
            } else {
                ed::LinkId selectedLinkId;
                if (ed::GetSelectedLinks(&selectedLinkId, 1) > 0) {
                    ActiveState->SelectedLinkID = selectedLinkId;
                    ActiveState->SelectedNodeID = ed::NodeId::Invalid;
                }
            }
        }
        else {
            ActiveState->SelectedNodeID = ed::NodeId::Invalid;
            ActiveState->SelectedLinkID = ed::LinkId::Invalid;
        }


        ed::End();
        ed::SetCurrentEditor(nullptr);
    }

    // 드래그 앤 드롭 타겟 (Content Browser에서 파일 드롭)
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
        {
            const char* filePath = static_cast<const char*>(payload->Data);
            FString path(filePath);
            FString lowerPath = path;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

            if (lowerPath.ends_with(".statemachine"))
            {
                LoadStateMachineFile(filePath);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();
}

void SAnimStateMachineWindow::RenderRightPanel(float width, float height)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("RightPanel", ImVec2(width, height), true);

    ImGui::Spacing();
    ImGui::Indent(5.0f);
    ImGui::TextDisabled("DETAILS");
    ImGui::Unindent(5.0f);
    ImGui::Separator();

    if (!ActiveState)
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    // State Node 선택 시
    if (ActiveState->SelectedNodeID)
    {
        FGraphNode* SelectedNode = FindNode(ActiveState, ActiveState->SelectedNodeID);
        if (SelectedNode)
        {
            ImGui::Spacing();
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "State: %s", SelectedNode->Name.c_str());
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            // State 이름 수정
            char NameBuf[128];
            strcpy_s(NameBuf, SelectedNode->Name.c_str());
            if (ImGui::InputText("State Name", NameBuf, sizeof(NameBuf)))
            {
            	FName OldName(SelectedNode->Name);
            	FName NewName(NameBuf);

            	// 엔진 데이터 갱신 요청
            	if (ActiveState->StateMachine->RenameNode(OldName, NewName))
            	{
            		// 성공 시 UI 노드 이름도 변경
            		SelectedNode->Name = NameBuf;
            	}
            }

            // ==========================================
            // Animation Asset Section
            // ==========================================
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextDisabled("Animation Asset");
            ImGui::Spacing();
            ImGui::Spacing();

            // 현재 애셋 표시
            if (ActiveState->StateMachine)
            {
                FName NodeName(SelectedNode->Name);
                if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(NodeName))
                {
                    // 현재 선택된 애셋 정보 박스
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                    ImGui::BeginChild("AssetInfo", ImVec2(-1, 70), true);

                    if (StateNode->AnimAssetType == EAnimAssetType::AnimSequence && StateNode->AnimationAsset)
                    {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "AnimSequence");
                        ImGui::TextWrapped("%s", StateNode->AnimationAsset->GetFilePath().c_str());
                    }
                    else if (StateNode->AnimAssetType == EAnimAssetType::BlendSpace2D && StateNode->BlendSpaceAsset)
                    {
                        ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "BlendSpace2D");
                        ImGui::TextWrapped("%s", StateNode->BlendSpaceAsset->GetFilePath().c_str());
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No Asset Selected");
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor();

                    // 버튼들
                    float ButtonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

                    if (ImGui::Button("AnimSeq", ImVec2(ButtonWidth, 0)))
                    {
                        ImGui::OpenPopup("AnimSequenceSelector");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Blend2D", ImVec2(ButtonWidth, 0)))
                    {
                        // BlendSpace2D 선택 다이얼로그
                        const FWideString BaseDir = UTF8ToWide(GDataDir) + L"/Blend";
                        const FWideString Extension = L".blend2d";
                        const FWideString Description = L"BlendSpace2D Files";

                        std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialog(BaseDir, Extension, Description);

                        if (!SelectedPath.empty())
                        {
                            FWideString AbsolutePath = SelectedPath.wstring();
                            FString FinalPathStr = ResolveAssetRelativePath(WideToUTF8(AbsolutePath), "");
                            UBlendSpace2D* LoadedBlendSpace = UBlendSpace2D::LoadFromFile(FinalPathStr);

                            if (LoadedBlendSpace)
                            {
                                StateNode->AnimAssetType = EAnimAssetType::BlendSpace2D;
                                StateNode->AnimationAsset = nullptr;
                                StateNode->BlendSpaceAsset = LoadedBlendSpace;
                            }
                            else
                            {
                                UE_LOG("StateMachine: LoadBlendSpace: Failed %s", WideToUTF8(AbsolutePath).c_str());
                            }
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear", ImVec2(ButtonWidth, 0)))
                    {
                        SelectedNode->AnimSequence = nullptr;
                        StateNode->AnimAssetType = EAnimAssetType::None;
                        StateNode->AnimationAsset = nullptr;
                        StateNode->BlendSpaceAsset = nullptr;
                    }

                    // Animation Sequence 선택 팝업
                    if (ImGui::BeginPopup("AnimSequenceSelector"))
                    {
                        ImGui::TextDisabled("Select Animation Sequence");
                        ImGui::Separator();
                        ImGui::Spacing();

                        TArray<UAnimSequence*> Sequences = UResourceManager::GetInstance().GetAll<UAnimSequence>();

                        if (Sequences.empty())
                        {
                            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "No animations available");
                        }
                        else
                        {
                            // 검색 필터
                            static char SearchBuf[128] = "";
                            ImGui::SetNextItemWidth(-1);
                            ImGui::InputTextWithHint("##AnimSearch", "Search...", SearchBuf, sizeof(SearchBuf));
                            ImGui::Spacing();
                            ImGui::Separator();

                            ImGui::BeginChild("AnimList", ImVec2(300, 400), true);

                            for (UAnimSequence* Seq : Sequences)
                            {
                                if (!Seq) continue;

                                FString AnimName = Seq->GetFilePath();
                                // .anim 파일만 표시
                                if (!AnimName.ends_with(".anim")) continue;

                                // 검색 필터 적용
                                if (SearchBuf[0] != '\0')
                                {
                                    std::string NameStr = AnimName.c_str();
                                    std::string SearchStr = SearchBuf;
                                    std::ranges::transform(NameStr, NameStr.begin(), ::tolower);
                                    std::ranges::transform(SearchStr, SearchStr.begin(), ::tolower);

                                    if (NameStr.find(SearchStr) == std::string::npos) { continue; }
                                }

                                bool bIsSelected = (StateNode->AnimationAsset == Seq);

                                if (ImGui::Selectable(AnimName.c_str(), bIsSelected))
                                {
                                    SelectedNode->AnimSequence = Seq;
                                    StateNode->AnimAssetType = EAnimAssetType::AnimSequence;
                                    StateNode->AnimationAsset = Seq;
                                    StateNode->BlendSpaceAsset = nullptr;
                                    ImGui::CloseCurrentPopup();
                                }

                                if (bIsSelected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndChild();
                        }

                        ImGui::Spacing();
                        if (ImGui::Button("Close", ImVec2(-1, 0)))
                        {
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndPopup();
                    }
                }
            }
            else if (SelectedNode->AnimSequence)
            {
                ImGui::Text("Current: %s", SelectedNode->AnimSequence->GetFilePath().c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "No Asset Selected");
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // Loop 옵션
            bool bLoop = SelectedNode->bLoop;
            if (ImGui::Checkbox("Loop Animation", &bLoop))
            {
                SelectedNode->bLoop = bLoop;

                // StateMachine에 반영
                if (ActiveState->StateMachine)
                {
                    FName nodeName(SelectedNode->Name);
                    if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(nodeName))
                    {
                        StateNode->bLoop = bLoop;
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

            // ==========================================
            // State Notifies
            // ==========================================
            if (ActiveState->StateMachine)
            {
                FName nodeName(SelectedNode->Name);
                FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(nodeName);
                if (StateNode)
                {
                    ImGui::TextDisabled("State Notifies");
                    ImGui::Spacing();
                    ImGui::Spacing();

                    // ------------------------------------------
                    // Entry Notifies
                    // ------------------------------------------
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
                    ImGui::Text("Entry Notifies");
                    ImGui::PopStyleColor();
                    ImGui::Spacing();

                    int32 entryNotifyToDelete = -1;
                    for (int32 i = 0; i < StateNode->StateEntryNotifies.Num(); ++i)
                    {
                        FAnimNotifyEvent& Notify = StateNode->StateEntryNotifies[i];
                        ImGui::PushID(("Entry" + std::to_string(i)).c_str());

                        RenderNotifyCombo("##NotifyName", Notify);

                        ImGui::SameLine();
                        if (ImGui::SmallButton("X"))
                        {
                            entryNotifyToDelete = i;
                        }

                        ImGui::PopID();
                    }

                    if (entryNotifyToDelete != -1)
                    {
                        StateNode->StateEntryNotifies.RemoveAt(entryNotifyToDelete);
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("+ Add Entry", ImVec2(-1, 0)))
                    {
                        FAnimNotifyEvent NewNotify;
                        NewNotify.NotifyName = FName("NewEntryNotify");
                        NewNotify.TriggerTime = 0.0f;
                        NewNotify.Duration = 0.0f;
                        StateNode->StateEntryNotifies.Add(NewNotify);
                    }

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::Spacing();

                    // ------------------------------------------
                    // Exit Notifies
                    // ------------------------------------------
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.4f, 1.0f));
                    ImGui::Text("Exit Notifies");
                    ImGui::PopStyleColor();
                    ImGui::Spacing();

                    int32 exitNotifyToDelete = -1;
                    for (int32 i = 0; i < StateNode->StateExitNotifies.Num(); ++i)
                    {
                        FAnimNotifyEvent& Notify = StateNode->StateExitNotifies[i];
                        ImGui::PushID(("Exit" + std::to_string(i)).c_str());

                        RenderNotifyCombo("##NotifyName", Notify);

                        ImGui::SameLine();
                        if (ImGui::SmallButton("X"))
                        {
                            exitNotifyToDelete = i;
                        }

                        ImGui::PopID();
                    }

                    if (exitNotifyToDelete != -1)
                    {
                        StateNode->StateExitNotifies.RemoveAt(exitNotifyToDelete);
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("+ Add Exit", ImVec2(-1, 0)))
                    {
                        FAnimNotifyEvent NewNotify;
                        NewNotify.NotifyName = FName("NewExitNotify");
                        NewNotify.TriggerTime = 0.0f;
                        NewNotify.Duration = 0.0f;
                        StateNode->StateExitNotifies.Add(NewNotify);
                    }

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::Spacing();

                    // ------------------------------------------
                    // Fully Blended Notifies
                    // ------------------------------------------
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                    ImGui::Text("Fully Blended Notifies");
                    ImGui::PopStyleColor();
                    ImGui::Spacing();

                    int32 blendedNotifyToDelete = -1;
                    for (int32 i = 0; i < StateNode->StateFullyBlendedNotifies.Num(); ++i)
                    {
                        FAnimNotifyEvent& Notify = StateNode->StateFullyBlendedNotifies[i];
                        ImGui::PushID(("Blended" + std::to_string(i)).c_str());

                        RenderNotifyCombo("##NotifyName", Notify);

                        ImGui::SameLine();
                        if (ImGui::SmallButton("X"))
                        {
                            blendedNotifyToDelete = i;
                        }

                        ImGui::PopID();
                    }

                    if (blendedNotifyToDelete != -1)
                    {
                        StateNode->StateFullyBlendedNotifies.RemoveAt(blendedNotifyToDelete);
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("+ Add Fully Blended", ImVec2(-1, 0)))
                    {
                        FAnimNotifyEvent NewNotify;
                        NewNotify.NotifyName = FName("NewBlendedNotify");
                        NewNotify.TriggerTime = 0.0f;
                        NewNotify.Duration = 0.0f;
                        StateNode->StateFullyBlendedNotifies.Add(NewNotify);
                    }
                }
            }
        }
    }
	// Transition (Link) 선택 시
    else if (ActiveState->SelectedLinkID)
    {
        FGraphLink* SelectedLink = FindLink(ActiveState, ActiveState->SelectedLinkID);
        if (SelectedLink)
        {
            FGraphNode* FromNode = FindNodeByPin(ActiveState, SelectedLink->StartPinID);
            FGraphNode* ToNode = FindNodeByPin(ActiveState, SelectedLink->EndPinID);

            ImGui::Spacing();
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Transition Details");
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            if (FromNode && ToNode && ActiveState->StateMachine)
            {
                // UI 레이아웃 상수
                const float ContentWidth = ImGui::GetContentRegionAvail().x;

                // 1. 기본 정보 표시 (박스 스타일)
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                ImGui::BeginChild("TransitionInfo", ImVec2(-1, 60), true);
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", FromNode->Name.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled(" -> ");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.4f, 1.0f), "%s", ToNode->Name.c_str());

                    ImGui::Spacing();

                    // Priority와 Blend Time을 한 줄에 배치
                    int32 CurrentPriority = ActiveState->StateMachine->GetTransitionPriority(FName(FromNode->Name), FName(ToNode->Name));

                    ImGui::TextDisabled("Priority:");
                    ImGui::SameLine();
                    ImGui::Text("%d", CurrentPriority);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("^"))
                    {
                        ActiveState->StateMachine->MoveTransitionPriority(FName(FromNode->Name), FName(ToNode->Name), -1);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("v"))
                    {
                        ActiveState->StateMachine->MoveTransitionPriority(FName(FromNode->Name), FName(ToNode->Name), 1);
                    }

                    ImGui::SameLine();
                    ImGui::TextDisabled("  Blend:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(60);
                    float blendTime = SelectedLink->BlendTime;
                    if (ImGui::DragFloat("##BlendTime", &blendTime, 0.01f, 0.0f, 5.0f, "%.2fs"))
                    {
                        SelectedLink->BlendTime = blendTime;
                        if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(FName(FromNode->Name)))
                        {
                            for (auto& Trans : StateNode->Transitions)
                            {
                                if (Trans.TargetStateName == FName(ToNode->Name))
                                {
                                    Trans.BlendTime = blendTime;
                                    break;
                                }
                            }
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ==========================================================
                // Conditions (두 줄 레이아웃)
                // ==========================================================
                ImGui::TextDisabled("CONDITIONS");
                ImGui::Spacing();

                bool bCanEditData = (FromNode && ToNode && ActiveState->StateMachine);
                int conditionToDelete = -1;

                ImGui::PushID("Conditions");
                for (int i = 0; i < SelectedLink->Conditions.size(); ++i)
                {
                    auto& Cond = SelectedLink->Conditions[i];
                    bool bDataChanged = false;

                    ImGui::PushID(i);

                    // 조건 박스 (시각적 구분)
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    ImGui::BeginChild("CondBox", ImVec2(-1, 52), true);
                    {
                        // 첫 번째 줄: Type과 Parameter Name
                        ImGui::TextDisabled("Type:");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(100);
                        if (ImGui::BeginCombo("##Type", GetConditionTypeString(Cond.Type)))
                        {
                            for (int type_i = 0; type_i <= (int)EAnimConditionType::CurrentTime; ++type_i)
                            {
                                EAnimConditionType type = (EAnimConditionType)type_i;
                                const bool bIsSelected = (Cond.Type == type);
                                if (ImGui::Selectable(GetConditionTypeString(type), bIsSelected))
                                {
                                    Cond.Type = type;
                                    bDataChanged = true;
                                }
                                if (bIsSelected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        // Parameter 타입일 때만 파라미터 이름 표시
                        if (Cond.Type == EAnimConditionType::Parameter)
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("Param:");
                            ImGui::SameLine();
                            char paramBuf[128];
                            strcpy_s(paramBuf, Cond.ParameterName.ToString().c_str());
                            ImGui::SetNextItemWidth(80);
                            if (ImGui::InputText("##ParamName", paramBuf, sizeof(paramBuf)))
                            {
                                Cond.ParameterName = paramBuf;
                                bDataChanged = true;
                            }
                        }

                        // 삭제 버튼 (오른쪽 정렬)
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                        if (ImGui::SmallButton("X"))
                        {
                            conditionToDelete = i;
                        }
                        ImGui::PopStyleColor(3);

                        // 두 번째 줄: Operator와 Threshold
                        ImGui::TextDisabled("Compare:");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(50);
                        if (ImGui::BeginCombo("##Op", GetConditionOpString(Cond.Op)))
                        {
                            for (int op_i = 0; op_i <= (int)EAnimConditionOp::LessOrEqual; ++op_i)
                            {
                                EAnimConditionOp op = (EAnimConditionOp)op_i;
                                const bool bIsSelected = (Cond.Op == op);
                                if (ImGui::Selectable(GetConditionOpString(op), bIsSelected))
                                {
                                    Cond.Op = op;
                                    bDataChanged = true;
                                }
                                if (bIsSelected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::SameLine();
                        ImGui::TextDisabled("Value:");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(60);
                        float step = (Cond.Type == EAnimConditionType::TimeRemainingRatio) ? 0.01f : 0.1f;
                        if (ImGui::DragFloat("##Threshold", &Cond.Threshold, step, 0.0f, 0.0f, "%.2f"))
                        {
                            bDataChanged = true;
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();

                    if (bDataChanged && bCanEditData)
                    {
                        FAnimCondition RealCond;
                        RealCond.Type = Cond.Type;
                        RealCond.ParameterName = Cond.ParameterName;
                        RealCond.Op = Cond.Op;
                        RealCond.Threshold = Cond.Threshold;

                        ActiveState->StateMachine->UpdateConditionInTransition(FName(FromNode->Name), FName(ToNode->Name),
                            i, RealCond);
                    }

                    ImGui::PopID();
                }

                // 삭제 로직 실행 (루프 밖에서)
                if (conditionToDelete != -1 && bCanEditData)
                {
                    ActiveState->StateMachine->RemoveConditionFromTransition(
                        FName(FromNode->Name),
                        FName(ToNode->Name),
                        conditionToDelete
                    );
                    SelectedLink->Conditions.erase(SelectedLink->Conditions.begin() + conditionToDelete);
                }
                ImGui::PopID(); // "Conditions"

                ImGui::Spacing();

                if (ImGui::Button("+ Add Condition", ImVec2(-1, 25)))
                {
                    if (bCanEditData)
                    {
                        FAnimCondition NewRealCond;
                        NewRealCond.Type = EAnimConditionType::TimeRemainingRatio;
                        NewRealCond.ParameterName = FName("None");
                        NewRealCond.Op = EAnimConditionOp::Less;
                        NewRealCond.Threshold = 0.1f;

                        ActiveState->StateMachine->AddConditionToTransition(FromNode->Name, ToNode->Name, NewRealCond);
                        SelectedLink->Conditions.push_back(NewRealCond);
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ==========================================================
                // Transition Notifies
                // ==========================================================
                ImGui::TextDisabled("TRANSITION NOTIFIES");
                ImGui::Spacing();

                if (bCanEditData)
                {
                    FName fromName(FromNode->Name);
                    FName toName(ToNode->Name);
                    FAnimStateTransition* Trans = ActiveState->StateMachine->FindTransition(fromName, toName);

                    if (Trans)
                    {
                        ImGui::PushID("TransitionNotifies");
                        int32 notifyToDelete = -1;

                        for (int32 i = 0; i < Trans->TransitionNotifies.Num(); ++i)
                        {
                            FAnimNotifyEvent& Notify = Trans->TransitionNotifies[i];
                            ImGui::PushID(i);

                            // Notify 박스
                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.15f, 0.15f, 1.0f));
                            ImGui::BeginChild("NotifyBox", ImVec2(-1, 28), true);
                            {
                                // Notify Name (드롭다운) - 너비 조정
                                ImGui::SetNextItemWidth(120);
                                RenderNotifyCombo("##NotifyName", Notify);

                                ImGui::SameLine();
                                ImGui::TextDisabled("@");
                                ImGui::SameLine();

                                // Trigger Time
                                ImGui::SetNextItemWidth(50);
                                ImGui::DragFloat("##TriggerTime", &Notify.TriggerTime, 0.01f, 0.0f, 1.0f, "%.2f");
                                if (ImGui::IsItemHovered())
                                {
                                    ImGui::SetTooltip("Transition progress (0.0 ~ 1.0)");
                                }

                                // 삭제 버튼 (오른쪽 정렬)
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                                if (ImGui::SmallButton("X"))
                                {
                                    notifyToDelete = i;
                                }
                                ImGui::PopStyleColor(3);
                            }
                            ImGui::EndChild();
                            ImGui::PopStyleColor();

                            ImGui::PopID();
                        }

                        if (notifyToDelete != -1)
                        {
                            Trans->TransitionNotifies.RemoveAt(notifyToDelete);
                        }
                        ImGui::PopID(); // "TransitionNotifies"

                        ImGui::Spacing();

                        if (ImGui::Button("+ Add Notify", ImVec2(-1, 25)))
                        {
                            FAnimNotifyEvent NewNotify;
                            NewNotify.NotifyName = FName("NewTransitionNotify");
                            NewNotify.TriggerTime = 0.5f;
                            NewNotify.Duration = 0.0f;
                            Trans->TransitionNotifies.Add(NewNotify);
                        }
                    }
                }
            } // if (FromNode && ToNode) 끝
        } // if (SelectedLink) 끝
    } // else if (SelectedLinkID) 끝
    else
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Select a state or transition to view details.");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// === Node Creation ===
void SAnimStateMachineWindow::CreateNode_State(FGraphState* State)
{
    if (!State || !State->StateMachine) return;

    // 고유한 이름 생성
    char StateName[64];
    sprintf_s(StateName, "State_%d", State->StateCounter++);

    // StateMachine에 State 추가
    FAnimStateNode* NewStateNode = State->StateMachine->AddNode(FName(StateName), nullptr, true);

    // Graph에 Node 추가
    FGraphNode Node;
    Node.ID = State->GetNextNodeId();
    Node.Name = StateName;
    Node.AnimSequence = nullptr;
    Node.bLoop = true;

    // Input/Output Pins
    Node.Inputs.emplace_back(State->GetNextPinId(), "In", EPinType::Flow, ed::PinKind::Input);
    Node.Outputs.emplace_back(State->GetNextPinId(), "Out", EPinType::Flow, ed::PinKind::Output);

    State->Nodes.push_back(Node);

    // 위치 설정 (약간씩 떨어뜨려서 생성)
    ed::SetCurrentEditor(State->Context);
    ed::SetNodePosition(Node.ID, ImVec2(100.0f + State->StateCounter * 50.0f, 100.0f + State->StateCounter * 50.0f));
    ed::SetCurrentEditor(nullptr);
}

// === Helper Functions ===
FGraphPin* SAnimStateMachineWindow::FindPin(FGraphState* State, ed::PinId PinID)
{
    if (!State) return nullptr;

    for (auto& Node : State->Nodes)
    {
        for (auto& Pin : Node.Inputs)
            if (Pin.ID == PinID) return &Pin;
        for (auto& Pin : Node.Outputs)
            if (Pin.ID == PinID) return &Pin;
    }
    return nullptr;
}

FGraphNode* SAnimStateMachineWindow::FindNode(FGraphState* State, ed::NodeId NodeID)
{
    if (!State) return nullptr;

    for (auto& Node : State->Nodes)
    {
        if (Node.ID == NodeID) return &Node;
    }
    return nullptr;
}

FGraphNode* SAnimStateMachineWindow::FindNodeByPin(FGraphState* State, ed::PinId PinID)
{
    if (!State) return nullptr;

    for (auto& Node : State->Nodes)
    {
        for (auto& Pin : Node.Inputs)
            if (Pin.ID == PinID) return &Node;
        for (auto& Pin : Node.Outputs)
            if (Pin.ID == PinID) return &Node;
    }
    return nullptr;
}

FGraphLink* SAnimStateMachineWindow::FindLink(FGraphState* State, ed::LinkId LinkID)
{
    if (!State) return nullptr;

    for (auto& Link : State->Links)
    {
        if (Link.ID == LinkID) return &Link;
    }
    return nullptr;
}

const char* SAnimStateMachineWindow::GetConditionOpString(EAnimConditionOp Op)
{
    switch (Op)
    {
    case EAnimConditionOp::Greater: return ">";
    case EAnimConditionOp::Less: return "<";
    case EAnimConditionOp::Equals: return "==";
    case EAnimConditionOp::NotEquals: return "!=";
    case EAnimConditionOp::GreaterOrEqual: return ">=";
    case EAnimConditionOp::LessOrEqual: return "<=";
    default: return "?";
    }
}

const char* SAnimStateMachineWindow::GetConditionTypeString(EAnimConditionType Type)
{
	switch (Type)
	{
	case EAnimConditionType::Parameter:          return "Parameter";
	case EAnimConditionType::TimeRemainingRatio: return "Time Remaining";
	case EAnimConditionType::CurrentTime:        return "Current Time";
	default: return "Unknown";
	}
}

void SAnimStateMachineWindow::SaveNodePositions(FGraphState* State)
{
    if (!State || !State->StateMachine) return;

    ed::SetCurrentEditor(State->Context);

    for (auto& Node : State->Nodes)
    {
        ImVec2 pos = ed::GetNodePosition(Node.ID);
        FName nodeName(Node.Name);
        State->StateMachine->NodePositions[nodeName] = FVector2D(pos.x, pos.y);
    }

    ed::SetCurrentEditor(nullptr);
}

void SAnimStateMachineWindow::SyncGraphFromStateMachine(FGraphState* State)
{
    if (!State || !State->StateMachine) return;

    // 기존 Graph 데이터 클리어
    State->Nodes.clear();
    State->Links.clear();

    ed::SetCurrentEditor(State->Context);

    // StateMachine의 Nodes를 Graph로 변환
    for (auto& Elem : State->StateMachine->GetNodes())
    {
        const FAnimStateNode& StateNode = Elem.second;

        FGraphNode Node;
        Node.ID = State->GetNextNodeId();
        Node.Name = StateNode.StateName.ToString();
        Node.AnimSequence = StateNode.AnimationAsset;
        Node.bLoop = StateNode.bLoop;

        Node.Inputs.emplace_back(State->GetNextPinId(), "In", EPinType::Flow, ed::PinKind::Input);
        Node.Outputs.emplace_back(State->GetNextPinId(), "Out", EPinType::Flow, ed::PinKind::Output);

        State->Nodes.push_back(Node);

        // 위치 복원 - 저장된 위치가 있으면 사용, 없으면 기본 위치
        if (State->StateMachine->NodePositions.Contains(StateNode.StateName))
        {
            const FVector2D& Pos = State->StateMachine->NodePositions[StateNode.StateName];
            ed::SetNodePosition(Node.ID, ImVec2(Pos.X, Pos.Y));
        }
        else
        {
            // 저장된 위치가 없으면 그리드 형태로 배치
            int32 nodeIndex = static_cast<int32>(State->Nodes.size() - 1);
            int32 row = nodeIndex / 3;
            int32 col = nodeIndex % 3;
            ed::SetNodePosition(Node.ID, ImVec2(100.0f + col * 300.0f, 100.0f + row * 250.0f));
        }
    }

    // Transitions를 Links로 변환
    for (auto& NodeElem : State->StateMachine->GetNodes())
    {
        const FAnimStateNode& StateNode = NodeElem.second;
        FGraphNode* FromNode = nullptr;

        for (auto& GNode : State->Nodes)
        {
            if (GNode.Name == StateNode.StateName.ToString())
            {
                FromNode = &GNode;
                break;
            }
        }

        if (!FromNode) continue;

        for (const FAnimStateTransition& Trans : StateNode.Transitions)
        {
            FGraphNode* ToNode = nullptr;
            for (auto& GNode : State->Nodes)
            {
                if (GNode.Name == Trans.TargetStateName.ToString())
                {
                    ToNode = &GNode;
                    break;
                }
            }

            if (!ToNode) continue;

            FGraphLink Link;
            Link.ID = State->GetNextLinkId();
            Link.StartPinID = FromNode->Outputs[0].ID;
            Link.EndPinID = ToNode->Inputs[0].ID;
            Link.BlendTime = Trans.BlendTime;

            // Conditions 복사
            for (const FAnimCondition& Cond : Trans.Conditions)
            {
                Link.Conditions.push_back(Cond);
            }

            State->Links.push_back(Link);
        }
    }

    ed::SetCurrentEditor(nullptr);
}


void SAnimStateMachineWindow::RefreshNotifyClassList()
{
    if (!bNotifyClassListDirty)
    {
        return;
    }

    AvailableNotifyClasses.Empty();

    UWorld* World = GEngine.GetDefaultWorld();
    if (!World)
    {
        return;
    }

    FLuaManager* LuaMgr = World->GetLuaManager();
    if (!LuaMgr)
    {
        return;
    }

    AvailableNotifyClasses = LuaMgr->GetRegisteredNotifyClasses();
    bNotifyClassListDirty = false;
}

void SAnimStateMachineWindow::RenderNotifyCombo(const char* Label, FAnimNotifyEvent& Notify)
{
    RefreshNotifyClassList();

    FString CurrentName = Notify.NotifyName.ToString();

    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo(Label, CurrentName.c_str()))
    {
        for (const FString& ClassName : AvailableNotifyClasses)
        {
            bool bIsSelected = (CurrentName == ClassName);
            if (ImGui::Selectable(ClassName.c_str(), bIsSelected))
            {
                Notify.NotifyName = FName(ClassName);
            }

            if (bIsSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
}
