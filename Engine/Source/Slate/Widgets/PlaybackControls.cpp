#include "pch.h"
#include "PlaybackControls.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "ImGui/imgui.h"

namespace PlaybackControls
{
	// === 스타일 관리 ===
	void BeginStyle()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));
	}

	void EndStyle()
	{
		ImGui::PopStyleVar(2);
	}

	// === 개별 버튼 렌더링 ===
	bool RenderToFrontButton(UTexture* Icon, float ButtonSize)
	{
		ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);
		bool bClicked = false;

		if (Icon && Icon->GetShaderResourceView())
		{
			bClicked = ImGui::ImageButton("##ToFront", Icon->GetShaderResourceView(), ButtonSizeVec);
		}
		else
		{
			bClicked = ImGui::Button("|<<", ButtonSizeVec);
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("To Front"); }

		return bClicked;
	}

	bool RenderStepBackwardsButton(UTexture* Icon, float ButtonSize)
	{
		ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);
		bool bClicked = false;

		if (Icon && Icon->GetShaderResourceView())
		{
			bClicked = ImGui::ImageButton("##StepBackwards", Icon->GetShaderResourceView(), ButtonSizeVec);
		}
		else
		{
			bClicked = ImGui::Button("|<", ButtonSizeVec);
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Previous Frame"); }

		return bClicked;
	}

	bool RenderPlayPauseButton(UTexture* PlayIcon, UTexture* PauseIcon, bool bIsPlaying, float ButtonSize)
	{
		ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);
		bool bClicked = false;

		if (bIsPlaying)
		{
			if (PauseIcon && PauseIcon->GetShaderResourceView())
			{
				bClicked = ImGui::ImageButton("##Pause", PauseIcon->GetShaderResourceView(), ButtonSizeVec);
			}
			else
			{
				bClicked = ImGui::Button("||", ButtonSizeVec);
			}
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Pause"); }
		}
		else
		{
			if (PlayIcon && PlayIcon->GetShaderResourceView())
			{
				bClicked = ImGui::ImageButton("##Play", PlayIcon->GetShaderResourceView(), ButtonSizeVec);
			}
			else
			{
				bClicked = ImGui::Button(">", ButtonSizeVec);
			}
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Play"); }
		}

		return bClicked;
	}

	bool RenderStepForwardButton(UTexture* Icon, float ButtonSize)
	{
		ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);
		bool bClicked = false;

		if (Icon && Icon->GetShaderResourceView())
		{
			bClicked = ImGui::ImageButton("##StepForward", Icon->GetShaderResourceView(), ButtonSizeVec);
		}
		else
		{
			bClicked = ImGui::Button(">|", ButtonSizeVec);
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Next Frame"); }

		return bClicked;
	}

	bool RenderToEndButton(UTexture* Icon, float ButtonSize)
	{
		ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);
		bool bClicked = false;

		if (Icon && Icon->GetShaderResourceView())
		{
			bClicked = ImGui::ImageButton("##ToEnd", Icon->GetShaderResourceView(), ButtonSizeVec);
		}
		else
		{
			bClicked = ImGui::Button(">>|", ButtonSizeVec);
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("To End"); }

		return bClicked;
	}

	bool RenderLoopButton(UTexture* LoopIcon, UTexture* LoopOffIcon, bool bIsLooping, float ButtonSize)
	{
		ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);
		bool bClicked = false;

		UTexture* Icon = bIsLooping ? LoopIcon : LoopOffIcon;
		if (Icon && Icon->GetShaderResourceView())
		{
			bClicked = ImGui::ImageButton("##Loop", Icon->GetShaderResourceView(), ButtonSizeVec);
		}
		else
		{
			if (bIsLooping)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
			}
			bClicked = ImGui::Button("Loop", ButtonSizeVec);
			if (bIsLooping)
			{
				ImGui::PopStyleColor();
			}
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip(bIsLooping ? "Loop: On" : "Loop: Off"); }

		return bClicked;
	}

	bool RenderSpeedControl(float& Speed, float MinSpeed, float MaxSpeed)
	{
		ImGui::Text("Speed:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		bool bChanged = ImGui::DragFloat("##Speed", &Speed, 0.05f, MinSpeed, MaxSpeed, "%.2fx");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Drag or click to edit playback speed"); }
		return bChanged;
	}

	// === 통합 렌더링 ===
	void Render(const FPlaybackIcons& Icons, FPlaybackState& State,
	            const FPlaybackCallbacks& Callbacks, float ButtonSize)
	{
		BeginStyle();

		// |<< ToFront
		if (RenderToFrontButton(Icons.GoToFront, ButtonSize))
		{
			if (Callbacks.OnToFront) { Callbacks.OnToFront(); }
		}
		ImGui::SameLine();

		// |< Previous
		if (RenderStepBackwardsButton(Icons.StepBackwards, ButtonSize))
		{
			if (Callbacks.OnStepBackwards) { Callbacks.OnStepBackwards(); }
		}
		ImGui::SameLine();

		// Play/Pause
		if (RenderPlayPauseButton(Icons.Play, Icons.Pause, State.bIsPlaying, ButtonSize))
		{
			State.bIsPlaying = !State.bIsPlaying;
			if (Callbacks.OnPlayPause) { Callbacks.OnPlayPause(); }
		}
		ImGui::SameLine();

		// >| Next
		if (RenderStepForwardButton(Icons.StepForward, ButtonSize))
		{
			if (Callbacks.OnStepForward) { Callbacks.OnStepForward(); }
		}
		ImGui::SameLine();

		// >>| ToEnd
		if (RenderToEndButton(Icons.GoToEnd, ButtonSize))
		{
			if (Callbacks.OnToEnd) { Callbacks.OnToEnd(); }
		}
		ImGui::SameLine();

		// Loop
		if (RenderLoopButton(Icons.Loop, Icons.LoopOff, State.bLoopAnimation, ButtonSize))
		{
			State.bLoopAnimation = !State.bLoopAnimation;
			if (Callbacks.OnLoopChanged) { Callbacks.OnLoopChanged(State.bLoopAnimation); }
		}
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(10, 0));
		ImGui::SameLine();

		// Speed
		if (RenderSpeedControl(State.PlaybackSpeed))
		{
			if (Callbacks.OnSpeedChanged) { Callbacks.OnSpeedChanged(State.PlaybackSpeed); }
		}

		EndStyle();
	}

	void RenderSimple(const FPlaybackIcons& Icons, FPlaybackState& State, float ButtonSize)
	{
		FPlaybackCallbacks EmptyCallbacks;
		Render(Icons, State, EmptyCallbacks, ButtonSize);
	}
}
