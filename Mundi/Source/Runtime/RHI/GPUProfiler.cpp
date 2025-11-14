#include "pch.h"
#include "GPUProfiler.h"

FGPUEventScope::FGPUEventScope(ID3D11DeviceContext* InContext, const char* InName)
	: Context(InContext)
	, Annotation(nullptr)
	, Timer(nullptr)
	, Name(InName)
{
	if (!Context)
	{
		return;
	}

	if (SUCCEEDED(Context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(&Annotation))))
	{
		std::wstring WideName = ConvertToWide(InName);
		Annotation->BeginEvent(WideName.c_str());
	}
}

FGPUEventScope::FGPUEventScope(ID3D11DeviceContext* InContext, const char* InName, FGPUTimer* InTimer)
	: Context(InContext)
	, Annotation(nullptr)
	, Timer(InTimer)
	, Name(InName)
{
	if (!Context)
	{
		return;
	}

	if (SUCCEEDED(Context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(&Annotation))))
	{
		std::wstring WideName = ConvertToWide(InName);
		Annotation->BeginEvent(WideName.c_str());
	}

	if (Timer)
	{
		Timer->BeginTimer(Name);
	}
}

FGPUEventScope::~FGPUEventScope()
{
	if (Timer)
	{
		Timer->EndTimer(Name);
	}

	if (Annotation)
	{
		Annotation->EndEvent();
		Annotation->Release();
	}
}

std::wstring FGPUEventScope::ConvertToWide(const char* InStr)
{
	if (!InStr)
	{
		return L"";
	}

	const int32 Len = MultiByteToWideChar(CP_UTF8, 0, InStr, -1, nullptr, 0);
	if (Len <= 0)
	{
		return L"";
	}

	std::wstring Result(Len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, InStr, -1, Result.data(), static_cast<int32>(Result.size()));
	return Result;
}


FGPUTimer::FGPUTimer(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
	: Device(InDevice)
	, Context(InContext)
	, FrameDisjointQuery(nullptr)
	, bFrameActive(false)
{
	if (!Device || !Context)
	{
		return;
	}

	// 프레임 단위 Disjoint Query 생성
	D3D11_QUERY_DESC DisjointDesc = {};
	DisjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	Device->CreateQuery(&DisjointDesc, &FrameDisjointQuery);
}

FGPUTimer::~FGPUTimer()
{
	if (FrameDisjointQuery)
	{
		FrameDisjointQuery->Release();
	}

	for (auto& Timer : Timers)
	{
		if (Timer.DisjointQuery) Timer.DisjointQuery->Release();
		if (Timer.StartQuery) Timer.StartQuery->Release();
		if (Timer.EndQuery) Timer.EndQuery->Release();
	}
}

void FGPUTimer::BeginFrame()
{
	if (!Context || !FrameDisjointQuery)
	{
		return;
	}

	// 이전 프레임 결과 수집
	ResolveTimers();

	// 새 프레임 시작
	Context->Begin(FrameDisjointQuery);
	bFrameActive = true;
}

void FGPUTimer::EndFrame()
{
	if (!Context || !FrameDisjointQuery || !bFrameActive)
	{
		return;
	}

	Context->End(FrameDisjointQuery);
	bFrameActive = false;
}

void FGPUTimer::BeginTimer(const char* InName)
{
	if (!Context || !bFrameActive)
	{
		return;
	}

	FTimerQuery* Timer = FindOrCreateTimer(InName);
	if (Timer && Timer->StartQuery)
	{
		Context->End(Timer->StartQuery);
		Timer->bActive = true;
	}
}

void FGPUTimer::EndTimer(const char* InName)
{
	if (!Context || !bFrameActive)
	{
		return;
	}

	FTimerQuery* Timer = FindOrCreateTimer(InName);
	if (Timer && Timer->EndQuery && Timer->bActive)
	{
		Context->End(Timer->EndQuery);
		Timer->bActive = false;
	}
}

float FGPUTimer::GetTime(const char* InName) const
{
	if (!InName)
	{
		return 0.0f;
	}

	for (const auto& Timer : Timers)
	{
		if (Timer.Name == InName)
		{
			return Timer.LastTimeMs;
		}
	}
	return 0.0f;
}

void FGPUTimer::PrintTimings() const
{
	UE_LOG("=== GPU Timings ===");
	for (const auto& Timer : Timers)
	{
		if (Timer.LastTimeMs > 0.0f)
		{
			UE_LOG("  %s: %.3f ms", Timer.Name.c_str(), Timer.LastTimeMs);
		}
	}
}

FTimerQuery* FGPUTimer::FindOrCreateTimer(const char* InName)
{
	if (!Device || !InName)
	{
		return nullptr;
	}

	// 기존 타이머 찾기
	for (auto& Timer : Timers)
	{
		if (Timer.Name == InName)
		{
			return &Timer;
		}
	}

	// 새 타이머 생성
	FTimerQuery NewTimer;
	NewTimer.Name = InName;

	D3D11_QUERY_DESC QueryDesc = {};
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP;

	Device->CreateQuery(&QueryDesc, &NewTimer.StartQuery);
	Device->CreateQuery(&QueryDesc, &NewTimer.EndQuery);

	Timers.push_back(NewTimer);
	return &Timers.back();
}

void FGPUTimer::ResolveTimers()
{
	if (!Context || !FrameDisjointQuery)
	{
		return;
	}

	// Disjoint Query 결과 대기
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};
	HRESULT hr = Context->GetData(FrameDisjointQuery, &DisjointData, sizeof(DisjointData), 0);

	if (hr != S_OK || DisjointData.Disjoint)
	{
		// GPU 타이밍이 불연속적이면 결과 무시
		return;
	}

	// 각 타이머 결과 수집
	for (auto& Timer : Timers)
	{
		if (!Timer.StartQuery || !Timer.EndQuery)
		{
			continue;
		}

		uint64 StartTime = 0;
		uint64 EndTime = 0;

		hr = Context->GetData(Timer.StartQuery, &StartTime, sizeof(uint64), 0);
		if (hr != S_OK)
		{
			continue;
		}

		hr = Context->GetData(Timer.EndQuery, &EndTime, sizeof(uint64), 0);
		if (hr != S_OK)
		{
			continue;
		}

		// 시간 계산 (밀리초)
		uint64 Delta = EndTime - StartTime;
		Timer.LastTimeMs = static_cast<float>(Delta) / static_cast<float>(DisjointData.Frequency) * 1000.0f;
	}
}
