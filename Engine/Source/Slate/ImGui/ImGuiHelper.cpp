#include "pch.h"
#include "ImGuiHelper.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "Renderer.h"
#include "GPUProfiler.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

IMPLEMENT_CLASS(UImGuiHelper)

UImGuiHelper::UImGuiHelper() = default;

UImGuiHelper::~UImGuiHelper()
{
	Release();
}

/**
 * @brief ImGui 초기화 함수
 */
void UImGuiHelper::Initialize(HWND InWindowHandle)
{
	if (bIsInitialized)
	{
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// 커스텀 다크 테마 적용
	SetupCustomTheme();

	ImGui_ImplWin32_Init(InWindowHandle);

    ImGuiIO& IO = ImGui::GetIO();
    // Restrict window moving to title bar only to avoid dragging content moving entire windows
    IO.ConfigWindowsMoveFromTitleBarOnly = true;
	// Use default ImGui font
	// IO.Fonts->AddFontDefault();

	// Note: This overload needs device and context to be passed explicitly
	// Use the new Initialize overload instead
	bIsInitialized = true;
}

/**
 * @brief ImGui 초기화 함수 (디바이스와 컨텍스트 포함)
 */
void UImGuiHelper::Initialize(HWND InWindowHandle, ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
	if (bIsInitialized)
	{
		return;
	}

	DeviceContext = InDeviceContext;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// 커스텀 다크 테마 적용
	SetupCustomTheme();

	ImGui_ImplWin32_Init(InWindowHandle);

	// TODO (동민, 한글) utf-8 설정을 푼다면 그냥 한글을 넣지 마세요.
	// ImGui는 utf-8을 기본으로 사용하기 때문에 그것에 맞춰 바꿔야 합니다.
    ImGuiIO& IO = ImGui::GetIO();
    // Restrict window moving to title bar only to avoid dragging content moving entire windows
    IO.ConfigWindowsMoveFromTitleBarOnly = true;
	ImFontConfig Cfg;
	/*
		오버 샘플은 폰트 비트맵을 만들 때 더 좋은 해상도로 래스터라이즈했다가
		목표 크기로 다운샘플해서 가장자리 품질을 높이는 슈퍼 샘플링 개념이다.
		따라서 베이크 시간/메모리 비용이 아깝다면 내릴 것
	*/
	Cfg.OversampleH = Cfg.OversampleV = 2;
	FString FontPath = GDataDir + "/Default/Font/malgun.ttf";
	IO.Fonts->AddFontFromFileTTF(FontPath.c_str(), 18.0f, &Cfg, IO.Fonts->GetGlyphRangesKorean());

	// Use default ImGui font
	// IO.Fonts->AddFontDefault();

	// Initialize ImGui DirectX11 backend with provided device and context
	ImGui_ImplDX11_Init(InDevice, InDeviceContext);

	bIsInitialized = true;
}

/**
 * @brief ImGui 자원 해제 함수
 */
void UImGuiHelper::Release()
{
	if (!bIsInitialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	bIsInitialized = false;
}

/**
 * @brief ImGui 새 프레임 시작
 */
void UImGuiHelper::BeginFrame() const
{
	if (!bIsInitialized)
	{
		return;
	}

	// GetInstance New Tick
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

/**
 * @brief ImGui 렌더링 종료 및 출력
 */
void UImGuiHelper::EndFrame() const
{
	if (!bIsInitialized || !DeviceContext)
	{
		return;
	}

	// Render ImGui
	ImGui::Render();

	{
		GPU_EVENT(DeviceContext, "UI Render");
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
}

/**
 * @brief WndProc Handler 래핑 함수
 * @return ImGui 자체 함수 반환
 */
LRESULT UImGuiHelper::WndProcHandler(HWND hWnd, uint32 msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

void UImGuiHelper::SetupCustomTheme()
{
	ImGuiStyle& Style = ImGui::GetStyle();

	// ===== Dracula Colorful/Saturated Theme =====
	// 더 진하고 컬러풀한 Dracula 변형
	// 기본 Dracula 팔레트에서 채도와 대비를 높인 버전

	// 모서리 둥글기 설정
	Style.WindowRounding = 4.0f;
	Style.FrameRounding = 4.0f;
	Style.GrabRounding = 4.0f;
	Style.TabRounding = 4.0f;
	Style.ScrollbarRounding = 6.0f;
	Style.ChildRounding = 4.0f;
	Style.PopupRounding = 4.0f;

	// 테두리 두께 설정
	Style.WindowBorderSize = 1.0f;
	Style.FrameBorderSize = 0.0f;
	Style.PopupBorderSize = 1.0f;
	Style.TabBorderSize = 0.0f;

	// 패딩 설정
	Style.WindowPadding = ImVec2(8.0f, 8.0f);
	Style.FramePadding = ImVec2(5.0f, 4.0f);
	Style.ItemSpacing = ImVec2(8.0f, 4.0f);
	Style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);

	ImVec4* Colors = Style.Colors;

	// Dracula Colorful 색상 정의 (더 진한 배경, 더 선명한 액센트)
	const ImVec4 Background = ImVec4(0.11f, 0.11f, 0.14f, 1.0f);         // #1c1c24 (더 진한 배경)
	const ImVec4 Surface = ImVec4(0.14f, 0.15f, 0.19f, 1.0f);            // #24262f (표면)
	const ImVec4 CurrentLine = ImVec4(0.22f, 0.23f, 0.30f, 1.0f);        // #383a4d (현재 라인)
	const ImVec4 Foreground = ImVec4(0.95f, 0.95f, 0.92f, 1.0f);         // #f2f2eb
	const ImVec4 Comment = ImVec4(0.45f, 0.51f, 0.70f, 1.0f);            // #7382b3 (더 밝은 코멘트)
	const ImVec4 Cyan = ImVec4(0.40f, 0.85f, 0.94f, 1.0f);               // #66d9f0 (진한 시안)
	const ImVec4 Green = ImVec4(0.35f, 0.92f, 0.55f, 1.0f);              // #59eb8c (진한 그린)
	const ImVec4 Orange = ImVec4(1.0f, 0.65f, 0.35f, 1.0f);              // #ffa659 (진한 오렌지)
	const ImVec4 Pink = ImVec4(1.0f, 0.42f, 0.70f, 1.0f);                // #ff6bb3 (진한 핑크)
	const ImVec4 Purple = ImVec4(0.70f, 0.50f, 1.0f, 1.0f);              // #b380ff (진한 퍼플)
	const ImVec4 Red = ImVec4(1.0f, 0.35f, 0.40f, 1.0f);                 // #ff5966 (진한 레드)
	const ImVec4 Yellow = ImVec4(0.98f, 0.95f, 0.45f, 1.0f);             // #faf273 (진한 옐로우)

	// 배경 색상
	Colors[ImGuiCol_WindowBg] = Background;
	Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);           // 투명 (부모 상속)
	Colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.12f, 0.98f);       // 더 어두운 팝업 배경

	// 테두리
	Colors[ImGuiCol_Border] = ImVec4(0.35f, 0.38f, 0.50f, 0.6f);         // 은은한 보라빛 테두리
	Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

	// 프레임 배경 (입력 필드, 콤보박스 등)
	Colors[ImGuiCol_FrameBg] = Surface;
	Colors[ImGuiCol_FrameBgHovered] = CurrentLine;
	Colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.30f, 0.40f, 1.0f);

	// 타이틀바
	Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
	Colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.15f, 0.19f, 1.0f);
	Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 0.75f);

	// 메뉴바
	Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);

	// 스크롤바
	Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
	Colors[ImGuiCol_ScrollbarGrab] = CurrentLine;
	Colors[ImGuiCol_ScrollbarGrabHovered] = Comment;
	Colors[ImGuiCol_ScrollbarGrabActive] = Purple;

	// 체크박스
	Colors[ImGuiCol_CheckMark] = Green;

	// 슬라이더
	Colors[ImGuiCol_SliderGrab] = Purple;
	Colors[ImGuiCol_SliderGrabActive] = Pink;

	// 버튼
	Colors[ImGuiCol_Button] = Surface;
	Colors[ImGuiCol_ButtonHovered] = CurrentLine;
	Colors[ImGuiCol_ButtonActive] = Purple;

	// 헤더 (트리 노드, CollapsingHeader 등)
	Colors[ImGuiCol_Header] = ImVec4(Purple.x, Purple.y, Purple.z, 0.25f);  // 반투명 퍼플
	Colors[ImGuiCol_HeaderHovered] = ImVec4(Purple.x, Purple.y, Purple.z, 0.45f);
	Colors[ImGuiCol_HeaderActive] = ImVec4(Purple.x, Purple.y, Purple.z, 0.65f);

	// 분리선
	Colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.38f, 0.50f, 0.5f);
	Colors[ImGuiCol_SeparatorHovered] = Purple;
	Colors[ImGuiCol_SeparatorActive] = Pink;

	// 크기 조절 그립
	Colors[ImGuiCol_ResizeGrip] = ImVec4(Purple.x, Purple.y, Purple.z, 0.25f);
	Colors[ImGuiCol_ResizeGripHovered] = ImVec4(Purple.x, Purple.y, Purple.z, 0.65f);
	Colors[ImGuiCol_ResizeGripActive] = Pink;

	// 탭 (컬러풀한 액센트)
	Colors[ImGuiCol_Tab] = Surface;
	Colors[ImGuiCol_TabHovered] = ImVec4(Purple.x, Purple.y, Purple.z, 0.45f);
	Colors[ImGuiCol_TabActive] = ImVec4(Purple.x, Purple.y, Purple.z, 0.35f);
	Colors[ImGuiCol_TabUnfocused] = Background;
	Colors[ImGuiCol_TabUnfocusedActive] = Surface;

	// 도킹 (DockSpace)
	#ifdef IMGUI_HAS_DOCK
	Colors[ImGuiCol_DockingPreview] = ImVec4(Purple.x, Purple.y, Purple.z, 0.7f);
	Colors[ImGuiCol_DockingEmptyBg] = Background;
	#endif

	// 테이블
	Colors[ImGuiCol_TableHeaderBg] = Surface;
	Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.35f, 0.38f, 0.50f, 0.6f);
	Colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.27f, 0.35f, 0.5f);
	Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.18f, 0.19f, 0.25f, 0.4f);

	// 텍스트 선택 (Cyan 하이라이트)
	Colors[ImGuiCol_TextSelectedBg] = ImVec4(Cyan.x, Cyan.y, Cyan.z, 0.35f);

	// 드래그 앤 드롭 타겟
	Colors[ImGuiCol_DragDropTarget] = Yellow;

	// 네비게이션 하이라이트
	Colors[ImGuiCol_NavHighlight] = Cyan;
	Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
	Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.1f, 0.1f, 0.15f, 0.5f);

	// 모달 윈도우 딤 배경
	Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.70f);

	// 텍스트 색상
	Colors[ImGuiCol_Text] = Foreground;
	Colors[ImGuiCol_TextDisabled] = Comment;

	// 플롯 (그래프) - 컬러풀한 색상
	Colors[ImGuiCol_PlotLines] = Cyan;
	Colors[ImGuiCol_PlotLinesHovered] = Pink;
	Colors[ImGuiCol_PlotHistogram] = Green;
	Colors[ImGuiCol_PlotHistogramHovered] = Yellow;
}
