# 애니메이션 시스템 아키텍처 (언리얼 방식)

## 개요

Mundi 엔진의 애니메이션 시스템은 **언리얼 엔진의 구조**를 따라 설계되었습니다.
핵심은 **애셋(Asset)과 인스턴스(Instance)의 분리**입니다.

---

## 핵심 구조

```
┌─────────────────────────────────────────────────────────────┐
│                        Character                            │
│  - SkeletalMeshComponent                                    │
│  - AnimStateMachine (애셋 참조)                              │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│               SkeletalMeshComponent                         │
│  - AnimInstance (실행 인스턴스)                              │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                   UAnimInstance                             │
│  - StateMachineNode (FAnimNode_StateMachine)                │
│  - UpdateAnimation()                                        │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│            FAnimNode_StateMachine                           │
│  [인스턴스 데이터]                                            │
│  - CurrentState                                             │
│  - TransitionTime                                           │
│  - CurrentAnimTime                                          │
│  - StateMachine → [애셋 참조]                                │
│                                                             │
│  [메서드]                                                    │
│  - Initialize(Pawn)                                         │
│  - Update(DeltaTime)                                        │
│  - Evaluate(OutPose)                                        │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│            UAnimStateMachine (애셋)                         │
│  [공유 데이터]                                                │
│  - StateAnimations[]                                        │
│  - Transitions[]                                            │
│                                                             │
│  [메서드]                                                    │
│  - RegisterStateAnimation()                                 │
│  - AddTransition()                                          │
│  - GetStateAnimation()                                      │
│  - FindTransitionBlendTime()                                │
└─────────────────────────────────────────────────────────────┘
```

---

## 클래스별 역할

### 1. UAnimStateMachine (애셋 클래스)

**위치**: `Source/Runtime/Engine/Animation/AnimStateMachine.h/cpp`

**역할**:
- 상태 정의와 전환 규칙을 저장하는 **공유 가능한 애셋**
- 여러 캐릭터가 같은 StateMachine을 공유할 수 있음

**데이터**:
```cpp
TArray<UAnimSequence*> StateAnimations;  // 각 상태의 애니메이션
TArray<FAnimStateTransition> Transitions; // 전환 규칙
```

**주요 메서드**:
```cpp
void RegisterStateAnimation(EAnimState State, UAnimSequence* Animation);
void AddTransition(const FAnimStateTransition& Transition);
UAnimSequence* GetStateAnimation(EAnimState State) const;
float FindTransitionBlendTime(EAnimState FromState, EAnimState ToState) const;
```

**사용 예시**:
```cpp
// 애셋 생성 (1번만, 여러 캐릭터가 공유)
UAnimStateMachine* SM = NewObject<UAnimStateMachine>();
SM->RegisterStateAnimation(EAnimState::Idle, IdleAnim);
SM->RegisterStateAnimation(EAnimState::Walk, WalkAnim);
SM->AddTransition(FAnimStateTransition(Idle, Walk, 0.2f));
```

---

### 2. FAnimNode_StateMachine (노드 클래스)

**위치**: `Source/Runtime/Engine/Animation/AnimNode_StateMachine.h/cpp`

**역할**:
- UAnimStateMachine 애셋을 **실행**하는 인스턴스
- 캐릭터마다 별도의 실행 상태를 가짐

**인스턴스 데이터**:
```cpp
EAnimState CurrentState;        // 현재 상태
EAnimState PreviousState;       // 이전 상태 (블렌딩용)
bool bIsTransitioning;          // 전환 중인지
float TransitionTime;           // 전환 경과 시간
float TransitionDuration;       // 전환 총 시간
float CurrentAnimTime;          // 현재 애니메이션 재생 시간
float PreviousAnimTime;         // 이전 애니메이션 재생 시간
```

**주요 메서드**:
```cpp
void Initialize(APawn* InPawn);                    // 초기화
void SetStateMachine(UAnimStateMachine* InSM);     // 애셋 설정
void Update(float DeltaSeconds);                   // 상태 업데이트
void Evaluate(FPoseContext& OutPose);              // 포즈 계산 (블렌딩)
```

**동작 원리**:
1. `Update()`: Movement 상태를 체크하여 자동으로 상태 전환
2. `Evaluate()`: 현재/이전 상태의 애니메이션을 블렌딩하여 최종 포즈 출력

---

### 3. UAnimInstance (실행 관리자)

**위치**: `Source/Runtime/Engine/Animation/AnimInstance.h/cpp`

**역할**:
- 캐릭터의 애니메이션 실행을 총괄
- StateMachine 노드를 멤버로 가지고 있음

**멤버**:
```cpp
FAnimNode_StateMachine StateMachineNode;  // State Machine 실행 노드
```

**주요 메서드**:
```cpp
void SetStateMachine(UAnimStateMachine* InStateMachine);  // 애셋 설정
FAnimNode_StateMachine* GetStateMachineNode();            // 노드 접근
```

---

### 4. USkeletalMeshComponent (렌더링 컴포넌트)

**역할**:
- AnimInstance를 통해 StateMachine 실행
- 계산된 포즈를 렌더링

**TickComponent 로직**:
```cpp
void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    if (AnimInstance)
    {
        FAnimNode_StateMachine* Node = AnimInstance->GetStateMachineNode();
        if (Node && Node->GetStateMachine())
        {
            // 1. 상태 업데이트
            Node->Update(DeltaTime);

            // 2. 포즈 계산
            FPoseContext Pose;
            Node->Evaluate(Pose);

            // 3. 본 트랜스폼 적용
            ApplyPose(Pose);
        }
    }
}
```

---

### 5. ACharacter (게임 로직)

**역할**:
- StateMachine 애셋 생성 및 설정

**BeginPlay 로직**:
```cpp
void ACharacter::BeginPlay()
{
    // 1. StateMachine 애셋 생성
    AnimStateMachine = NewObject<UAnimStateMachine>();

    // 2. 애니메이션 등록
    AnimStateMachine->RegisterStateAnimation(EAnimState::Idle, IdleAnim);
    AnimStateMachine->RegisterStateAnimation(EAnimState::Walk, WalkAnim);

    // 3. 전환 규칙 추가
    AnimStateMachine->AddTransition(FAnimStateTransition(Idle, Walk, 0.2f));

    // 4. SkeletalMeshComponent에 설정
    //    (내부적으로 AnimInstance->SetStateMachine() 호출)
    SkeletalMeshComponent->SetAnimationStateMachine(AnimStateMachine);
}
```

---

## 애셋 vs 인스턴스 분리의 장점

### 이전 방식 (분리 전)
```cpp
UAnimStateMachine
├─ StateAnimations[]      // 애셋 데이터
├─ Transitions[]          // 애셋 데이터
├─ CurrentState           // 인스턴스 데이터 ❌
├─ TransitionTime         // 인스턴스 데이터 ❌
└─ OwnerPawn              // 인스턴스 데이터 ❌

문제점:
- 캐릭터마다 별도의 StateMachine 객체 필요
- 메모리 낭비 (같은 애니메이션 데이터 중복)
- 공유 불가능
```

### 현재 방식 (분리 후)
```cpp
UAnimStateMachine (애셋, 공유)
├─ StateAnimations[]      // 애셋 데이터 ✅
└─ Transitions[]          // 애셋 데이터 ✅

FAnimNode_StateMachine (인스턴스, 개별)
├─ StateMachine*          // 애셋 참조
├─ CurrentState           // 인스턴스 데이터 ✅
├─ TransitionTime         // 인스턴스 데이터 ✅
└─ OwnerPawn              // 인스턴스 데이터 ✅

장점:
- 100개의 적이 같은 StateMachine 애셋 공유
- 메모리 효율적
- 에디터에서 수정 시 모든 캐릭터에 즉시 반영
```

---

## 확장 가능성

### 1. 노드 기반 애니메이션 그래프

현재 `FAnimNode_StateMachine` 하나만 있지만, 다양한 노드를 추가할 수 있습니다:

```cpp
// 베이스 클래스
class FAnimNode_Base
{
    virtual void Update(float DeltaTime) = 0;
    virtual void Evaluate(FPoseContext& OutPose) = 0;
};

// 다양한 노드들
class FAnimNode_SequencePlayer : public FAnimNode_Base { };
class FAnimNode_BlendTwoPoses : public FAnimNode_Base { };
class FAnimNode_StateMachine : public FAnimNode_Base { };  // 이미 있음
class FAnimNode_BlendSpace2D : public FAnimNode_Base { };
class FAnimNode_IK : public FAnimNode_Base { };
class FAnimNode_LayerBlend : public FAnimNode_Base { };
```

**사용 예시**:
```cpp
// AnimInstance에 여러 노드 조합
class UMyAnimInstance : public UAnimInstance
{
    FAnimNode_StateMachine LocomotionSM;  // 이동 State Machine
    FAnimNode_BlendSpace2D AimOffset;     // 조준 Blend Space
    FAnimNode_LayerBlend UpperBodyBlend;  // 상체 레이어 블렌딩
    FAnimNode_IK FootIK;                  // 발 IK
};
```

---

### 2. Blend Space 2D

**목적**: 2차원 파라미터로 애니메이션 블렌딩 (예: 속도 + 방향)

```cpp
class UBlendSpace2D : public UObject
{
    // 샘플 포인트들
    struct FSamplePoint
    {
        FVector2D Position;      // (X=속도, Y=방향)
        UAnimSequence* Animation;
    };

    TArray<FSamplePoint> Samples;

    // 주어진 파라미터에서 블렌드 가중치 계산
    void GetBlendWeights(FVector2D Param, TArray<float>& OutWeights);
};

class FAnimNode_BlendSpace2D : public FAnimNode_Base
{
    UBlendSpace2D* BlendSpace;
    FVector2D BlendParameter;  // 실시간 파라미터

    void Evaluate(FPoseContext& OutPose) override
    {
        // 파라미터 기반으로 여러 애니메이션 블렌딩
        TArray<float> Weights;
        BlendSpace->GetBlendWeights(BlendParameter, Weights);
        // ... 블렌딩 로직
    }
};
```

---

### 3. 애니메이션 레이어

**목적**: 상체/하체를 별도로 제어

```cpp
class FAnimNode_LayerBlend : public FAnimNode_Base
{
    FAnimNode_Base* BaseLayer;     // 전신 애니메이션
    FAnimNode_Base* OverlayLayer;  // 상체 애니메이션

    TArray<int32> OverlayBones;    // 상체 본 인덱스들

    void Evaluate(FPoseContext& OutPose) override
    {
        // 1. 전신 애니메이션 계산
        BaseLayer->Evaluate(OutPose);

        // 2. 상체 애니메이션 계산
        FPoseContext OverlayPose;
        OverlayLayer->Evaluate(OverlayPose);

        // 3. 상체 본만 오버레이
        for (int32 BoneIdx : OverlayBones)
        {
            OutPose.LocalSpacePose[BoneIdx] = OverlayPose.LocalSpacePose[BoneIdx];
        }
    }
};
```

---

### 4. IK (Inverse Kinematics)

```cpp
class FAnimNode_TwoJointIK : public FAnimNode_Base
{
    FAnimNode_Base* InputNode;
    FVector TargetLocation;  // 손/발이 닿을 위치

    void Evaluate(FPoseContext& OutPose) override
    {
        // 1. 입력 포즈 계산
        InputNode->Evaluate(OutPose);

        // 2. IK 계산 (TargetLocation에 도달하도록 본 회전 조정)
        SolveTwoJointIK(OutPose, TargetLocation);
    }
};
```

---

### 5. Montage (일회성 애니메이션)

**목적**: 공격, 스킬 등 일회성 애니메이션

```cpp
class UAnimMontage : public UObject
{
    UAnimSequence* Animation;
    TArray<FMontageSection> Sections;  // 섹션 분할
};

class FAnimNode_Montage : public FAnimNode_Base
{
    FAnimNode_Base* DefaultNode;  // 기본 애니메이션
    UAnimMontage* CurrentMontage;

    void Evaluate(FPoseContext& OutPose) override
    {
        if (CurrentMontage && CurrentMontage->IsPlaying())
        {
            // 몽타주 재생
            EvaluateMontage(OutPose);
        }
        else
        {
            // 기본 애니메이션
            DefaultNode->Evaluate(OutPose);
        }
    }
};
```

---

## 에디터 구현 방법

### 1. State Machine 에디터

**UI 구성**:
```
┌──────────────────────────────────────────────────────────┐
│  State Machine Editor                          [X]       │
├──────────────────────────────────────────────────────────┤
│  ┌────────────────┐                                      │
│  │  [Idle]        │─────0.2s─────►┌────────────────┐    │
│  │                │                │  [Walk]        │    │
│  └────────────────┘                │                │    │
│         ▲                           └────────────────┘    │
│         │                                    │            │
│         └─────────0.2s─────────────────────┘            │
│                                                          │
│  Selected: Walk → Run Transition                        │
│  Blend Time: [0.3] seconds                              │
│  Condition: Speed > 200.0                               │
└──────────────────────────────────────────────────────────┘
```

**ImGui 코드**:
```cpp
class SAnimStateMachineEditor : public SWindow
{
    UAnimStateMachine* EditingStateMachine;

    void OnRender() override
    {
        ImGui::Begin("State Machine Editor");

        // 캔버스
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // 상태 노드 그리기
        for (int i = 0; i < (int)EAnimState::MAX; ++i)
        {
            EAnimState State = (EAnimState)i;
            FVector2D NodePos = GetStateNodePosition(State);

            // 노드 박스
            ImVec2 node_min(canvas_pos.x + NodePos.x, canvas_pos.y + NodePos.y);
            ImVec2 node_max(node_min.x + 120, node_min.y + 60);

            ImU32 color = (State == SelectedState) ? IM_COL32(255,200,0,255) : IM_COL32(100,100,200,255);
            draw_list->AddRectFilled(node_min, node_max, color, 5.0f);

            // 상태 이름
            const char* name = UAnimStateMachine::GetStateName(State);
            ImVec2 text_pos(node_min.x + 10, node_min.y + 20);
            draw_list->AddText(text_pos, IM_COL32(255,255,255,255), name);

            // 클릭 감지
            if (ImGui::IsMouseHoveringRect(node_min, node_max) && ImGui::IsMouseClicked(0))
            {
                SelectedState = State;
            }
        }

        // 전환 화살표 그리기
        for (const FAnimStateTransition& Trans : EditingStateMachine->GetTransitions())
        {
            FVector2D FromPos = GetStateNodePosition(Trans.FromState);
            FVector2D ToPos = GetStateNodePosition(Trans.ToState);

            ImVec2 p1(canvas_pos.x + FromPos.x + 120, canvas_pos.y + FromPos.y + 30);
            ImVec2 p2(canvas_pos.x + ToPos.x, canvas_pos.y + ToPos.y + 30);

            draw_list->AddLine(p1, p2, IM_COL32(255,255,255,255), 2.0f);

            // 화살표
            DrawArrowHead(draw_list, p1, p2);

            // 블렌드 시간 표시
            char label[32];
            sprintf(label, "%.1fs", Trans.BlendTime);
            ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            draw_list->AddText(mid, IM_COL32(255,200,0,255), label);
        }

        // 디테일 패널
        ImGui::Separator();
        ImGui::Text("Selected State: %s", UAnimStateMachine::GetStateName(SelectedState));

        // 애니메이션 선택
        if (ImGui::Button("Set Animation..."))
        {
            OpenAnimationPicker(SelectedState);
        }

        ImGui::End();
    }

    void DrawArrowHead(ImDrawList* dl, ImVec2 from, ImVec2 to)
    {
        // 화살표 머리 그리기
        FVector2D dir = FVector2D(to.x - from.x, to.y - from.y).GetNormalized();
        FVector2D perp(-dir.y, dir.x);

        ImVec2 arrow_tip = to;
        ImVec2 arrow_left(to.x - dir.x * 10 + perp.x * 5, to.y - dir.y * 10 + perp.y * 5);
        ImVec2 arrow_right(to.x - dir.x * 10 - perp.x * 5, to.y - dir.y * 10 - perp.y * 5);

        dl->AddTriangleFilled(arrow_tip, arrow_left, arrow_right, IM_COL32(255,255,255,255));
    }
};
```

---

### 2. Blend Space 2D 에디터

**UI 구성**:
```
┌──────────────────────────────────────────────────────────┐
│  Blend Space 2D Editor                         [X]       │
├──────────────────────────────────────────────────────────┤
│  X Axis: Speed (0 ~ 400)                                │
│  Y Axis: Direction (-180 ~ 180)                         │
│                                                          │
│  ┌────────────────────────────────────────────────┐     │
│  │          ▲ Y (Direction)                       │     │
│  │  180°    │                                     │     │
│  │          │                                     │     │
│  │      ●───┼───●  (Sample Points)               │     │
│  │          │                                     │     │
│  │  ────────┼────────► X (Speed)                 │     │
│  │          │                                     │     │
│  │      ●───┼───●                                 │     │
│  │          │                                     │     │
│  │  -180°   │                                     │     │
│  └────────────────────────────────────────────────┘     │
│                                                          │
│  Sample Points:                                         │
│  ● (0, 0) = Idle                                        │
│  ● (200, 0) = Walk Forward                              │
│  ● (200, 180) = Walk Backward                           │
│  ● (400, 45) = Run Forward Right                        │
│                                                          │
│  [Add Sample]  [Remove]  [Preview]                      │
└──────────────────────────────────────────────────────────┘
```

**구현**:
```cpp
class SBlendSpace2DEditor : public SWindow
{
    UBlendSpace2D* EditingBlendSpace;
    FVector2D PreviewParameter;  // 프리뷰용

    void OnRender() override
    {
        ImGui::Begin("Blend Space 2D Editor");

        // 파라미터 범위 설정
        ImGui::InputFloat("X Min", &EditingBlendSpace->XMin);
        ImGui::InputFloat("X Max", &EditingBlendSpace->XMax);
        ImGui::InputFloat("Y Min", &EditingBlendSpace->YMin);
        ImGui::InputFloat("Y Max", &EditingBlendSpace->YMax);

        // 2D 그리드 캔버스
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size(400, 400);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // 배경 그리드
        DrawGrid(draw_list, canvas_pos, canvas_size);

        // 샘플 포인트 그리기
        for (auto& Sample : EditingBlendSpace->Samples)
        {
            ImVec2 pos = ParamToScreen(Sample.Position, canvas_pos, canvas_size);
            draw_list->AddCircleFilled(pos, 8.0f, IM_COL32(255,200,0,255));

            // 클릭 감지
            if (ImGui::IsMouseHoveringRect(
                ImVec2(pos.x - 10, pos.y - 10),
                ImVec2(pos.x + 10, pos.y + 10)) && ImGui::IsMouseClicked(0))
            {
                SelectedSample = &Sample;
            }
        }

        // 프리뷰 마커
        ImVec2 preview_pos = ParamToScreen(PreviewParameter, canvas_pos, canvas_size);
        draw_list->AddCircle(preview_pos, 12.0f, IM_COL32(0,255,0,255), 0, 2.0f);

        // 프리뷰 컨트롤
        ImGui::SliderFloat2("Preview Param", &PreviewParameter.X, 0, 400);

        if (ImGui::Button("Add Sample Point"))
        {
            OpenAnimationPicker();
        }

        ImGui::End();
    }

    ImVec2 ParamToScreen(FVector2D Param, ImVec2 canvas_pos, ImVec2 canvas_size)
    {
        float x = (Param.X - EditingBlendSpace->XMin) / (EditingBlendSpace->XMax - EditingBlendSpace->XMin);
        float y = (Param.Y - EditingBlendSpace->YMin) / (EditingBlendSpace->YMax - EditingBlendSpace->YMin);

        return ImVec2(
            canvas_pos.x + x * canvas_size.x,
            canvas_pos.y + (1.0f - y) * canvas_size.y
        );
    }
};
```

---

### 3. Animation Graph 에디터 (노드 기반)

**ImNodes 라이브러리 사용**:
```cpp
// ThirdParty에 imnodes 추가 필요
// https://github.com/Nelarius/imnodes

class SAnimGraphEditor : public SWindow
{
    void OnRender() override
    {
        ImGui::Begin("Anim Graph");

        ImNodes::BeginNodeEditor();

        // State Machine 노드
        ImNodes::BeginNode(1);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("State Machine");
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(10);
        ImGui::Text("Pose");
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();

        // Blend Space 노드
        ImNodes::BeginNode(2);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("Blend Space 2D");
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(20);
        ImGui::Text("Pose");
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();

        // Layer Blend 노드
        ImNodes::BeginNode(3);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("Layer Blend");
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(30);
        ImGui::Text("Base");
        ImNodes::EndInputAttribute();

        ImNodes::BeginInputAttribute(31);
        ImGui::Text("Overlay");
        ImNodes::EndInputAttribute();

        ImNodes::BeginOutputAttribute(32);
        ImGui::Text("Result");
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();

        // 연결선
        ImNodes::Link(0, 10, 30);  // StateMachine → LayerBlend Base
        ImNodes::Link(1, 20, 31);  // BlendSpace → LayerBlend Overlay

        ImNodes::EndNodeEditor();
        ImGui::End();
    }
};
```

---

## Lua 연결 (나중에)

### Lua 바인딩 예시

```lua
-- AnimState enum
local AnimState = {
    Idle = 0,
    Walk = 1,
    Run = 2,
    Jump = 3,
    Fall = 4,
    Fly = 5,
    Crouch = 6
}

-- StateMachine 애셋 생성
local stateMachine = CreateAnimStateMachine()

-- 애니메이션 등록
stateMachine:RegisterStateAnimation(AnimState.Idle, idleAnim)
stateMachine:RegisterStateAnimation(AnimState.Walk, walkAnim)
stateMachine:RegisterStateAnimation(AnimState.Run, runAnim)

-- 전환 규칙
stateMachine:AddTransition(AnimState.Idle, AnimState.Walk, 0.2)
stateMachine:AddTransition(AnimState.Walk, AnimState.Run, 0.3)

-- 캐릭터에 설정
character:GetMesh():SetAnimationStateMachine(stateMachine)
```

---

## 요약

### 현재 구현된 것
✅ UAnimStateMachine (애셋)
✅ FAnimNode_StateMachine (인스턴스)
✅ UAnimInstance 통합
✅ 애셋/인스턴스 분리
✅ 자동 상태 전환
✅ 부드러운 블렌딩

### 향후 확장 가능
🔲 Blend Space 2D
🔲 애니메이션 레이어
🔲 IK 시스템
🔲 Montage (일회성 애니메이션)
🔲 노드 기반 AnimGraph
🔲 State Machine 비주얼 에디터
🔲 Blend Space 2D 에디터
🔲 Lua 바인딩

---

**작성자**: Claude Code
**날짜**: 2025-11-16
