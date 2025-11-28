#pragma once
#include "BodyInstance.h"

struct FConstraintInstance
{
public:
	PxJoint* JointHandle;

	FBodyInstance* BodyA;
	FBodyInstance* BodyB;
};
