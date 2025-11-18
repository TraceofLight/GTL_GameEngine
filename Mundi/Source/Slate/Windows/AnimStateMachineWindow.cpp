#include "pch.h"
#include "AnimStateMachineWindow.h"
#include "AnimStateMachine.h"
#include "AnimSequence.h"
#include "BlendSpace2D.h"
#include "FBXLoader.h"
#include "PlatformProcess.h"
#include "USlateManager.h"
#include <commdlg.h>  // Windows 파일 다이얼로그

namespace ed = ax::NodeEditor;

SAnimStateMachineWindow::SAnimStateMachineWindow()
{
    Rect = { 0, 0, 0, 0 };
}

SAnimStateMachineWindow::~SAnimStateMachineWindow()
{
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

    // 기본 탭 하나 생성
	CreateNewGraphTab("New State Machine", nullptr);

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
        const FWideString BaseDir = UTF8ToWide(GDataDir) + L"/Animation";
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
                            UE_LOG("StateMachine saved: %S", SavePath.c_str());
                        }
                        else
                        {
                            UE_LOG("[error] StateMachine save failed.");
                        }
                    }
                    catch (const std::exception& Exception)
                    {
                        UE_LOG("[error] StateMachine Save Error: %s", Exception.what());
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
                    UE_LOG("[Error] Failed to load AnimStateMachine: %S", FinalPathStr.c_str());
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
                char label[64];
                sprintf_s(label, "State Machine %d", (int)Tabs.size() + 1);
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
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    ImRect rect(cursorPos, cursorPos + size);
    float centerX = (rect.Min.x + rect.Max.x) * 0.5f;
    float centerY = (rect.Min.y + rect.Max.y) * 0.5f;

    // 삼각형 그리기 (Flow 타입)
    const float triangleSize = size.x * 0.6f;
    const float halfSize = triangleSize * 0.5f;

    ImVec2 p1(centerX - halfSize, centerY - halfSize * 0.866f);
    ImVec2 p2(centerX + halfSize, centerY);
    ImVec2 p3(centerX - halfSize, centerY + halfSize * 0.866f);

    if (filled)
    {
        drawList->AddTriangleFilled(p1, p2, p3, color);
    }
    else
    {
        drawList->AddTriangle(p1, p2, p3, color, 2.0f);
    }

    ImGui::Dummy(size);
}

void SAnimStateMachineWindow::RenderCenterPanel(float width, float height)
{
    ImGui::BeginChild("GraphArea", ImVec2(width, height), true);

    if (ActiveState)
    {
        ed::SetCurrentEditor(ActiveState->Context);
        ed::Begin("StateGraph", ImVec2(0.0, 0.0f));

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
            ImGui::PushID(Node.ID.Get());

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
        for (auto& Link : ActiveState->Links)
        {
            ImColor linkColor = Link.Conditions.empty() ?
                ImColor(200, 200, 200, 255) : ImColor(100, 200, 255, 255); // 조건 있으면 파란색

            // 두께를 약간 두껍게
            ed::Link(Link.ID, Link.StartPinID, Link.EndPinID, linkColor, 3.0f);
        }

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

        if (ed::BeginDelete())
        {
             // ... 삭제 로직 (기존 동일) ...
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

            ImGui::Spacing();

            // Animation Sequence 선택
            if (ImGui::Button("Select AnimSequence...", ImVec2(-1, 0)))
            {
                ImGui::OpenPopup("AnimSequenceSelector");
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

                        // 검색 필터 적용
                        if (SearchBuf[0] != '\0')
                        {
                            std::string NameStr = AnimName.c_str();
                            std::string SearchStr = SearchBuf;
                            // 대소문자 구분 없이 검색
                            std::ranges::transform(NameStr, NameStr.begin(), ::tolower);
                            std::ranges::transform(SearchStr, SearchStr.begin(), ::tolower);

                            if (NameStr.find(SearchStr) == std::string::npos) { continue; }
                        }

                        bool bIsSelected = (SelectedNode->AnimSequence == Seq);

                        if (ImGui::Selectable(AnimName.c_str(), bIsSelected))
                        {
                            SelectedNode->AnimSequence = Seq;

                            // StateMachine에 반영
                            if (ActiveState->StateMachine)
                            {
                                FName NodeName(SelectedNode->Name);
                                if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(NodeName))
                                {
                                    StateNode->AnimAssetType = EAnimAssetType::AnimSequence;
                                    StateNode->AnimationAsset = Seq;
                                    StateNode->BlendSpaceAsset = nullptr;
                                }
                            }

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

            // BlendSpace2D 선택
        	if (ImGui::Button("Select BlendSpace2D...", ImVec2(-1, 0)))
        	{
        		// 1. 다이얼로그 설정
        		const FWideString BaseDir = UTF8ToWide(GDataDir) + L"/Animation";
        		const FWideString Extension = L".blend2d";
        		const FWideString Description = L"BlendSpace2D Files";

        		// 2. 파일 선택 다이얼로그 열기 (SelectedPath는 ABSOLUTE PATH)
        		std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialog(BaseDir, Extension, Description);

        		if (!SelectedPath.empty())
        		{
        			// Absolute Path
        			FWideString AbsolutePath = SelectedPath.wstring();

        			// Relative Path
        			FString FinalPathStr = ResolveAssetRelativePath(WideToUTF8(AbsolutePath), "");

        			UBlendSpace2D* LoadedBlendSpace = UBlendSpace2D::LoadFromFile(FinalPathStr);

        			if (LoadedBlendSpace)
        			{
        				// StateMachine에 반영
        				if (ActiveState->StateMachine)
        				{
        					FName NodeName(SelectedNode->Name);
        					if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(NodeName))
        					{
        						// StateNode에 로드된 에셋 포인터 할당
        						StateNode->AnimAssetType = EAnimAssetType::BlendSpace2D;
        						StateNode->AnimationAsset = nullptr;
        						StateNode->BlendSpaceAsset = LoadedBlendSpace;
        					}
        				}
        			}
        			else
        			{
        				UE_LOG("[Error] Failed to load BlendSpace2D from file: %S", AbsolutePath.c_str());
        			}
        		}
        	}

            // 현재 애셋 표시
            if (ActiveState->StateMachine)
            {
                FName NodeName(SelectedNode->Name);
                if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(NodeName))
                {
                    if (StateNode->AnimAssetType == EAnimAssetType::AnimSequence && StateNode->AnimationAsset)
                    {
                        ImGui::Text("Type: AnimSequence");
                        ImGui::Text("Current: %s", StateNode->AnimationAsset->GetFilePath().c_str());
                    }
                    else if (StateNode->AnimAssetType == EAnimAssetType::BlendSpace2D && StateNode->BlendSpaceAsset)
                    {
                        ImGui::Text("Type: BlendSpace2D");
                        ImGui::Text("Current: %s", StateNode->BlendSpaceAsset->GetFilePath().c_str());
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "No Asset Selected");
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear"))
                    {
                        SelectedNode->AnimSequence = nullptr;
                        StateNode->AnimAssetType = EAnimAssetType::None;
                        StateNode->AnimationAsset = nullptr;
                        StateNode->BlendSpaceAsset = nullptr;
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
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Transition");
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            if (FromNode && ToNode)
            {
                ImGui::Text("From: %s", FromNode->Name.c_str());
                ImGui::Text("To: %s", ToNode->Name.c_str());
                ImGui::Separator();
                ImGui::Spacing();

                // Blend Time
                float blendTime = SelectedLink->BlendTime;
                if (ImGui::DragFloat("Blend Time", &blendTime, 0.01f, 0.0f, 5.0f))
                {
                    SelectedLink->BlendTime = blendTime;

                    // StateMachine에 반영
                    if (ActiveState->StateMachine)
                    {
                        FName fromName(FromNode->Name);
                        FName toName(ToNode->Name);
                        if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(fromName))
                        {
                            for (auto& Trans : StateNode->Transitions)
                            {
                                if (Trans.TargetStateName == toName)
                                {
                                    Trans.BlendTime = blendTime;
                                    break;
                                }
                            }
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ==========================================================
                // Conditions (수정된 로직)
                // ==========================================================
                ImGui::TextDisabled("Transition Conditions");
                ImGui::Spacing();

                // StateMachine에 즉시 반영하기 위해 From/To 노드 정보 미리 찾기
                bool bCanEditData = (FromNode && ToNode && ActiveState->StateMachine);

                // Condition 리스트 표시 및 편집
                int conditionToDelete = -1; // 삭제할 인덱스 마킹

                for (int i = 0; i < SelectedLink->Conditions.size(); ++i)
                {
                    auto& Cond = SelectedLink->Conditions[i];
                    bool bDataChanged = false;

                    ImGui::PushID(i); // 각 라인의 위젯 ID가 겹치지 않게 Push

                    // 1. Parameter Name (InputText)
                    char paramBuf[128];
                    strcpy_s(paramBuf, Cond.ParameterName.ToString().c_str());
                    ImGui::SetNextItemWidth(120);
                    if (ImGui::InputText("##ParamName", paramBuf, sizeof(paramBuf)))
                    {
                        Cond.ParameterName = paramBuf;
                        bDataChanged = true;
                    }

                    ImGui::SameLine();

                    // 2. Operator (Combo)
                    ImGui::SetNextItemWidth(80);
                    if (ImGui::BeginCombo("##Op", GetConditionOpString(Cond.Op)))
                    {
                        // EAnimConditionOp의 모든 항목을 순회
                        for (int op_i = 0; op_i <= (int)EAnimConditionOp::LessOrEqual; ++op_i)
                        {
                            EAnimConditionOp op = (EAnimConditionOp)op_i;
                            const bool bIsSelected = (Cond.Op == op);
                            if (ImGui::Selectable(GetConditionOpString(op), bIsSelected))
                            {
                                Cond.Op = op;
                                bDataChanged = true;
                            }
                            if (bIsSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::SameLine();

                    // 3. Threshold (DragFloat)
                    ImGui::SetNextItemWidth(80);
                    if (ImGui::DragFloat("##Threshold", &Cond.Threshold, 0.1f))
                    {
                        bDataChanged = true;
                    }

                    ImGui::SameLine();

                    // 4. Delete Button
                    if (ImGui::SmallButton("X"))
                    {
                        conditionToDelete = i; // 즉시 삭제하지 않고 마킹
                    }

                    // [데이터 동기화] 변경 사항이 생기면 StateMachine에 즉시 업데이트
                    if (bDataChanged && bCanEditData)
                    {
                        FAnimCondition RealCond(
                            FName(Cond.ParameterName),
                            Cond.Op,
                            Cond.Threshold
                        );
                        ActiveState->StateMachine->UpdateConditionInTransition(
                            FName(FromNode->Name),
                            FName(ToNode->Name),
                            i,
                            RealCond
                        );
                    }

                    ImGui::PopID(); // PushID 해제
                }

                // 삭제 로직 실행 (루프 밖에서)
                if (conditionToDelete != -1 && bCanEditData)
                {
                    // 1. StateMachine에서 삭제
                    ActiveState->StateMachine->RemoveConditionFromTransition(
                        FName(FromNode->Name),
                        FName(ToNode->Name),
                        conditionToDelete
                    );
                    // 2. UI 리스트에서 삭제
                    SelectedLink->Conditions.erase(SelectedLink->Conditions.begin() + conditionToDelete);
                }

                ImGui::Spacing();

                if (ImGui::Button("Add Condition", ImVec2(-1, 0)))
                {
                    if (bCanEditData)
                    {
                        // StateMachine에 추가
                        FAnimCondition NewRealCond(FName("NewParam"), EAnimConditionOp::Greater, 0.0f);
                        ActiveState->StateMachine->AddConditionToTransition(FromNode->Name, ToNode->Name, NewRealCond);

                        // UI 리스트에도 추가
                        SelectedLink->Conditions.push_back(NewRealCond);
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
    ed::SetNodePosition(Node.ID, ImVec2(100 + State->StateCounter * 50, 100 + State->StateCounter * 50));
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
            int nodeIndex = State->Nodes.size() - 1;
            int row = nodeIndex / 3;
            int col = nodeIndex % 3;
            ed::SetNodePosition(Node.ID, ImVec2(100 + col * 300, 100 + row * 250));
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
