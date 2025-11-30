#pragma once
#include <functional>

class UTexture;

/**
 * @brief 공통 재생 컨트롤 아이콘 세트
 */
struct FPlaybackIcons
{
	UTexture* GoToFront = nullptr;
	UTexture* StepBackwards = nullptr;
	UTexture* Play = nullptr;
	UTexture* Pause = nullptr;
	UTexture* StepForward = nullptr;
	UTexture* GoToEnd = nullptr;
	UTexture* Loop = nullptr;
	UTexture* LoopOff = nullptr;
};

/**
 * @brief 공통 재생 컨트롤 상태
 */
struct FPlaybackState
{
	bool bIsPlaying = false;
	bool bLoopAnimation = true;
	float PlaybackSpeed = 1.0f;
};

/**
 * @brief 공통 재생 컨트롤 콜백
 */
struct FPlaybackCallbacks
{
	std::function<void()> OnToFront;
	std::function<void()> OnStepBackwards;
	std::function<void()> OnPlayPause;
	std::function<void()> OnStepForward;
	std::function<void()> OnToEnd;
	std::function<void(bool)> OnLoopChanged;
	std::function<void(float)> OnSpeedChanged;
};

/**
 * @brief 공통 재생 컨트롤 UI 렌더링
 * @details Animation, BlendSpace2D 등에서 재사용 가능한 재생 컨트롤
 */
namespace PlaybackControls
{
	/**
	 * @brief 재생 컨트롤 버튼들 렌더링
	 * @param Icons 아이콘 텍스처 세트
	 * @param State 현재 재생 상태 (in/out)
	 * @param Callbacks 버튼 클릭 시 호출될 콜백들
	 * @param ButtonSize 버튼 크기 (기본 24.0f)
	 */
	void Render(const FPlaybackIcons& Icons, FPlaybackState& State,
	            const FPlaybackCallbacks& Callbacks, float ButtonSize = 24.0f);

	/**
	 * @brief 간단한 재생 컨트롤 (콜백 없이 State만 업데이트)
	 */
	void RenderSimple(const FPlaybackIcons& Icons, FPlaybackState& State, float ButtonSize = 24.0f);

	// === 개별 버튼 렌더링 (확장용) ===
	void BeginStyle();
	void EndStyle();
	bool RenderToFrontButton(UTexture* Icon, float ButtonSize);
	bool RenderStepBackwardsButton(UTexture* Icon, float ButtonSize);
	bool RenderPlayPauseButton(UTexture* PlayIcon, UTexture* PauseIcon, bool bIsPlaying, float ButtonSize);
	bool RenderStepForwardButton(UTexture* Icon, float ButtonSize);
	bool RenderToEndButton(UTexture* Icon, float ButtonSize);
	bool RenderLoopButton(UTexture* LoopIcon, UTexture* LoopOffIcon, bool bIsLooping, float ButtonSize);
	bool RenderSpeedControl(float& Speed, float MinSpeed = 0.1f, float MaxSpeed = 5.0f);
}
