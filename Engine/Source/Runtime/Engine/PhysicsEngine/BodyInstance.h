#pragma once
#include "PxPhysicsAPI.h"

class GameObject;
class UPrimitiveComponent;
using namespace physx;

struct FBodyInstance
{
public:
	FBodyInstance(UPrimitiveComponent* InOwner) : OwnerComponent(InOwner), PhysicsActor(nullptr) {}
	~FBodyInstance();

	void InitBody(PxPhysics* Physics, PxScene* Scene, const FMatrix WorldMat, bool bIsDynamic);

	void SyncPhysicsToComponent();

	void SetBodyTransform(const FMatrix& NewMatrix);

	bool IsDynamic() const; 

	void TermBody();

private:
	UPrimitiveComponent* OwnerComponent;
	PxRigidActor* PhysicsActor; // Dynamic과 Static의 부모 클래스

};
