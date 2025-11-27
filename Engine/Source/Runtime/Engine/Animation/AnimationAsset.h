#pragma once
#include "Source/Runtime/AssetManagement/ResourceBase.h"

/**
 * UAnimationAsset
 * 모든 애니메이션 에셋의 베이스 클래스
 */
class UAnimationAsset : public UResourceBase
{
public:
	DECLARE_CLASS(UAnimationAsset, UResourceBase)

	UAnimationAsset();
	virtual ~UAnimationAsset() override;
};
