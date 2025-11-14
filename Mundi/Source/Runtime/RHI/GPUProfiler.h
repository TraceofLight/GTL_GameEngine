#pragma once
#include <d3d11_1.h>

// GPU 이벤트 마킹 매크로
#define GPU_EVENT(Context, Name) FGPUEventScope TOKENPASTE2(__GPUEvent_, __LINE__)(Context, Name)
#define GPU_EVENT_TIMER(Context, Name, Timer) FGPUEventScope TOKENPASTE2(__GPUEvent_, __LINE__)(Context, Name, Timer)
#define TOKENPASTE2(x, y) TOKENPASTE3(x, y)
#define TOKENPASTE3(x, y) x##y

// GPU 타이머 매크로
#define GPU_TIMER(Timer, Name) FGPUTimerScope TOKENPASTE2(__GPUTimer_, __LINE__)(Timer, Name)

class FGPUTimer;

struct FTimerQuery
{
	static constexpr int32 BufferCount = 32;

	FString Name;
	ID3D11Query* DisjointQuery;
	ID3D11Query* StartQueries[BufferCount];
	ID3D11Query* EndQueries[BufferCount];
	float LastTimeMs;
	bool bActive;

	FTimerQuery()
		: DisjointQuery(nullptr)
		  , LastTimeMs(0.0f)
		  , bActive(false)
	{
		for (int32 i = 0; i < BufferCount; ++i)
		{
			StartQueries[i] = nullptr;
			EndQueries[i] = nullptr;
		}
	}
};

/**
 * @brief GPU 디버그 이벤트 마커 RAII 래퍼
 * @details PIX, RenderDoc 등에서 Pass 단위로 구분하여 표시
 *
 * 생성자에서 BeginEvent, 소멸자에서 EndEvent 자동 호출
 *
 * 사용 예시:
 * void MyRenderPass()
 * {
 *     GPU_EVENT(RHI->GetDeviceContext(), "GBuffer Pass");
 *     // 렌더링 코드...
 * }
 */
class FGPUEventScope
{
public:
	FGPUEventScope(ID3D11DeviceContext* InContext, const char* InName);
	FGPUEventScope(ID3D11DeviceContext* InContext, const char* InName, FGPUTimer* InTimer);
	~FGPUEventScope();

	FGPUEventScope(const FGPUEventScope&) = delete;
	FGPUEventScope& operator=(const FGPUEventScope&) = delete;

private:
	static std::wstring ConvertToWide(const char* InStr);

	ID3D11DeviceContext* Context;
	ID3DUserDefinedAnnotation* Annotation;
	FGPUTimer* Timer;
	const char* Name;
};

/**
 * @brief GPU 타이밍 측정 클래스
 * @details D3D11 Query를 사용하여 GPU 실행 시간 측정
 *
 * 사용 예시:
 * FGPUTimer Timer(DeviceContext);
 * Timer.BeginTimer("MyPass");
 * // GPU 렌더링 코드...
 * Timer.EndTimer("MyPass");
 *
 * // 다음 프레임에서 결과 확인
 * float TimeMs = Timer.GetTime("MyPass");
 */
class FGPUTimer
{
public:
	FGPUTimer(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
	~FGPUTimer();

	void BeginFrame();
	void EndFrame();

	void BeginTimer(const char* InName);
	void EndTimer(const char* InName);

	// 이전 프레임의 측정 결과 가져오기 (밀리초)
	float GetTime(const char* InName) const;

	// 모든 타이머 결과 출력
	void PrintTimings() const;

private:
	FTimerQuery* FindOrCreateTimer(const char* InName);
	void ResolveTimers(int32 QueryIndex);

	ID3D11Device* Device;
	ID3D11DeviceContext* Context;
	TArray<FTimerQuery> Timers;

	static constexpr int32 BufferCount = 32;
	ID3D11Query* FrameDisjointQueries[BufferCount];
	int32 CurrentQueryIndex;
	bool bFrameActive;
};

/**
 * @brief GPU 타이머 RAII 래퍼
 * @details 생성자에서 BeginTimer, 소멸자에서 EndTimer 자동 호출
 */
class FGPUTimerScope
{
public:
	FGPUTimerScope(FGPUTimer* InTimer, const char* InName)
		: Timer(InTimer), Name(InName)
	{
		if (Timer)
		{
			Timer->BeginTimer(Name);
		}
	}

	~FGPUTimerScope()
	{
		if (Timer)
		{
			Timer->EndTimer(Name);
		}
	}

	FGPUTimerScope(const FGPUTimerScope&) = delete;
	FGPUTimerScope& operator=(const FGPUTimerScope&) = delete;

private:
	FGPUTimer* Timer;
	const char* Name;
};
