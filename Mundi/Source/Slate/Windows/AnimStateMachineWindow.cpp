#include "pch.h"
#include "AnimStateMachineWindow.h"
#include "AnimStateMachine.h"
#include "AnimSequence.h"
#include "FBXLoader.h"
#include "PlatformProcess.h"
#include "USlateManager.h"

namespace ed = ax::NodeEditor;

SAnimStateMachineWindow::SAnimStateMachineWindow()
{
    Rect = { 0, 0, 0, 0 };

	// TEST
	UFbxLoader& FbxLoader = UFbxLoader::GetInstance();
	USkeletalMesh* SkeletalMesh = FbxLoader.LoadFbxMesh(GDataDir + "/X Bot.fbx");
	TArray<UAnimSequence*> IdleAnims = FbxLoader.LoadAllFbxAnimations(GDataDir + "/Animation/XBOT_Idle.fbx", *SkeletalMesh->GetSkeleton());
	TArray<UAnimSequence*> WalkAnims = FbxLoader.LoadAllFbxAnimations(GDataDir + "/Animation/XBOT_Walking.fbx", *SkeletalMesh->GetSkeleton());
	TArray<UAnimSequence*> RunAnims = FbxLoader.LoadAllFbxAnimations(GDataDir + "/Animation/XBOT_Slow Run.fbx", *SkeletalMesh->GetSkeleton());
	// TEST
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

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }

    if (ImGui::Begin("Animation State Machine Editor", &bIsOpen, flags))
    {
        // === Menu Bar ===
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
            	const FWideString BaseDir = UTF8ToWide(GDataDir) + L"/Animation";
            	const FWideString Extension = L".statemachine";
            	const FWideString Description = L"State Machines";
            	FWideString DefaultFileName = L"NewStateMachine";

                if (ImGui::MenuItem("New State Machine"))
                {
                    char label[64];
                    sprintf_s(label, "State Machine %d", static_cast<int32>(Tabs.size()) + 1);
					CreateNewGraphTab(label, nullptr);
                }
            	// [Save]
            	if (ImGui::MenuItem("Save", "Ctrl+S"))
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
            					if (ActiveState->StateMachine->SaveToFile(SavePath))
            					{
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
            	// [Load] - 변경된 핵심 부분
            	if (ImGui::MenuItem("Load", "Ctrl+O"))
            	{
            		std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialog(BaseDir, Extension, Description);

            		if (!SelectedPath.empty())
            		{
            			FWideString FilePath = SelectedPath.wstring();
            			UAnimStateMachine* LoadedAsset = RESOURCE.Load<UAnimStateMachine>(FilePath);

            			if (LoadedAsset)
            			{
            				// @TODO - 이미 열려있는지 확인
            				// 로드된 에셋으로 새 탭 생성
            				std::string FileName = SelectedPath.stem().string();
            				CreateNewGraphTab(FileName.c_str(), LoadedAsset, FilePath);
            			}
            			else
            			{
            				UE_LOG("[Error] Failed to load AnimStateMachine: %S", FilePath.c_str());
            			}
            		}
            	}
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

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

void SAnimStateMachineWindow::RenderCenterPanel(float width, float height)
{
    ImGui::BeginChild("GraphArea", ImVec2(width, height), true);

    if (ActiveState)
    {
        ed::SetCurrentEditor(ActiveState->Context);
        ed::Begin("StateGraph", ImVec2(0.0, 0.0f));

        // 1. Draw State Nodes
        for (auto& Node : ActiveState->Nodes)
        {
            // Entry State는 초록색으로 표시
            bool bIsEntry = false;
            if (ActiveState->StateMachine)
            {
                bIsEntry = (ActiveState->StateMachine->GetEntryStateName().ToString() == Node.Name);
            }

            ed::PushStyleColor(ed::StyleColor_NodeBg, bIsEntry ?
                ImVec4(0.2f, 0.6f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.4f, 1.0f));

            ed::BeginNode(Node.ID);

            ImGui::PushItemWidth(150.0f);
            ImGui::Text(Node.Name.c_str());
            ImGui::PopItemWidth();

            // State 정보 표시
            ImGui::Spacing();
            if (Node.AnimSequence)
            {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Anim: %s",
                    Node.AnimSequence->GetFilePath().c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "No Animation");
            }

            if (Node.bLoop)
            {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[LOOP]");
            }

            // Input/Output Pins (상태 간 연결용)
            ImGui::BeginGroup();
            for (auto& Pin : Node.Inputs)
            {
                ed::BeginPin(Pin.ID, Pin.Kind);
                ImGui::Text("-> In  ");
                ed::EndPin();
            }
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            for (auto& Pin : Node.Outputs)
            {
                ed::BeginPin(Pin.ID, Pin.Kind);
                ImGui::Text("  Out ->");
                ed::EndPin();
            }
            ImGui::EndGroup();

            ed::EndNode();
            ed::PopStyleColor();
        }

        // 2. Draw Transitions (Links)
        for (auto& Link : ActiveState->Links)
        {
            // Transition 정보에 따라 색상 다르게 표시
            ImColor linkColor = Link.Conditions.empty() ?
                ImColor(150, 150, 150) : ImColor(100, 200, 255);

            ed::Link(Link.ID, Link.StartPinID, Link.EndPinID, linkColor, 2.0f);
        }

        // 3. Handle Creation (State 간 Transition 연결)
        if (ed::BeginCreate())
        {
            ed::PinId startPinId, endPinId;
            if (ed::QueryNewLink(&startPinId, &endPinId))
            {
                // 출력 핀에서 입력 핀으로만 연결 가능
                FGraphPin* StartPin = FindPin(ActiveState, startPinId);
                FGraphPin* EndPin = FindPin(ActiveState, endPinId);

                if (StartPin && EndPin &&
                    StartPin->Kind == ed::PinKind::Output &&
                    EndPin->Kind == ed::PinKind::Input)
                {
                    if (ed::AcceptNewItem())
                    {
                        // GraphLink 생성
                        FGraphLink NewLink;
                        NewLink.ID = ActiveState->GetNextLinkId();
                        NewLink.StartPinID = startPinId;
                        NewLink.EndPinID = endPinId;
                        NewLink.BlendTime = 0.2f;

                        // 실제 State Machine에 Transition 추가
                        FGraphNode* FromNode = FindNodeByPin(ActiveState, startPinId);
                        FGraphNode* ToNode = FindNodeByPin(ActiveState, endPinId);

                        if (FromNode && ToNode && ActiveState->StateMachine)
                        {
                            FName fromName(FromNode->Name);
                            FName toName(ToNode->Name);
                            ActiveState->StateMachine->AddTransition(fromName, toName, NewLink.BlendTime);
                        }

                        ActiveState->Links.push_back(NewLink);
                    }
                }
            }
        }
        ed::EndCreate();

        // 4. Handle Deletion
		if (ed::BeginDelete())
		{
		    // ==============================================================
		    // 1. 링크(Transition) 삭제 처리
		    // ==============================================================
		    ed::LinkId deletedLinkId;
		    while (ed::QueryDeletedLink(&deletedLinkId))
		    {
		        if (ed::AcceptDeletedItem())
		        {
		            // UI에서 링크 객체 찾기
		            FGraphLink* Link = FindLink(ActiveState, deletedLinkId);
		            if (Link)
		            {
		                FGraphNode* FromNode = FindNodeByPin(ActiveState, Link->StartPinID);
		                FGraphNode* ToNode = FindNodeByPin(ActiveState, Link->EndPinID);

		                if (FromNode && ToNode && ActiveState->StateMachine)
		                {
		                    FName FromName(FromNode->Name);
		                    FName ToName(ToNode->Name);
		                	ActiveState->StateMachine->RemoveTransition(FromName, ToName);
		                }
		            }

		            // UI 데이터 업데이트: 링크 목록에서 제거
		            auto& Links = ActiveState->Links;
		            std::erase_if(Links,
		                          [deletedLinkId](const FGraphLink& L) { return L.ID == deletedLinkId; });
		        }
		    }

		    // ==============================================================
		    // 2. 노드(State) 삭제 처리
		    // ==============================================================
		    ed::NodeId deletedNodeId;
		    while (ed::QueryDeletedNode(&deletedNodeId))
		    {
		        if (ed::AcceptDeletedItem())
		        {
		            FGraphNode* Node = FindNode(ActiveState, deletedNodeId);

		            if (Node && ActiveState->StateMachine)
		            {
		                ActiveState->StateMachine->RemoveNode(FName(Node->Name));
		            }

		            auto& Links = ActiveState->Links;
		            std::erase_if(Links,
		                          [&](const FGraphLink& L) {
			                          FGraphNode* StartNode = FindNodeByPin(ActiveState, L.StartPinID);
			                          FGraphNode* EndNode = FindNodeByPin(ActiveState, L.EndPinID);
			                          // 링크의 시작이나 끝이 삭제된 노드라면 UI에서 제거
			                          return (StartNode && StartNode->ID == deletedNodeId) ||
				                          (EndNode && EndNode->ID == deletedNodeId);
						}
		            );

		            // UI 데이터 업데이트: 노드 목록에서 제거
		            auto& Nodes = ActiveState->Nodes;
		            std::erase_if(Nodes,
		                          [deletedNodeId](const FGraphNode& N) { return N.ID == deletedNodeId; });
		        }
		    }
		}
		ed::EndDelete();

        // 5. Selection 처리
        if (ed::GetSelectedObjectCount() > 0)
        {
            ed::NodeId selectedNodeId;
            if (ed::GetSelectedNodes(&selectedNodeId, 1) > 0)
            {
                ActiveState->SelectedNodeID = selectedNodeId;
                ActiveState->SelectedLinkID = ed::LinkId::Invalid;
            }
            else
            {
                ed::LinkId selectedLinkId;
                if (ed::GetSelectedLinks(&selectedLinkId, 1) > 0)
                {
                    ActiveState->SelectedLinkID = selectedLinkId;
                    ActiveState->SelectedNodeID = ed::NodeId::Invalid;
                }
            }
        }
        else
        {
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
            if (ImGui::Button("Select Animation...", ImVec2(-1, 0)))
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
                                    StateNode->AnimationAsset = Seq;
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

            if (SelectedNode->AnimSequence)
            {
                ImGui::Text("Current: %s", SelectedNode->AnimSequence->GetFilePath().c_str());

                ImGui::SameLine();
                if (ImGui::SmallButton("Clear"))
                {
                    SelectedNode->AnimSequence = nullptr;

                    // StateMachine에 반영
                    if (ActiveState->StateMachine)
                    {
                        FName nodeName(SelectedNode->Name);
                        if (FAnimStateNode* StateNode = ActiveState->StateMachine->GetNodes().Find(nodeName))
                        {
                            StateNode->AnimationAsset = nullptr;
                        }
                    }
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "No Animation Selected");
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
                FGraphNode* FromNode = FindNodeByPin(ActiveState, SelectedLink->StartPinID);
                FGraphNode* ToNode = FindNodeByPin(ActiveState, SelectedLink->EndPinID);
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

    // 위치 설정
    ed::SetCurrentEditor(State->Context);
    ed::SetNodePosition(Node.ID, ImVec2(100 + State->StateCounter * 50, 100));
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

        // 위치 복원
        if (State->StateMachine->NodePositions.Contains(StateNode.StateName))
        {
            const FVector2D& Pos = State->StateMachine->NodePositions[StateNode.StateName];
            ed::SetNodePosition(Node.ID, ImVec2(Pos.X, Pos.Y));
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
