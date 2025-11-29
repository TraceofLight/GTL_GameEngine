#include "pch.h"
#include "PrimitiveComponent.h"
#include "FBodyInstance.h"
// For mass/inertia helpers
#include "extensions/PxRigidBodyExt.h"

FBodyInstance::~FBodyInstance()
{
    TermBody();
}

void FBodyInstance::CreateActor(PxPhysics* Physics, const FMatrix WorldMat, bool bIsDynamic)
{
    if (PhysicsActor || !Physics) return;

    // Extract position/rotation (ignore scale)
    FTransform WT(WorldMat);
    PxVec3 P(WT.Translation.X, WT.Translation.Y, WT.Translation.Z);
    PxQuat Q(WT.Rotation.X, WT.Rotation.Y, WT.Rotation.Z, WT.Rotation.W);
    if (!Q.isFinite() || Q.magnitudeSquared() < 1e-6f)
    {
        Q = PxQuat(PxIdentity);
    }
    else
    {
        Q.normalize();
    }

    PxTransform Trans(P, Q);

    PhysicsActor = bIsDynamic
        ? static_cast<PxRigidActor*>(Physics->createRigidDynamic(Trans))
        : static_cast<PxRigidActor*>(Physics->createRigidStatic(Trans));

    if (!PhysicsActor)
        return;

    PhysicsActor->userData = OwnerComponent;
}

void FBodyInstance::AddToScene(PxScene* Scene)
{
    if (PhysicsActor && Scene)
    {
        Scene->addActor(*PhysicsActor);
    }
}

void FBodyInstance::SyncPhysicsToComponent()
{
    if (!PhysicsActor || !OwnerComponent) return;

    if (PhysicsActor->is<PxRigidDynamic>())
    {
        PxTransform T = PhysicsActor->getGlobalPose();
        const FVector Scale = OwnerComponent->GetWorldScale();
        FTransform NewTransform(
            FVector(T.p.x, T.p.y, T.p.z),
            FQuat(T.q.x, T.q.y, T.q.z, T.q.w),
            Scale);
        OwnerComponent->UpdateWorldMatrixFromPhysics(NewTransform.ToMatrix());
    }
}

void FBodyInstance::SetBodyTransform(const FMatrix& NewMatrix)
{
    if (!PhysicsActor) return;

    // Use only pos/rot for PhysX
    FTransform WT(NewMatrix);
    PxTransform Trans(
        PxVec3(WT.Translation.X, WT.Translation.Y, WT.Translation.Z),
        PxQuat(WT.Rotation.X, WT.Rotation.Y, WT.Rotation.Z, WT.Rotation.W));
    PhysicsActor->setGlobalPose(Trans);
}

bool FBodyInstance::IsDynamic() const
{
    return PhysicsActor && PhysicsActor->is<PxRigidDynamic>();
}

void FBodyInstance::TermBody()
{
    if (PhysicsActor)
    {
        if (PxScene* Scene = PhysicsActor->getScene())
        {
            Scene->removeActor(*PhysicsActor);
        }

        PhysicsActor->release();
        PhysicsActor = nullptr;
    }
}

void FBodyInstance::AttachBoxShape(PxPhysics* Physics, PxMaterial* Material, const PxVec3& halfExtents, const PxVec3& localOffset)
{
    if (!PhysicsActor || !Physics || !Material)
        return;

    PxVec3 he(
        PxMax(halfExtents.x, 0.01f),
        PxMax(halfExtents.y, 0.01f),
        PxMax(halfExtents.z, 0.01f)
    );

    PxShape* shape = Physics->createShape(PxBoxGeometry(he), *Material);
    if (!shape)
        return;

    if (localOffset != PxVec3(0,0,0))
    {
        shape->setLocalPose(PxTransform(localOffset));
    }

    PhysicsActor->attachShape(*shape);
    shape->release(); // refcount held by actor

    if (auto* dyn = PhysicsActor->is<PxRigidDynamic>())
    {
        PxRigidBodyExt::updateMassAndInertia(*dyn, 1.0f);
        dyn->wakeUp();
    }
}

