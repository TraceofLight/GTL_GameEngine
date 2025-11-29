#pragma once
#include "FBodyInstance.h"

struct FConstraintInstance
{
public:
	PxJoint* JointHandle;

	FBodyInstance* BodyA;
	FBodyInstance* BodyB;
};
