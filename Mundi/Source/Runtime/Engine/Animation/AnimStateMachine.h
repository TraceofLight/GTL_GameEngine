#pragma once
#include "Object.h"
#include "PoseContext.h"
#include"UAnimStateMachine.generated.h"
class APawn;
class ACharacter;
class UCharacterMovementComponent;
class UAnimSequence;

/**
 * @brief 비교 연산자
 */
enum class EAnimConditionOp : uint8
{
	Equals,         // ==
	NotEquals,      // !=
	Greater,        // >
	Less,           // <
	GreaterOrEqual, // >=
	LessOrEqual     // <=
};

/**
 * @brief 단일 트랜지션 조건
 * 예: "Speed" (ParamName)가 100.0 (Threshold)보다 "Greater" (Op) 해야 한다.
 */
struct FAnimCondition
{
	FName ParameterName;      // 검사할 변수 이름 (예: "Speed", "IsAir")
	EAnimConditionOp Op;      // 비교 방법
	float Threshold;          // 기준 값

	FAnimCondition()
		: ParameterName(), Op(EAnimConditionOp::Greater), Threshold(0.0f) {}

	FAnimCondition(FName InName, EAnimConditionOp InOp, float InVal)
		: ParameterName(InName), Op(InOp), Threshold(InVal) {}
};

/**
 * @brief 상태 전환 규칙 (수정됨)
 * 이제 조건 목록(Conditions)을 가집니다.
 */
struct FAnimStateTransition
{
	FName TargetStateName;
	float BlendTime;
	TArray<FAnimCondition> Conditions;

	FAnimStateTransition(FName InTarget, float InBlendTime)
		: TargetStateName(InTarget), BlendTime(InBlendTime) {}

	void AddCondition(FName Param, EAnimConditionOp Op, float Val)
	{
		Conditions.Add(FAnimCondition(Param, Op, Val));
	}
};

/**
 * @brief 애니메이션 타입
 */
enum class EAnimAssetType : uint8
{
	None,
	AnimSequence,
	BlendSpace2D
};

/**
 * @brief 상태 노드 (Data)
 * * 하나의 상태를 정의하는 단위. 이름, 애니메이션, 나가는 연결선(Transition)을 가짐.
 */
struct FAnimStateNode
{
	/** 상태 이름 (고유 ID 역할, 예: "Idle", "Run") */
	FName StateName;

	/** 애니메이션 타입 */
	EAnimAssetType AnimAssetType;

	/** 이 상태에서 재생할 애니메이션 (AnimSequence 사용 시) */
	UAnimSequence* AnimationAsset;

	/** 이 상태에서 사용할 BlendSpace2D (BlendSpace2D 사용 시) */
	class UBlendSpace2D* BlendSpaceAsset;

	/** 이 상태에서 나갈 수 있는 트랜지션 목록 */
	TArray<FAnimStateTransition> Transitions;

	/** 루프 여부 */
	bool bLoop;

	FAnimStateNode()
		: StateName()
		, AnimAssetType(EAnimAssetType::None)
		, AnimationAsset(nullptr)
		, BlendSpaceAsset(nullptr)
		, bLoop(true) {}

	FAnimStateNode(const FName InStateName)
		: StateName(InStateName)
		, AnimAssetType(EAnimAssetType::None)
		, AnimationAsset(nullptr)
		, BlendSpaceAsset(nullptr)
		, bLoop(true) {}
};

/**
 * @brief 애니메이션 State Machine (Asset)
 * * 노드들의 집합을 관리하는 데이터 컨테이너.
 * 에디터에서 저장/로드가 가능한 형태.
 */
class UAnimStateMachine : public UResourceBase
{
	GENERATED_REFLECTION_BODY()

public:
	UAnimStateMachine();
	virtual ~UAnimStateMachine() override = default;

// ===== 에디터/구축 API =====
public:
	/**
	 * @brief 새로운 상태 노드 추가 (AnimSequence)
	 * @return 추가가 제대로 되었는지 결과 반환 (같은 이름 존재 시 추가 X)
	 */
	FAnimStateNode* AddNode(FName NodeName, UAnimSequence* InAnim, bool bInLoop = true);

	/**
	 * @brief 새로운 상태 노드 추가 (BlendSpace2D)
	 * @return 추가가 제대로 되었는지 결과 반환 (같은 이름 존재 시 추가 X)
	 */
	FAnimStateNode* AddNodeWithBlendSpace(FName NodeName, class UBlendSpace2D* InBlendSpace, bool bInLoop = true);

	/** * @brief 노드 삭제 (중요: 연결된 트랜지션도 정리함)
	 * @return 삭제 성공 여부
	 */
	bool RemoveNode(FName NodeName);

	/**
	 * @brief 노드 이름 변경 (중요: Map Key 교체 및 트랜지션 타겟 갱신)
	 * @return 이름 변경 성공 여부 (중복 이름 있으면 실패)
	 */
	bool RenameNode(FName OldName, FName NewName);

	/**
	 * @brief 트랜지션 연결
	 */
	FAnimStateTransition* AddTransition(FName FromState, FName ToState, float BlendTime);

	/**
	 * @brief 트랜지션 삭제
	 * @return 삭제 성공 여부
	 */
	bool RemoveTransition(FName FromState, FName ToState);

	/**
	 * @brief 특정 트랜지션 포인터 찾기 (조건 편집용)
	 */
	FAnimStateTransition* FindTransition(FName FromState, FName ToState);

	/**
	 * @brief 특정 트랜지션에 조건 추가
	 * @param FromState 출발 노드 이름
	 * @param ToState 도착 노드 이름
	 * @param Condition 추가할 조건 데이터
	 */
	bool AddConditionToTransition(FName FromState, FName ToState, const FAnimCondition& Condition);

	/**
	 * @brief 트랜지션에서 특정 인덱스의 조건 삭제
	 */
	bool RemoveConditionFromTransition(FName FromState, FName ToState, int32 ConditionIndex);

	/**
	 * @brief 트랜지션에서 특정 인덱스의 조건 수정
	 */
	bool UpdateConditionInTransition(FName FromState, FName ToState, int32 ConditionIndex, const FAnimCondition& NewCondition);

	TMap<FName, FAnimStateNode>& GetNodes() { return Nodes; }

	/**
	 * @brief 시작 상태 설정
	 */
	void SetEntryState(FName NodeName) { EntryStateName = NodeName; }

// ===== 런타임 조회 API =====
public:
	/**
	 * @brief 이름으로 노드 찾기
	 */
	const FAnimStateNode* FindNode(FName NodeName) const { return Nodes.Find(NodeName); }

	/**
	 * @brief 시작 상태 이름 반환
	 */
	FName GetEntryStateName() const { return EntryStateName; }

protected:
	/** 모든 상태 노드 저장소 {이름, 노드} */
	TMap<FName, FAnimStateNode> Nodes;

	/** 시작 상태 이름 (Entry Point) */
	FName EntryStateName;

// ===== Serialize API =====
public:
	/**
	 * @brief 현재 State Machine 애셋을 JSON 파일로 저장합니다.
	 * @param Path FString 경로 (예: "Content/MyPlayer.StateMachine")
	 * @return 저장 성공 여부
	 */
	bool SaveToFile(const FWideString& Path);

	/**
	 * @brief JSON 파일에서 State Machine 애셋 데이터를 불러옵니다.
	 * @param InFilePath FString 경로
	 * @return 로드 성공 여부
	 */
	void Load(const FString& InFilePath, ID3D11Device* InDevice);

public:
	// ImGui 노드 에디터의 위치 저장
	TMap<FName, FVector2D> NodePositions;
};
