#pragma once
#include "Enums.h"

class FViewport;
class FViewportClient;
class UTexture;
class AGizmoActor;
class SViewportWindow;
struct ID3D11Device;

/**
 * @brief 뷰포트 툴바 위젯 (공용)
 * @details 여러 에디터(SkeletalEditor, ParticleEditor 등)에서 공용으로 사용하는 뷰포트 툴바
 *          - 기즈모 모드 버튼 (Select, Translate, Rotate, Scale)
 *          - 기즈모 공간 버튼 (World/Local)
 *          - 카메라 옵션 드롭다운
 *          - 카메라 속도 버튼
 *          - 뷰모드 드롭다운
 *          - ShowFlag 드롭다운
 *          - (선택) 뷰포트 레이아웃 전환 버튼
 */
class SViewportToolbarWidget
{
public:
	SViewportToolbarWidget();
	~SViewportToolbarWidget();

	// 초기화 (아이콘 로드)
	void Initialize(ID3D11Device* InDevice);

	// 툴바 렌더링
	// @param ViewportClient 현재 뷰포트 클라이언트
	// @param GizmoActor 기즈모 액터 (nullptr이면 기즈모 버튼 비활성화)
	// @param bShowCameraDropdown 카메라 옵션 드롭다운 표시 여부
	// @param bShowLayoutSwitch 뷰포트 레이아웃 전환 버튼 표시 여부
	// @param ViewportType 뷰포트 타입 (카메라 드롭다운용)
	// @param ViewportName 뷰포트 이름 (카메라 드롭다운용)
	// @param OwnerViewport 소유 뷰포트 윈도우 (레이아웃 스위치용, nullptr이면 스위치 버튼 비활성화)
	void Render(FViewportClient* ViewportClient, AGizmoActor* GizmoActor,
		bool bShowCameraDropdown = false, bool bShowLayoutSwitch = false,
		EViewportType ViewportType = EViewportType::Perspective, const char* ViewportName = "Perspective",
		SViewportWindow* OwnerViewport = nullptr);

	// 툴바 높이 반환
	float GetToolbarHeight() const { return ToolbarHeight; }

private:
	// 각 섹션 렌더링
	void RenderGizmoModeButtons(AGizmoActor* GizmoActor) const;
	void RenderGizmoSpaceButton(AGizmoActor* GizmoActor) const;
	void RenderCameraOptionDropdownMenu(const FViewportClient* ViewportClient, EViewportType ViewportType, const char* ViewportName, SViewportWindow* OwnerViewport);
	void RenderCameraSpeedButton(FViewportClient* ViewportClient);
	void RenderViewModeDropdownMenu(FViewportClient* ViewportClient) const;
	void RenderShowFlagDropdownMenu(const FViewportClient* ViewportClient) const;
	void RenderViewportLayoutSwitchButton(SViewportWindow* OwnerViewport);

	// 헬퍼 함수
	float CalculateCameraSpeed(const FViewportClient* ViewportClient) const;

	// 아이콘 로드
	void LoadIcons(ID3D11Device* Device);

private:
	ID3D11Device* Device = nullptr;

	// 툴바 설정
	static constexpr float ToolbarHeight = 30.0f;

	// 기즈모 아이콘
	UTexture* IconSelect = nullptr;
	UTexture* IconMove = nullptr;
	UTexture* IconRotate = nullptr;
	UTexture* IconScale = nullptr;
	UTexture* IconWorldSpace = nullptr;
	UTexture* IconLocalSpace = nullptr;

	// 카메라/뷰포트 모드 아이콘
	UTexture* IconCamera = nullptr;
	UTexture* IconPerspective = nullptr;
	UTexture* IconTop = nullptr;
	UTexture* IconBottom = nullptr;
	UTexture* IconLeft = nullptr;
	UTexture* IconRight = nullptr;
	UTexture* IconFront = nullptr;
	UTexture* IconBack = nullptr;

	// 카메라 설정 아이콘
	UTexture* IconSpeed = nullptr;
	UTexture* IconFOV = nullptr;
	UTexture* IconNearClip = nullptr;
	UTexture* IconFarClip = nullptr;

	// 뷰모드 아이콘
	UTexture* IconViewMode_Lit = nullptr;
	UTexture* IconViewMode_Unlit = nullptr;
	UTexture* IconViewMode_Wireframe = nullptr;
	UTexture* IconViewMode_BufferVis = nullptr;

	// ShowFlag 아이콘
	UTexture* IconShowFlag = nullptr;
	UTexture* IconRevert = nullptr;
	UTexture* IconStats = nullptr;
	UTexture* IconHide = nullptr;
	UTexture* IconBVH = nullptr;
	UTexture* IconGrid = nullptr;
	UTexture* IconDecal = nullptr;
	UTexture* IconStaticMesh = nullptr;
	UTexture* IconSkeletalMesh = nullptr;
	UTexture* IconBillboard = nullptr;
	UTexture* IconEditorIcon = nullptr;
	UTexture* IconFog = nullptr;
	UTexture* IconCollision = nullptr;
	UTexture* IconAntiAliasing = nullptr;
	UTexture* IconTile = nullptr;
	UTexture* IconShadow = nullptr;
	UTexture* IconShadowAA = nullptr;
	UTexture* IconSkinning = nullptr;
	UTexture* IconParticle = nullptr;

	// 뷰포트 레이아웃 아이콘
	UTexture* IconSingleToMultiViewport = nullptr;
	UTexture* IconMultiToSingleViewport = nullptr;

	// 카메라 속도 설정
	int32 CameraSpeedSetting = 4;
	static constexpr float SPEED_MULTIPLIERS[8] = { 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
	static constexpr float BASE_CAMERA_SPEED = 10.0f;

	// ViewMode 서브모드 상태
	int32 CurrentLitSubMode = 0;
	int32 CurrentBufferVisSubMode = 1;
};
