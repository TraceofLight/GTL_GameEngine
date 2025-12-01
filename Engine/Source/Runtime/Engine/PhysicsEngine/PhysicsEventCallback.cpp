#include "pch.h"
#include "PhysicsEventCallback.h"
#include "PrimitiveComponent.h"

void FPhysicsEventCallback::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs)
{
	for (PxU32 i = 0; i < nbPairs; ++i)
	{
		const PxContactPair& cp = pairs[i];

		if (cp.flags & (PxContactPairFlag::eREMOVED_SHAPE_0 | PxContactPairFlag::eREMOVED_SHAPE_1))
			continue;

		UPrimitiveComponent* CompA = static_cast<UPrimitiveComponent*>(pairHeader.actors[0]->userData);
		UPrimitiveComponent* CompB = static_cast<UPrimitiveComponent*>(pairHeader.actors[1]->userData);

		if (CompA)
		{
			CompA->OnComponentHit(CompB);
		}

		if (CompB)
		{
			CompB->OnComponentHit(CompA);
		}
	}
}

void FPhysicsEventCallback::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
	for (PxU32 i = 0; i < count; ++i)
	{
		if (pairs[i].flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
			continue;

		UPrimitiveComponent* TriggerComp = static_cast<UPrimitiveComponent*>(pairs[i].triggerActor->userData);
		UPrimitiveComponent* OtherComp = static_cast<UPrimitiveComponent*>(pairs[i].otherActor->userData);

		if (pairs[i].status == PxPairFlag::eNOTIFY_TOUCH_FOUND)
		{
			if (TriggerComp) TriggerComp->OnComponentBeginOverlap(OtherComp);
		}
		else if (pairs[i].status == PxPairFlag::eNOTIFY_TOUCH_LOST)
		{
			if (TriggerComp) TriggerComp->OnComponentEndOverlap(OtherComp);
		}
	
	}
}
