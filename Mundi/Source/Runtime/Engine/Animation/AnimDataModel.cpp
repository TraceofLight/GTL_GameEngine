#include "pch.h"
#include "AnimDataModel.h"

IMPLEMENT_CLASS(UAnimDataModel)

UAnimDataModel::UAnimDataModel()
	: Skeleton(nullptr)
	, PlayLength(0.0f)
	, NumberOfFrames(0)
	, NumberOfKeys(0)
{
}

UAnimDataModel::~UAnimDataModel()
{
}
