#pragma once

#include "Object.h"

/**
 * @brief Single Animation Play Data
 * @details SkeletalMeshComponent에서 단일 애니메이션 재생 설정을 저장하는 구조체 (UE FSingleAnimationPlayData)
 *
 * @param AnimToPlay 재생할 애니메이션 애셋
 * @param bSavedLooping 루프 재생 여부 (기본 설정)
 * @param bSavedPlaying 자동 재생 여부 (기본 설정)
 * @param SavedPosition 시작 재생 위치 (초)
 * @param SavedPlayRate 재생 속도 배율
 *
 * UE Reference: Engine/Source/Runtime/Engine/Public/SingleAnimationPlayData.h
 */
USTRUCT()
struct FSingleAnimationPlayData
{
	// The default sequence to play on this skeletal mesh
	UPROPERTY(EditAnywhere, Category="Animation")
	class UAnimSequence* AnimToPlay;

	// Default setting for looping for SequenceToPlay. This is not current state of looping.
	UPROPERTY(EditAnywhere, Category="Animation")
	bool bSavedLooping;

	// Default setting for playing for SequenceToPlay. This is not current state of playing.
	UPROPERTY(EditAnywhere, Category="Animation")
	bool bSavedPlaying;

	// Default setting for position of SequenceToPlay to play.
	UPROPERTY(EditAnywhere, Category="Animation")
	float SavedPosition;

	// Default setting for play rate of SequenceToPlay to play.
	UPROPERTY(EditAnywhere, Category="Animation")
	float SavedPlayRate;

	FSingleAnimationPlayData()
		: AnimToPlay(nullptr)
		, bSavedLooping(true)
		, bSavedPlaying(true)
		, SavedPosition(0.0f)
		, SavedPlayRate(1.0f)
	{
	}

	// Called when initialized
	void Initialize(class UAnimSingleNodeInstance* Instance);

	// Populates this play data with the current state of the supplied instance
	void PopulateFrom(class UAnimSingleNodeInstance* Instance);

	// Validates position against animation length
	void ValidatePosition();
};
