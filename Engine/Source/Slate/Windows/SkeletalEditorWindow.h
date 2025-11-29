#pragma once
#include "Window.h"
#include "SplitterH.h"
#include "SplitterV.h"

class FViewport;
class FViewportClient;
class UWorld;
class USkeletalMesh;
class UAnimSequence;
class ViewerState;
struct ID3D11Device;
class UTexture;

// Forward declarations for panel classes
class SSkeletalToolbarPanel;
class SSkeletalAssetPanel;
class SSkeletalViewportPanel;
class SSkeletalBoneTreePanel;
class SSkeletalDetailPanel;

/**
 * @brief 스켈레탈 메시 에디터 윈도우
 * @details 스플리터 기반 레이아웃
 *          - 상단: Toolbar (메뉴바)
 *          - 좌측: Asset Panel (FBX 로드)
 *          - 중앙: Viewport (3D 프리뷰)
 *          - 우상단: Bone Tree (본 계층 구조)
 *          - 우하단: Detail Panel (선택된 본 속성)
 */
class SSkeletalEditorWindow : public SWindow
{
public:
	SSkeletalEditorWindow();
	virtual ~SSkeletalEditorWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

	// SWindow overrides
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// Viewport 렌더링 (SlateManager에서 호출)
	void OnRenderViewport();

	// 스켈레탈 메시 로드
	void LoadSkeletalMesh(const FString& Path);

	// Accessors
	FViewport* GetViewport() const;
	FViewportClient* GetViewportClient() const;
	class AGizmoActor* GetGizmoActor() const;
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }
	bool IsFocused() const { return bIsFocused; }
	bool ShouldBlockEditorInput() const { return bIsOpen && bIsFocused; }

	// 패널 접근용
	ViewerState* GetActiveState() const { return ActiveState; }

	// 렌더 타겟 관리
	void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
	void RenderToPreviewRenderTarget();

	// 렌더 타겟 접근자
	ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

private:
	// Bone Transform 헬퍼
	void UpdateBoneTransformFromSkeleton(ViewerState* State);
	void ApplyBoneTransform(ViewerState* State);
	void ExpandToSelectedBone(ViewerState* State, int32 BoneIndex);

	// 탭 상태
	ViewerState* ActiveState = nullptr;
	TArray<ViewerState*> Tabs;
	int32 ActiveTabIndex = -1;
	int32 NextTabId = 1;  // 고유 탭 ID 생성용 카운터

	// 레거시 참조
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// 스플리터 레이아웃
	// MainSplitter (V): Toolbar | Content
	//   ContentSplitter (H): Left | CenterRight
	//     CenterRightSplitter (H): Center | Right
	//       RightSplitter (V): BoneTree | Detail
	SSplitterV* MainSplitter = nullptr;
	SSplitterH* ContentSplitter = nullptr;
	SSplitterH* CenterRightSplitter = nullptr;
	SSplitterV* RightSplitter = nullptr;

	// 패널들
	SSkeletalToolbarPanel* ToolbarPanel = nullptr;
	SSkeletalAssetPanel* AssetPanel = nullptr;
	SSkeletalViewportPanel* ViewportPanel = nullptr;
	SSkeletalBoneTreePanel* BoneTreePanel = nullptr;
	SSkeletalDetailPanel* DetailPanel = nullptr;

	// 뷰포트 영역 캐시 (실제 3D 렌더링 영역)
	FRect ViewportRect;

	// UI 상태
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;
	bool bRequestFocus = false;
	bool bIsFocused = false;

	// 전용 렌더 타겟 (스켈레탈 메쉬 프리뷰용)
	ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
	ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
	ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
	ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
	uint32 PreviewRenderTargetWidth = 0;
	uint32 PreviewRenderTargetHeight = 0;

	// 렌더 타겟 관리 (내부용)
	void CreateRenderTarget(uint32 Width, uint32 Height);
	void ReleaseRenderTarget();
};

// ============================================================================
// Panel Classes
// ============================================================================

/**
 * @brief 스켈레탈 에디터 툴바 패널
 */
class SSkeletalToolbarPanel : public SWindow
{
public:
	SSkeletalToolbarPanel(SSkeletalEditorWindow* InOwner);
	virtual void OnRender() override;

private:
	SSkeletalEditorWindow* Owner = nullptr;
};

/**
 * @brief 스켈레탈 에디터 에셋 패널 (FBX 로드)
 */
class SSkeletalAssetPanel : public SWindow
{
public:
	SSkeletalAssetPanel(SSkeletalEditorWindow* InOwner);
	virtual void OnRender() override;

private:
	SSkeletalEditorWindow* Owner = nullptr;
};

/**
 * @brief 스켈레탈 에디터 뷰포트 패널
 */
class SSkeletalViewportPanel : public SWindow
{
public:
	SSkeletalViewportPanel(SSkeletalEditorWindow* InOwner);
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;

	// 3D 콘텐츠 렌더링 영역 (실제 뷰포트 영역)
	FRect ContentRect;

private:
	SSkeletalEditorWindow* Owner = nullptr;
};

/**
 * @brief 스켈레탈 에디터 본 트리 패널
 */
class SSkeletalBoneTreePanel : public SWindow
{
public:
	SSkeletalBoneTreePanel(SSkeletalEditorWindow* InOwner);
	virtual void OnRender() override;

private:
	SSkeletalEditorWindow* Owner = nullptr;
	void RenderBoneTree(ViewerState* State);
};

/**
 * @brief 스켈레탈 에디터 디테일 패널 (UE5 스타일 탭 분리)
 * @details Asset Details 탭: Material Slots, LOD Picker
 *          Details 탭: Bone Transform (Bone/Reference/Mesh Relative)
 */
class SSkeletalDetailPanel : public SWindow
{
public:
	SSkeletalDetailPanel(SSkeletalEditorWindow* InOwner);
	virtual void OnRender() override;

private:
	SSkeletalEditorWindow* Owner = nullptr;

	// 탭별 렌더링
	void RenderAssetDetailsTab(ViewerState* State);
	void RenderDetailsTab(ViewerState* State);

	// Details 탭 하위 섹션
	void RenderBoneSection(ViewerState* State);
	void RenderTransformsSection(ViewerState* State);

	// Asset Details 탭 하위 섹션
	void RenderMaterialSlotsSection(ViewerState* State);
	void RenderLODPickerSection(ViewerState* State);

	// 트랜스폼 편집 헬퍼
	bool RenderTransformEditor(const char* Label, FVector& Location, FVector& Rotation, FVector& Scale, bool bReadOnly = false);
	void SyncBoneTransformFromViewport(ViewerState* State);
	void ApplyBoneTransformToViewport(ViewerState* State);
};
