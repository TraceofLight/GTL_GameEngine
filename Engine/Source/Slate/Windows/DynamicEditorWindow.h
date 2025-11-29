#pragma once
#include "SWindow.h"
#include "SSplitterH.h"
#include "SSplitterV.h"

class FViewport;
class FViewportClient;
class UWorld;
class USkeletalMesh;
class UAnimSequence;
class UBlendSpace2D;
class UAnimStateMachine;
struct ID3D11Device;
class AGizmoActor;

// Forward declarations for panel classes
class SEditorPanel;
class SDynamicEditorWindow;

/**
 * @brief 에디터 모드 열거형
 * @details 동적 에디터 윈도우에서 지원하는 편집 모드
 */
enum class EEditorMode : uint8
{
	Skeletal,      // 스켈레탈 메시 편집 (본 구조, 메시)
	Animation,     // 애니메이션 편집 (타임라인, 키프레임)
	AnimGraph,     // 애니메이션 그래프 (스테이트 머신)
	BlendSpace2D   // 블렌드 스페이스 2D 편집
};

/**
 * @brief 에디터 탭 상태
 * @details 각 탭의 고유 상태를 관리 (모드, 에셋, 뷰포트 등)
 */
struct FEditorTabState
{
	// 기본 정보
	FName Name;
	int32 TabId = 0;
	EEditorMode Mode = EEditorMode::Skeletal;

	// 월드 및 뷰포트
	UWorld* World = nullptr;
	FViewport* Viewport = nullptr;
	FViewportClient* Client = nullptr;

	// 공통 에셋 참조
	class ASkeletalMeshActor* PreviewActor = nullptr;
	USkeletalMesh* CurrentMesh = nullptr;
	FString LoadedMeshPath;

	// Skeletal 모드 상태
	int32 SelectedBoneIndex = -1;
	bool bShowMesh = true;
	bool bShowBones = true;
	bool bBoneLinesDirty = true;
	int32 LastSelectedBoneIndex = -1;
	char MeshPathBuffer[260] = {0};
	std::set<int32> ExpandedBoneIndices;

	// 본 트랜스폼 편집
	FVector EditBoneLocation;
	FVector EditBoneRotation;
	FVector EditBoneScale;
	bool bBoneTransformChanged = false;
	bool bBoneRotationEditing = false;
	bool bWasGizmoDragging = false;
	int32 DragStartBoneIndex = -1;

	// Animation 모드 상태
	int32 SelectedAnimationIndex = -1;
	UAnimSequence* CurrentAnimation = nullptr;
	bool bIsPlaying = false;
	float CurrentAnimationTime = 0.0f;
	bool bLoopAnimation = true;
	float PlaybackSpeed = 1.0f;

	// 타임라인 뷰
	float TimelineZoom = 1.0f;
	float TimelineScroll = 0.0f;
	bool bShowFrameNumbers = false;
	int32 PlaybackRangeStartFrame = 0;
	int32 PlaybackRangeEndFrame = -1;
	int32 WorkingRangeStartFrame = 0;
	int32 WorkingRangeEndFrame = -1;
	int32 ViewRangeStartFrame = 0;
	int32 ViewRangeEndFrame = -1;

	// Notify Track
	bool bNotifiesExpanded = false;
	int32 SelectedNotifyTrackIndex = -1;
	int32 SelectedNotifyIndex = -1;
	TArray<FString> NotifyTrackNames;
	std::set<int32> UsedTrackNumbers;
	bool bDraggingNotify = false;
	float NotifyDragOffsetX = 0.0f;

	// Notify 클립보드
	bool bHasNotifyClipboard = false;
	struct FAnimNotifyEvent NotifyClipboard;

	// Animation Recording
	bool bIsRecording = false;
	bool bShowRecordDialog = false;
	char RecordFileNameBuffer[128] = "";
	FString RecordedFileName = "";
	TArray<TMap<int32, FTransform>> RecordedFrames;
	float RecordStartTime = 0.0f;
	float RecordFrameRate = 30.0f;

	// Detail Panel
	int32 DetailPanelTabIndex = 0;
	uint32 BoneTransformModeFlags = 0x7;
	FVector ReferenceBoneLocation;
	FVector ReferenceBoneRotation;
	FVector ReferenceBoneScale;
	FVector MeshRelativeBoneLocation;
	FVector MeshRelativeBoneRotation;
	FVector MeshRelativeBoneScale;

	// AnimGraph 모드 상태
	UAnimStateMachine* StateMachine = nullptr;
	FWideString StateMachineFilePath;

	// BlendSpace2D 모드 상태
	UBlendSpace2D* BlendSpace = nullptr;
	FWideString BlendSpaceFilePath;
	FVector2D PreviewBlendPosition;
	int32 SelectedSampleIndex = -1;
	bool bIsDraggingSample = false;
};

/**
 * @brief 에디터 패널 기본 클래스
 * @details 모든 에디터 패널의 공통 인터페이스
 */
class SEditorPanel : public SWindow
{
public:
	SEditorPanel(SDynamicEditorWindow* InOwner) : Owner(InOwner) {}
	virtual ~SEditorPanel() = default;

	virtual void OnRender() override {}
	virtual void OnUpdate(float DeltaSeconds) {}

	// 패널이 현재 모드에서 활성화되는지 여부
	virtual bool IsActiveForMode(EEditorMode Mode) const { return true; }

protected:
	SDynamicEditorWindow* Owner = nullptr;
};

/**
 * @brief 동적 에디터 윈도우
 * @details 스플리터 기반 레이아웃으로 여러 에디터 모드를 지원하는 통합 윈도우
 *          - Skeletal: 스켈레탈 메시 편집
 *          - Animation: 애니메이션 편집
 *          - AnimGraph: 스테이트 머신 편집
 *          - BlendSpace2D: 블렌드 스페이스 편집
 */
class SDynamicEditorWindow : public SWindow
{
public:
	SDynamicEditorWindow();
	virtual ~SDynamicEditorWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

	// SWindow overrides
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// Viewport 렌더링
	void OnRenderViewport();

	// 에셋 로드
	void LoadSkeletalMesh(const FString& Path);
	void LoadAnimation(const FString& Path);
	void LoadAnimGraph(const FString& Path);
	void LoadBlendSpace(const FString& Path);

	// 직접 에셋 설정 (이미 로드된 객체 전달용)
	void SetBlendSpace(UBlendSpace2D* InBlendSpace);
	void SetAnimStateMachine(UAnimStateMachine* InStateMachine);

	// 모드 전환
	void SetEditorMode(EEditorMode NewMode);
	EEditorMode GetEditorMode() const;

	// Accessors
	FViewport* GetViewport() const;
	FViewportClient* GetViewportClient() const;
	AGizmoActor* GetGizmoActor() const;
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }
	bool IsFocused() const { return bIsFocused; }
	bool ShouldBlockEditorInput() const { return bIsOpen && bIsFocused; }

	// 탭/상태 접근
	FEditorTabState* GetActiveState() const { return ActiveState; }
	UWorld* GetWorld() const { return World; }
	ID3D11Device* GetDevice() const { return Device; }

	// 렌더 타겟 관리
	void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
	void RenderToPreviewRenderTarget();
	ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

	// 패널 접근 (자식 패널에서 사용)
	FRect GetViewportRect() const { return ViewportRect; }

protected:
	// 탭 관리
	FEditorTabState* CreateNewTab(const char* Name, EEditorMode Mode);
	void DestroyTab(FEditorTabState* State);
	void CloseTab(int32 Index);

	// 레이아웃 구성 (모드별)
	virtual void SetupLayoutForMode(EEditorMode Mode);

	// Bone Transform 헬퍼
	void UpdateBoneTransformFromSkeleton(FEditorTabState* State);
	void ApplyBoneTransform(FEditorTabState* State);
	void ExpandToSelectedBone(FEditorTabState* State, int32 BoneIndex);

protected:
	// 탭 상태
	FEditorTabState* ActiveState = nullptr;
	TArray<FEditorTabState*> Tabs;
	int32 ActiveTabIndex = -1;
	int32 NextTabId = 1;

	// 월드/디바이스 참조
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// 스플리터 레이아웃
	SSplitterV* MainSplitter = nullptr;
	SSplitterH* ContentSplitter = nullptr;
	SSplitterH* CenterRightSplitter = nullptr;
	SSplitterV* RightSplitter = nullptr;

	// 패널들 (모드에 따라 다르게 구성)
	TArray<SEditorPanel*> Panels;

	// 뷰포트 패널 참조 (마우스 입력용)
	class SViewportPreviewPanel* ViewportPanel = nullptr;

	// 뷰포트 영역 캐시
	FRect ViewportRect;

	// UI 상태
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;
	bool bRequestFocus = false;
	bool bIsFocused = false;

	// 전용 렌더 타겟
	ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
	ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
	ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
	ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
	uint32 PreviewRenderTargetWidth = 0;
	uint32 PreviewRenderTargetHeight = 0;

	// 렌더 타겟 관리
	void CreateRenderTarget(uint32 Width, uint32 Height);
	void ReleaseRenderTarget();
};
