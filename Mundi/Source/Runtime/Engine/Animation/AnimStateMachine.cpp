#include "pch.h"
#include "AnimStateMachine.h"
#include "AnimSequence.h"

IMPLEMENT_CLASS(UAnimStateMachine)

UAnimStateMachine::UAnimStateMachine() = default;

FAnimStateNode* UAnimStateMachine::AddNode(FName NodeName, UAnimSequence* InAnim, bool bInLoop)
{
	FAnimStateNode NewNode(NodeName);
	NewNode.AnimationAsset = InAnim;
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
        NodeJson["AnimAsset"] = Node.AnimationAsset ? Node.AnimationAsset->GetFilePath() : "";
        NodeJson["Loop"] = Node.bLoop;

        // 에디터 위치 저장
        if (NodePositions.Contains(Node.StateName))
        {
            const FVector2D& Pos = NodePositions[Node.StateName];
            NodeJson["EditorPosX"] = Pos.X;
            NodeJson["EditorPosY"] = Pos.Y;
        }
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
                JSON condJson = JSON::Make(JSON::Class::Object);
                condJson["Param"] = Cond.ParameterName.ToString();
                condJson["Op"] = static_cast<int32>(Cond.Op);
                condJson["Val"] = Cond.Threshold;
                CondsArray.append(condJson);
            }
            TransJson["Conditions"] = CondsArray;
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
        UE_LOG("[UAnimStateMachine] Failed to load file: %s", InFilePath.c_str());
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

            FString NameStr, AnimPathStr;
            bool bLoop = true;
            FJsonSerializer::ReadString(NodeJson, "Name", NameStr);
            FJsonSerializer::ReadString(NodeJson, "AnimAsset", AnimPathStr);
            FJsonSerializer::ReadBool(NodeJson, "Loop", bLoop, true);

            UAnimSequence* Anim = nullptr;
            if (!AnimPathStr.empty())
            {
                Anim = UResourceManager::GetInstance().Load<UAnimSequence>(AnimPathStr);
            }

            AddNode(FName(NameStr), Anim, bLoop);

            // 에디터 위치 로드
            float PosX = 0, PosY = 0;
            if (FJsonSerializer::ReadFloat(NodeJson, "EditorPosX", PosX) &&
                FJsonSerializer::ReadFloat(NodeJson, "EditorPosY", PosY))
            {
                NodePositions.Add(FName(NameStr), FVector2D(PosX, PosY));
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

                    FJsonSerializer::ReadString(CondJson, "Param", ParamStr);
                    FJsonSerializer::ReadInt32(CondJson, "Op", OpInt);
                    FJsonSerializer::ReadFloat(CondJson, "Val", Val);

                    NewTrans->AddCondition(FName(ParamStr), static_cast<EAnimConditionOp>(OpInt), Val);
                }
            }
        }
    }

    UE_LOG("[UAnimStateMachine] Loaded successfully: %s", InFilePath.c_str());
}
