#pragma once
#include "PxPhysicsAPI.h"

class GameObject;
class UPrimitiveComponent;
class UBodySetup;
struct FShape;
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

    // Extended API
    void CreateShapesFromBodySetup();
    void AddSimpleShape(const FShape& S);
    void SetMaterial(PxMaterial* InMaterial) { MaterialOverride = InMaterial; }

public:
    UBodySetup* BodySetup = nullptr;

private:
    UPrimitiveComponent* OwnerComponent;
    PxRigidActor* PhysicsActor; // Dynamic or Static
    PxMaterial* MaterialOverride = nullptr;

    TArray<PxShape*> Shapes;
};
