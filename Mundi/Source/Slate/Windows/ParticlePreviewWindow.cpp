#include "pch.h"
#include "ParticlePreviewWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "World.h"
#include "ParticleSystemActor.h"
#include "Source/Runtime/Engine/Particle/ParticleSystem.h"
#include "Source/Runtime/Engine/Particle/ParticleSystemComponent.h"
#include "CameraActor.h"
#include "ImGui/imgui.h"

SParticlePreviewWindow::SParticlePreviewWindow()
{
	ViewportRect = FRect(0, 0, 0, 0);
}

SParticlePreviewWindow::~SParticlePreviewWindow()
{
	PreviewActor = nullptr;

	if (ViewportClient)
	{
		delete ViewportClient;
		ViewportClient = nullptr;
	}

	if (Viewport)
	{
		delete Viewport;
		Viewport = nullptr;
	}

	if (World)
	{
		delete World;
		World = nullptr;
	}
}

bool SParticlePreviewWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
	Device = InDevice;
	SetRect(StartX, StartY, StartX + Width, StartY + Height);

	// TODO: Preview World/Viewport 구현
	// 현재는 스텁 구현
	bIsOpen = true;
	return true;
}

void SParticlePreviewWindow::OnRender()
{
	if (!bIsOpen)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}

	if (ImGui::Begin("Particle Preview", &bIsOpen, flags))
	{
		OnRenderControlPanel();
		ImGui::Separator();
		OnRenderViewport();
	}
	ImGui::End();
}

void SParticlePreviewWindow::OnRenderControlPanel()
{
	if (ImGui::Button("Restart"))
	{
		RestartParticleSystem();
	}

	ImGui::SameLine();
	ImGui::Text("(Preview not implemented yet)");
}

void SParticlePreviewWindow::OnRenderViewport()
{
	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	if (availableSize.x < 10 || availableSize.y < 10)
	{
		return;
	}

	// TODO: 실제 뷰포트 렌더링 구현
	ImGui::Text("Particle Preview Viewport");
	ImGui::Text("Size: %.0f x %.0f", availableSize.x, availableSize.y);
}

void SParticlePreviewWindow::OnUpdate(float DeltaSeconds)
{
	if (!bIsOpen || !World)
	{
		return;
	}

	// TODO: World tick
}

void SParticlePreviewWindow::OnMouseMove(FVector2D MousePos)
{
	// TODO: 카메라 회전
}

void SParticlePreviewWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	// TODO: 카메라 조작
}

void SParticlePreviewWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	// TODO: 카메라 조작
}

void SParticlePreviewWindow::LoadParticleSystem(const FString& Path)
{
	// TODO: .psys 파일 로딩 구현
}

void SParticlePreviewWindow::SetParticleSystem(UParticleSystem* InTemplate)
{
	// TODO: 프리뷰 구현
}

void SParticlePreviewWindow::RestartParticleSystem()
{
	// TODO: 프리뷰 구현
}
