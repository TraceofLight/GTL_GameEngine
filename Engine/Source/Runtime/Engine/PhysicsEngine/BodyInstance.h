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

    // 복사 시 PhysicsActor는 복사하지 않음 (dangling pointer 방지)
    FBodyInstance(const FBodyInstance& Other)
        : OwnerComponent(Other.OwnerComponent)
        , PhysicsActor(nullptr)  // 물리 액터는 복사하지 않고 nullptr로 초기화
    {
    }

    FBodyInstance& operator=(const FBodyInstance& Other)
    {
        if (this != &Other)
        {
            TermBody();  // 기존 물리 액터 정리
            OwnerComponent = Other.OwnerComponent;
            PhysicsActor = nullptr;  // 물리 액터는 복사하지 않음
        }
        return *this;
    }

	void CreateActor(PxPhysics* Physics, const FMatrix WorldMat, bool bIsDynamic);
	void AddToScene(PxScene* Scene);

    void SyncPhysicsToComponent();
    void SetBodyTransform(const FMatrix& NewMatrix);
    bool IsDynamic() const;
    FTransform GetWorldTransform() const;

    // Attach a simple box shape to the body (AABB-based). Half extents in meters.
    void AttachBoxShape(PxPhysics* Physics, PxMaterial* Material, const PxVec3& halfExtents, const PxVec3& localOffset = PxVec3(0,0,0));

    void TermBody();

	bool IsValid() const { return PhysicsActor != nullptr; }
	PxRigidActor* GetPhysicsActor() const { return PhysicsActor; }

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
