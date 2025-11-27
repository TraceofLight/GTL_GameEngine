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
	, CurrentQueryIndex(0)
	, bFrameActive(false)
{
	for (int32 i = 0; i < BufferCount; ++i)
	{
		FrameDisjointQueries[i] = nullptr;
	}

	if (!Device || !Context)
	{
		return;
	}

	D3D11_QUERY_DESC DisjointDesc = {};
	DisjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	for (int32 i = 0; i < BufferCount; ++i)
	{
		Device->CreateQuery(&DisjointDesc, &FrameDisjointQueries[i]);
	}
}

FGPUTimer::~FGPUTimer()
{
	for (int32 i = 0; i < BufferCount; ++i)
	{
		if (FrameDisjointQueries[i])
		{
			FrameDisjointQueries[i]->Release();
		}
	}

	for (auto& Timer : Timers)
	{
		if (Timer.DisjointQuery)
		{
			Timer.DisjointQuery->Release();
		}
		for (int32 i = 0; i < FTimerQuery::BufferCount; ++i)
		{
			if (Timer.StartQueries[i])
			{
				Timer.StartQueries[i]->Release();
			}
			if (Timer.EndQueries[i])
			{
				Timer.EndQueries[i]->Release();
			}
		}
	}
}

void FGPUTimer::BeginFrame()
{
	ID3D11Query* CurrentQuery = FrameDisjointQueries[CurrentQueryIndex];
	if (!Context || !CurrentQuery)
	{
		return;
	}

	Context->Begin(CurrentQuery);
	bFrameActive = true;
}

void FGPUTimer::EndFrame()
{
	ID3D11Query* CurrentQuery = FrameDisjointQueries[CurrentQueryIndex];
	if (!Context || !CurrentQuery || !bFrameActive)
	{
		return;
	}

	Context->End(CurrentQuery);
	bFrameActive = false;

	int32 ResolveQueryIndex = (CurrentQueryIndex + 1) % BufferCount;
	ID3D11Query* ResolveQuery = FrameDisjointQueries[ResolveQueryIndex];
	if (ResolveQuery)
	{
		ResolveTimers(ResolveQueryIndex);
	}

	CurrentQueryIndex = (CurrentQueryIndex + 1) % BufferCount;
}

void FGPUTimer::BeginTimer(const char* InName)
{
	if (!Context || !bFrameActive)
	{
		return;
	}

	FTimerQuery* Timer = FindOrCreateTimer(InName);
	if (Timer && Timer->StartQueries[CurrentQueryIndex])
	{
		Context->End(Timer->StartQueries[CurrentQueryIndex]);
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
	if (Timer && Timer->EndQueries[CurrentQueryIndex] && Timer->bActive)
	{
		Context->End(Timer->EndQueries[CurrentQueryIndex]);
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

	FTimerQuery NewTimer;
	NewTimer.Name = InName;

	D3D11_QUERY_DESC QueryDesc = {};
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP;

	for (int32 i = 0; i < FTimerQuery::BufferCount; ++i)
	{
		Device->CreateQuery(&QueryDesc, &NewTimer.StartQueries[i]);
		Device->CreateQuery(&QueryDesc, &NewTimer.EndQueries[i]);
	}

	Timers.push_back(NewTimer);
	return &Timers.back();
}

void FGPUTimer::ResolveTimers(int32 QueryIndex)
{
	if (!Context)
	{
		return;
	}

	ID3D11Query* DisjointQuery = FrameDisjointQueries[QueryIndex];
	if (!DisjointQuery)
	{
		return;
	}

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};
	HRESULT Result = Context->GetData(DisjointQuery, &DisjointData, sizeof(DisjointData), 0);

	if (Result != S_OK)
	{
		return;
	}

	if (DisjointData.Disjoint)
	{
		return;
	}

	for (auto& Timer : Timers)
	{
		ID3D11Query* StartQuery = Timer.StartQueries[QueryIndex];
		ID3D11Query* EndQuery = Timer.EndQueries[QueryIndex];

		if (!StartQuery || !EndQuery)
		{
			continue;
		}

		uint64 StartTime = 0;
		uint64 EndTime = 0;

		Result = Context->GetData(StartQuery, &StartTime, sizeof(uint64), 0);
		if (Result != S_OK)
		{
			continue;
		}

		Result = Context->GetData(EndQuery, &EndTime, sizeof(uint64), 0);
		if (Result != S_OK)
		{
			continue;
		}

		uint64 Delta = EndTime - StartTime;
		Timer.LastTimeMs = static_cast<float>(Delta) / static_cast<float>(DisjointData.Frequency) * 1000.0f;
	}
}
