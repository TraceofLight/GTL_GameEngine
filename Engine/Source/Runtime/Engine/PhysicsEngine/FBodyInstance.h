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

	void CreateActor(PxPhysics* Physics, const FMatrix WorldMat, bool bIsDynamic);
	void AddToScene(PxScene* Scene);

    void SyncPhysicsToComponent();
    void SetBodyTransform(const FMatrix& NewMatrix);
    bool IsDynamic() const; 

    // Attach a simple box shape to the body (AABB-based). Half extents in meters.
    void AttachBoxShape(PxPhysics* Physics, PxMaterial* Material, const PxVec3& halfExtents, const PxVec3& localOffset = PxVec3(0,0,0));

    void TermBody();

	bool IsValid() const { return PhysicsActor != nullptr; }

    // Extended API
    void CreateShapesFromBodySetup();
    void AddSimpleShape(const FShape& S);
    void SetMaterial(PxMaterial* InMaterial) { MaterialOverride = InMaterial; }

public:
    UBodySetup* BodySetup = nullptr;

private:
    UPrimitiveComponent* OwnerComponent;
    PxRigidActor* PhysicsActor; // Dynamic과 Static의 부모 클래스
    PxMaterial* MaterialOverride = nullptr;
	
    TArray<PxShape*> Shapes;
};
