#include "pch.h"
#include "AnimStateMachine.h"
#include "AnimSequence.h"
#include "BlendSpace2D.h"
#include <algorithm>

IMPLEMENT_CLASS(UAnimStateMachine)

UAnimStateMachine::UAnimStateMachine() = default;

FAnimStateNode* UAnimStateMachine::AddNode(FName NodeName, UAnimSequence* InAnim, bool bInLoop)
{
	FAnimStateNode NewNode(NodeName);
	NewNode.AnimAssetType = EAnimAssetType::AnimSequence;
	NewNode.AnimationAsset = InAnim;
	NewNode.BlendSpaceAsset = nullptr;
	NewNode.bLoop = bInLoop;

	Nodes.Add(NodeName, NewNode);
	return &Nodes[NodeName];
}

FAnimStateNode* UAnimStateMachine::AddNodeWithBlendSpace(FName NodeName, UBlendSpace2D* InBlendSpace, bool bInLoop)
{
	FAnimStateNode NewNode(NodeName);
	NewNode.AnimAssetType = EAnimAssetType::BlendSpace2D;
	NewNode.AnimationAsset = nullptr;
	NewNode.BlendSpaceAsset = InBlendSpace;
	NewNode.bLoop = bInLoop;

	Nodes.Add(NodeName, NewNode);
	return &Nodes[NodeName];
}

bool UAnimStateMachine::RemoveNode(FName NodeName)
{
	if (!Nodes.Contains(NodeName)) { return false; }

	// 이 노드를 타겟으로 하는 다른 노드들의 트랜지션 제거
	for (auto& Elem : Nodes)
	{
		FAnimStateNode& Node = Elem.second;
		for (int32 Idx = Node.Transitions.Num() - 1; Idx >= 0; Idx--)
		{
			if (Node.Transitions[Idx].TargetStateName == NodeName)
			{
				Node.Transitions.RemoveAt(Idx);
			}
		}
	}

	if (EntryStateName == NodeName) { EntryStateName = FName(); }
	NodePositions.Remove(NodeName);
	Nodes.Remove(NodeName);

	return true;
}

bool UAnimStateMachine::RenameNode(FName OldName, FName NewName)
{
	// 이름이 같거나, 원본이 없거나, 새 이름이 이미 있으면 실패
	if (OldName == NewName) { return true; }
	if (!Nodes.Contains(OldName)) { return false; }
	if (Nodes.Contains(NewName)) { return false; }

	FAnimStateNode NodeData = Nodes[OldName];
	NodeData.StateName = NewName;

	Nodes.Remove(OldName);
	Nodes.Add(NewName, NodeData);

	// 에디터 위치 데이터 이동
	if (NodePositions.Contains(OldName))
	{
		FVector2D Pos = NodePositions[OldName];
		NodePositions.Remove(OldName);
		NodePositions.Add(NewName, Pos);
	}

	// 이 노드를 가리키던 트랜지션의 타겟 이름을 갱신
	for (auto& Elem : Nodes)
	{
		FAnimStateNode& CurrNode = Elem.second;
		for (FAnimStateTransition& Trans : CurrNode.Transitions)
		{
			if (Trans.TargetStateName == OldName)
			{
				Trans.TargetStateName = NewName;
			}
		}
	}

	if (EntryStateName == OldName)
	{
		EntryStateName = NewName;
	}
	return true;
}

FAnimStateTransition* UAnimStateMachine::AddTransition(FName FromState, FName ToState, float BlendTime)
{
	if (FAnimStateNode* Node = Nodes.Find(FromState))
	{
		Node->Transitions.Add(FAnimStateTransition(ToState, BlendTime));
		return &Node->Transitions[Node->Transitions.Num() - 1];
	}
	return nullptr;
}

bool UAnimStateMachine::RemoveTransition(FName FromState, FName ToState)
{
	FAnimStateNode* Node = Nodes.Find(FromState);
	if (!Node) { return false; }

	uint32 RemovedCount = 0;
	for (int32 Idx = Node->Transitions.Num() - 1; Idx >= 0; Idx--)
	{
		if (Node->Transitions[Idx].TargetStateName == ToState)
		{
			Node->Transitions.RemoveAt(Idx);
			RemovedCount++;
		}
	}

	return RemovedCount > 0;
}

FAnimStateTransition* UAnimStateMachine::FindTransition(FName FromState, FName ToState)
{
	FAnimStateNode* Node = Nodes.Find(FromState);
	if (!Node) { return nullptr; }

	for (auto& Trans : Node->Transitions)
	{
		if (Trans.TargetStateName == ToState)
		{
			return &Trans;
		}
	}
	return nullptr;
}

bool UAnimStateMachine::AddConditionToTransition(FName FromState, FName ToState, const FAnimCondition& Condition)
{
	FAnimStateNode* Node = Nodes.Find(FromState);
	if (!Node) return false;

	for (FAnimStateTransition& Trans : Node->Transitions)
	{
		if (Trans.TargetStateName == ToState)
		{
			Trans.Conditions.Add(Condition);
			return true;
		}
	}

	return false;
}

bool UAnimStateMachine::RemoveConditionFromTransition(FName FromState, FName ToState, int32 ConditionIndex)
{
	FAnimStateTransition* Trans = FindTransition(FromState, ToState);

	if (Trans && Trans->Conditions.Num() > ConditionIndex && ConditionIndex >= 0)
	{
		Trans->Conditions.RemoveAt(ConditionIndex);
		return true;
	}
	return false;
}

bool UAnimStateMachine::UpdateConditionInTransition(FName FromState, FName ToState, int32 ConditionIndex, const FAnimCondition& NewCondition)
{
	FAnimStateTransition* Trans = FindTransition(FromState, ToState);

	if (Trans && Trans->Conditions.Num() > ConditionIndex && ConditionIndex >= 0)
	{
		Trans->Conditions[ConditionIndex] = NewCondition;
		return true;
	}
	return false;
}

bool UAnimStateMachine::MoveTransitionPriority(FName FromState, FName ToState, int32 Direction)
{
	FAnimStateNode* Node = Nodes.Find(FromState);
	if (!Node) return false;

	// 1. 현재 트랜지션의 인덱스 찾기
	int32 CurrentIndex = -1;
	for (int32 i = 0; i < Node->Transitions.Num(); ++i)
	{
		if (Node->Transitions[i].TargetStateName == ToState)
		{
			CurrentIndex = i;
			break;
		}
	}

	if (CurrentIndex == -1) return false;

	// 2. 이동할 새 인덱스 계산
	int32 NewIndex = CurrentIndex + Direction;

	// 3. 범위 체크 (맨 위에서 더 올리거나, 맨 아래에서 더 내릴 수 없음)
	if (NewIndex < 0 || NewIndex >= Node->Transitions.Num())
	{
		return false; // 이동 불가
	}

	// 4. 배열 내에서 위치 교환 (Swap)
	std::swap(Node->Transitions[CurrentIndex], Node->Transitions[NewIndex]);

	return true;
}

int32 UAnimStateMachine::GetTransitionPriority(FName FromState, FName ToState)
{
	FAnimStateNode* Node = Nodes.Find(FromState);
	if (!Node) return -1;

	for (int32 i = 0; i < Node->Transitions.Num(); ++i)
	{
		if (Node->Transitions[i].TargetStateName == ToState)
		{
			return i; // 0이 가장 높은 우선순위
		}
	}
	return -1;
}

bool UAnimStateMachine::SaveToFile(const FWideString& Path)
{
    JSON Root = JSON::Make(JSON::Class::Object);

    // 1. Entry State 저장
    Root["EntryState"] = EntryStateName.ToString();

    // 2. Nodes 배열 생성
    JSON NodesArray = JSON::Make(JSON::Class::Array);
    JSON TransitionsArray = JSON::Make(JSON::Class::Array); // 트랜지션은 별도로 모음

    for (auto& Elem : Nodes)
    {
        const FAnimStateNode& Node = Elem.second;
        JSON NodeJson = JSON::Make(JSON::Class::Object);

        NodeJson["Name"] = Node.StateName.ToString();
        NodeJson["AnimAssetType"] = static_cast<int32>(Node.AnimAssetType);

        // 경로를 슬래시(/)로 정규화하여 저장
        FString AnimAssetPath = Node.AnimationAsset ? Node.AnimationAsset->GetFilePath() : "";
        std::replace(AnimAssetPath.begin(), AnimAssetPath.end(), '\\', '/');
        NodeJson["AnimAsset"] = AnimAssetPath;

        FString BlendSpacePath = Node.BlendSpaceAsset ? Node.BlendSpaceAsset->GetFilePath() : "";
        std::replace(BlendSpacePath.begin(), BlendSpacePath.end(), '\\', '/');
        NodeJson["BlendSpaceAsset"] = BlendSpacePath;

        NodeJson["Loop"] = Node.bLoop;

        // 에디터 위치 저장
        if (NodePositions.Contains(Node.StateName))
        {
            const FVector2D& Pos = NodePositions[Node.StateName];
            NodeJson["EditorPosX"] = Pos.X;
            NodeJson["EditorPosY"] = Pos.Y;
        }

        // State Notify 저장
        JSON StateEntryArray = JSON::Make(JSON::Class::Array);
        for (const FAnimNotifyEvent& Notify : Node.StateEntryNotifies)
        {
            JSON NotifyJson = JSON::Make(JSON::Class::Object);
            NotifyJson["Name"] = Notify.NotifyName.ToString();
            NotifyJson["Time"] = Notify.TriggerTime;
            NotifyJson["Duration"] = Notify.Duration;
            NotifyJson["TrackIndex"] = Notify.TrackIndex;
            NotifyJson["PropertyData"] = Notify.PropertyData;
            StateEntryArray.append(NotifyJson);
        }
        NodeJson["StateEntryNotifies"] = StateEntryArray;

        JSON StateExitArray = JSON::Make(JSON::Class::Array);
        for (const FAnimNotifyEvent& Notify : Node.StateExitNotifies)
        {
            JSON NotifyJson = JSON::Make(JSON::Class::Object);
            NotifyJson["Name"] = Notify.NotifyName.ToString();
            NotifyJson["Time"] = Notify.TriggerTime;
            NotifyJson["Duration"] = Notify.Duration;
            NotifyJson["TrackIndex"] = Notify.TrackIndex;
            NotifyJson["PropertyData"] = Notify.PropertyData;
            StateExitArray.append(NotifyJson);
        }
        NodeJson["StateExitNotifies"] = StateExitArray;

        JSON StateBlendedArray = JSON::Make(JSON::Class::Array);
        for (const FAnimNotifyEvent& Notify : Node.StateFullyBlendedNotifies)
        {
            JSON NotifyJson = JSON::Make(JSON::Class::Object);
            NotifyJson["Name"] = Notify.NotifyName.ToString();
            NotifyJson["Time"] = Notify.TriggerTime;
            NotifyJson["Duration"] = Notify.Duration;
            NotifyJson["TrackIndex"] = Notify.TrackIndex;
            NotifyJson["PropertyData"] = Notify.PropertyData;
            StateBlendedArray.append(NotifyJson);
        }
        NodeJson["StateFullyBlendedNotifies"] = StateBlendedArray;

        NodesArray.append(NodeJson);

        // 3. 이 노드에서 나가는 Transitions 배열 생성
        for (const FAnimStateTransition& Trans : Node.Transitions)
        {
            JSON TransJson = JSON::Make(JSON::Class::Object);
            TransJson["From"] = Node.StateName.ToString();
            TransJson["To"] = Trans.TargetStateName.ToString();
            TransJson["BlendTime"] = Trans.BlendTime;

            // 4. Conditions 배열 생성
            JSON CondsArray = JSON::Make(JSON::Class::Array);
            for (const FAnimCondition& Cond : Trans.Conditions)
            {
                JSON CondJson = JSON::Make(JSON::Class::Object);
                CondJson["Type"] = static_cast<int32>(Cond.Type);
                CondJson["Param"] = Cond.ParameterName.ToString();
                CondJson["Op"] = static_cast<int32>(Cond.Op);
                CondJson["Val"] = Cond.Threshold;
                CondsArray.append(CondJson);
            }
            TransJson["Conditions"] = CondsArray;

            // 5. Transition Notify 배열 생성
            JSON TransNotifiesArray = JSON::Make(JSON::Class::Array);
            for (const FAnimNotifyEvent& Notify : Trans.TransitionNotifies)
            {
                JSON NotifyJson = JSON::Make(JSON::Class::Object);
                NotifyJson["Name"] = Notify.NotifyName.ToString();
                NotifyJson["Time"] = Notify.TriggerTime;  // Transition 진행률 (0.0 ~ 1.0)
                NotifyJson["Duration"] = Notify.Duration;
                NotifyJson["TrackIndex"] = Notify.TrackIndex;
                NotifyJson["PropertyData"] = Notify.PropertyData;
                TransNotifiesArray.append(NotifyJson);
            }
            TransJson["TransitionNotifies"] = TransNotifiesArray;

            TransitionsArray.append(TransJson);
        }
    }

    Root["Nodes"] = NodesArray;
    Root["Transitions"] = TransitionsArray;

    return FJsonSerializer::SaveJsonToFile(Root, Path);
}

void UAnimStateMachine::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    JSON Root;
    if (!FJsonSerializer::LoadJsonFromFile(Root, UTF8ToWide(InFilePath)))
    {
        UE_LOG("AnimStateMachine: Load: Failed %s", InFilePath.c_str());
        return;
    }

    Nodes.Empty();
    NodePositions.Empty();
    EntryStateName = FName();

    // Entry State 로드
    FString EntryStateStr;
    FJsonSerializer::ReadString(Root, "EntryState", EntryStateStr);
    EntryStateName = FName(EntryStateStr);

    // Nodes 배열 로드 (노드 먼저 전부 생성)
    JSON NodesArray;
    if (FJsonSerializer::ReadArray(Root, "Nodes", NodesArray))
    {
        for (int32 Idx = 0; Idx < NodesArray.size(); ++Idx)
        {
            JSON NodeJson = NodesArray.at(Idx);

            FString NameStr, AnimPathStr, BlendSpacePathStr;
            int32 AnimAssetType = 0;
            bool bLoop = true;
            FJsonSerializer::ReadString(NodeJson, "Name", NameStr);
            FJsonSerializer::ReadInt32(NodeJson, "AnimAssetType", AnimAssetType, 0);
            FJsonSerializer::ReadString(NodeJson, "AnimAsset", AnimPathStr);
            FJsonSerializer::ReadString(NodeJson, "BlendSpaceAsset", BlendSpacePathStr);
            FJsonSerializer::ReadBool(NodeJson, "Loop", bLoop, true);

            EAnimAssetType AssetType = static_cast<EAnimAssetType>(AnimAssetType);

            // 타입에 따라 노드 추가
            if (AssetType == EAnimAssetType::BlendSpace2D && !BlendSpacePathStr.empty())
            {
                UE_LOG("AnimStateMachine: Load: BlendSpace2D %s", BlendSpacePathStr.c_str());
                UBlendSpace2D* BlendSpace = UBlendSpace2D::LoadFromFile(BlendSpacePathStr);
                if (BlendSpace)
                {
                    UE_LOG("AnimStateMachine: Load: BlendSpace2D %d samples", BlendSpace->GetNumSamples());
                }
                AddNodeWithBlendSpace(FName(NameStr), BlendSpace, bLoop);
            }
            else if (AssetType == EAnimAssetType::AnimSequence && !AnimPathStr.empty())
            {
                UAnimSequence* Anim = UResourceManager::GetInstance().Load<UAnimSequence>(AnimPathStr);
                AddNode(FName(NameStr), Anim, bLoop);
            }
            else
            {
                // 타입 없거나 애셋 없으면 빈 노드
                AddNode(FName(NameStr), nullptr, bLoop);
            }

            // 에디터 위치 로드
            float PosX = 0, PosY = 0;
            if (FJsonSerializer::ReadFloat(NodeJson, "EditorPosX", PosX) &&
                FJsonSerializer::ReadFloat(NodeJson, "EditorPosY", PosY))
            {
                NodePositions.Add(FName(NameStr), FVector2D(PosX, PosY));
            }

            // State Notify 로드
            FAnimStateNode* LoadedNode = Nodes.Find(FName(NameStr));
            if (LoadedNode)
            {
                // State Entry Notifies
                JSON StateEntryArray;
                if (FJsonSerializer::ReadArray(NodeJson, "StateEntryNotifies", StateEntryArray))
                {
                    for (int32 NotifyIdx = 0; NotifyIdx < StateEntryArray.size(); ++NotifyIdx)
                    {
                        JSON NotifyJson = StateEntryArray.at(NotifyIdx);
                        FAnimNotifyEvent NotifyEvent;
                        FString NotifyName;
                        FJsonSerializer::ReadString(NotifyJson, "Name", NotifyName);
                        FJsonSerializer::ReadFloat(NotifyJson, "Time", NotifyEvent.TriggerTime);
                        FJsonSerializer::ReadFloat(NotifyJson, "Duration", NotifyEvent.Duration);
                        FJsonSerializer::ReadInt32(NotifyJson, "TrackIndex", NotifyEvent.TrackIndex);
                        FJsonSerializer::ReadString(NotifyJson, "PropertyData", NotifyEvent.PropertyData);
                        NotifyEvent.NotifyName = FName(NotifyName);
                        LoadedNode->StateEntryNotifies.Add(NotifyEvent);
                    }
                }

                // State Exit Notifies
                JSON StateExitArray;
                if (FJsonSerializer::ReadArray(NodeJson, "StateExitNotifies", StateExitArray))
                {
                    for (int32 NotifyIdx = 0; NotifyIdx < StateExitArray.size(); ++NotifyIdx)
                    {
                        JSON NotifyJson = StateExitArray.at(NotifyIdx);
                        FAnimNotifyEvent NotifyEvent;
                        FString NotifyName;
                        FJsonSerializer::ReadString(NotifyJson, "Name", NotifyName);
                        FJsonSerializer::ReadFloat(NotifyJson, "Time", NotifyEvent.TriggerTime);
                        FJsonSerializer::ReadFloat(NotifyJson, "Duration", NotifyEvent.Duration);
                        FJsonSerializer::ReadInt32(NotifyJson, "TrackIndex", NotifyEvent.TrackIndex);
                        FJsonSerializer::ReadString(NotifyJson, "PropertyData", NotifyEvent.PropertyData);
                        NotifyEvent.NotifyName = FName(NotifyName);
                        LoadedNode->StateExitNotifies.Add(NotifyEvent);
                    }
                }

                // State Fully Blended Notifies
                JSON StateBlendedArray;
                if (FJsonSerializer::ReadArray(NodeJson, "StateFullyBlendedNotifies", StateBlendedArray))
                {
                    for (int32 NotifyIdx = 0; NotifyIdx < StateBlendedArray.size(); ++NotifyIdx)
                    {
                        JSON NotifyJson = StateBlendedArray.at(NotifyIdx);
                        FAnimNotifyEvent NotifyEvent;
                        FString NotifyName;
                        FJsonSerializer::ReadString(NotifyJson, "Name", NotifyName);
                        FJsonSerializer::ReadFloat(NotifyJson, "Time", NotifyEvent.TriggerTime);
                        FJsonSerializer::ReadFloat(NotifyJson, "Duration", NotifyEvent.Duration);
                        FJsonSerializer::ReadInt32(NotifyJson, "TrackIndex", NotifyEvent.TrackIndex);
                        FJsonSerializer::ReadString(NotifyJson, "PropertyData", NotifyEvent.PropertyData);
                        NotifyEvent.NotifyName = FName(NotifyName);
                        LoadedNode->StateFullyBlendedNotifies.Add(NotifyEvent);
                    }
                }
            }
        }
    }

	// Transitions 배열 로드
    JSON TransitionsArray;
    if (FJsonSerializer::ReadArray(Root, "Transitions", TransitionsArray))
    {
        for (int32 Idx = 0; Idx < TransitionsArray.size(); ++Idx)
        {
            JSON TransJson = TransitionsArray.at(Idx);

            FString FromStr, ToStr;
            float BlendTime = 0.2f;
            FJsonSerializer::ReadString(TransJson, "From", FromStr);
            FJsonSerializer::ReadString(TransJson, "To", ToStr);
            FJsonSerializer::ReadFloat(TransJson, "BlendTime", BlendTime, 0.2f);

            FAnimStateTransition* NewTrans = AddTransition(FName(FromStr), FName(ToStr), BlendTime);

            // Conditions 배열 로드
        	JSON CondsArray;
        	if (FJsonSerializer::ReadArray(TransJson, "Conditions", CondsArray))
        	{
        		for (int32 j = 0; j < CondsArray.size(); ++j)
        		{
        			JSON CondJson = CondsArray.at(j);

        			FString ParamStr;
        			int32 OpInt = 0;
        			float Val = 0.0f;
        			int32 TypeInt = 0;

        			// [추가] "Type" 필드 읽기 (없으면 0으로 간주하여 호환성 유지)
        			FJsonSerializer::ReadInt32(CondJson, "Type", TypeInt, 0);
        			FJsonSerializer::ReadString(CondJson, "Param", ParamStr);
        			FJsonSerializer::ReadInt32(CondJson, "Op", OpInt);
        			FJsonSerializer::ReadFloat(CondJson, "Val", Val);

        			// AddCondition 호출 시 타입도 함께 전달
        			NewTrans->AddCondition(static_cast<EAnimConditionType>(TypeInt),
						FName(ParamStr), static_cast<EAnimConditionOp>(OpInt), Val);
        		}
        	}
            // Transition Notify 배열 로드
            if (NewTrans)
            {
                JSON TransNotifiesArray;
                if (FJsonSerializer::ReadArray(TransJson, "TransitionNotifies", TransNotifiesArray))
                {
                    for (int32 NotifyIdx = 0; NotifyIdx < TransNotifiesArray.size(); ++NotifyIdx)
                    {
                        JSON NotifyJson = TransNotifiesArray.at(NotifyIdx);
                        FAnimNotifyEvent NotifyEvent;
                        FString NotifyName;
                        FJsonSerializer::ReadString(NotifyJson, "Name", NotifyName);
                        FJsonSerializer::ReadFloat(NotifyJson, "Time", NotifyEvent.TriggerTime);
                        FJsonSerializer::ReadFloat(NotifyJson, "Duration", NotifyEvent.Duration);
                        FJsonSerializer::ReadInt32(NotifyJson, "TrackIndex", NotifyEvent.TrackIndex);
                        FJsonSerializer::ReadString(NotifyJson, "PropertyData", NotifyEvent.PropertyData);
                        NotifyEvent.NotifyName = FName(NotifyName);
                        NewTrans->TransitionNotifies.Add(NotifyEvent);
                    }
                }
            }
        }
    }

    UE_LOG("AnimStateMachine: Load: %s", InFilePath.c_str());
}
