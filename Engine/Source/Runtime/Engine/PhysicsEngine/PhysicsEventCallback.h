#pragma once

#include "PxPhysicsAPI.h"

using namespace physx;

class FPhysicsEventCallback : public PxSimulationEventCallback
{
public:

	// 물리적인 충돌
	void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override;

	// Trigger
	void onTrigger(PxTriggerPair* pairs, PxU32 count) override;


	// 필요할 때 오버라이드해서 사용
	virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override {}
	virtual void onWake(PxActor** actors, PxU32 count) override {}
	virtual void onSleep(PxActor** actors, PxU32 count) override {}
	virtual void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}
};
