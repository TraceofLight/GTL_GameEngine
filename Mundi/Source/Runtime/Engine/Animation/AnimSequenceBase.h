#pragma once
#include "AnimationAsset.h"
#include "AnimDataModel.h"

class UAnimDataModel;
struct FAnimNotifyEvent;

/**
 * UAnimSequenceBase
 * 시퀀스 기반 애니메이션의 베이스 클래스
 */
class UAnimSequenceBase : public UAnimationAsset
{
public:
	DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset)

	UAnimSequenceBase();
	virtual ~UAnimSequenceBase() override;

	// 데이터 모델 접근
	virtual UAnimDataModel* GetDataModel() const { return DataModel; }
	virtual void SetDataModel(UAnimDataModel* InDataModel) { DataModel = InDataModel; }

	// Notify 관리
	void SortNotifies();
	void GetAnimNotifiesFromDeltaPositions(float PreviousTime, float CurrentTime, TArray<const FAnimNotifyEvent*>& OutNotifies) const;
	void AddNotify(const FAnimNotifyEvent& NewNotify);
	void ClearNotifies();

	// Getters
	virtual float GetPlayLength() const { return DataModel ? DataModel->GetPlayLength() : 0.0f; }
	bool IsLooping() const { return bLoop; }

protected:
	UAnimDataModel* DataModel;

private:
	TArray<FAnimNotifyEvent> Notifies;
	float SequenceLength;
	float RateScale;
	bool bLoop;
};
