#include "pch.h"
#include "PrimitiveComponent.h"
#include "FBodyInstance.h"
#include "BodySetup.h"
#include "Source/Runtime/Engine/Components/ShapeComponent.h"
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

        // 디버그: Scene에 추가된 총 액터 수 + 소유 액터 이름
        PxU32 numActors = Scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
        bool bIsDynamic = PhysicsActor->is<PxRigidDynamic>() != nullptr;
        PxTransform T = PhysicsActor->getGlobalPose();

        const char* compName = OwnerComponent ? OwnerComponent->GetName().c_str() : "null";
        const char* actorName = (OwnerComponent && OwnerComponent->GetOwner())
            ? OwnerComponent->GetOwner()->GetName().c_str() : "no_actor";

        UE_LOG("[Physics] AddToScene: %s, Total:%u, Comp=%s, Actor=%s, Pos=(%.1f,%.1f,%.1f)",
            bIsDynamic ? "Dynamic" : "Static", numActors,
            compName, actorName, T.p.x, T.p.y, T.p.z);
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

void FBodyInstance::CreateShapesFromBodySetup()
{
    if (!PhysicsActor || !BodySetup) return;

    PxMaterial* Mat = MaterialOverride ? MaterialOverride : PHYSICS.GetDefaultMaterial();
    if (!Mat) return;

    PxPhysics* Physics = PHYSICS.GetPhysics();
    if (!Physics) return;

    // Apply component world scale to geometry sizes and local offsets
    const FVector WS = OwnerComponent ? OwnerComponent->GetWorldScale() : FVector(1,1,1);
    const FVector AbsS(FMath::Abs(WS.X), FMath::Abs(WS.Y), FMath::Abs(WS.Z));

    // Spheres
    for (const FKSphereElem& S : BodySetup->AggGeom.SphereElems)
    {
        const float RadiusWS = S.Radius * (AbsS.X + AbsS.Y + AbsS.Z) / 3.0f; // uniform approx
        PxSphereGeometry Geom(PxMax(RadiusWS, 0.01f));
        const PxVec3 C(S.Center.X * WS.X, S.Center.Y * WS.Y, S.Center.Z * WS.Z);
        PxTransform LocalPose(C);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (!Shape) continue;
        Shape->setLocalPose(LocalPose);
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
    }

    // Boxes
    for (const FKBoxElem& B : BodySetup->AggGeom.BoxElems)
    {
        const PxVec3 HE(B.X * AbsS.X, B.Y * AbsS.Y, B.Z * AbsS.Z);
        PxBoxGeometry Geom(PxMax(HE.x, 0.01f), PxMax(HE.y, 0.01f), PxMax(HE.z, 0.01f));
        const FQuat R = FQuat::MakeFromEulerZYX(B.Rotation);
        PxQuat Q(R.X, R.Y, R.Z, R.W);
        const PxVec3 C(B.Center.X * WS.X, B.Center.Y * WS.Y, B.Center.Z * WS.Z);
        PxTransform LocalPose(C, Q);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (!Shape) continue;
        Shape->setLocalPose(LocalPose);
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
    }

    // Capsules (Sphyls)
    for (const FKCapsuleElem& C : BodySetup->AggGeom.SphylElems)
    {
        if (C.Radius <= 0.0f || C.Length <= 0.0f) continue; // PhysX requires positive dimensions
        // PxCapsuleGeometry: radius and halfHeight (half of the cylindrical part length)
        // Use average scale for radius, and appropriate axis scale for length
        const float AvgScale = (AbsS.X + AbsS.Y + AbsS.Z) / 3.0f;
        const float RadiusWS = C.Radius * AvgScale;
        const float HalfWS = (C.Length * 0.5f) * AvgScale;
        PxCapsuleGeometry Geom(PxMax(RadiusWS, 0.01f), PxMax(HalfWS, 0.01f));
        // PhysX capsules are X-axis aligned; apply user rotation
        const FQuat R = FQuat::MakeFromEulerZYX(C.Rotation);
        PxQuat Q(R.X, R.Y, R.Z, R.W);
        const PxVec3 Ctr(C.Center.X * WS.X, C.Center.Y * WS.Y, C.Center.Z * WS.Z);
        PxTransform LocalPose(Ctr, Q);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (!Shape) continue;
        Shape->setLocalPose(LocalPose);
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
    }

    // Cooked convex mesh (works for both static and dynamic actors)
    if (BodySetup->ConvexMesh)
    {
        // Apply uniform scale to convex mesh
        const float UniformScale = (AbsS.X + AbsS.Y + AbsS.Z) / 3.0f;
        PxMeshScale MeshScale(PxVec3(AbsS.X, AbsS.Y, AbsS.Z), PxQuat(PxIdentity));
        PxConvexMeshGeometry Geom(BodySetup->ConvexMesh, MeshScale);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (Shape)
        {
            PhysicsActor->attachShape(*Shape);
            Shapes.Add(Shape);
            UE_LOG("[Physics] Attached convex mesh shape");
        }
    }

    // Cooked triangle mesh (static actors only - PhysX limitation!)
    if (BodySetup->TriMesh)
    {
        // Triangle meshes can only be attached to static actors
        if (PhysicsActor->is<PxRigidStatic>())
        {
            PxMeshScale MeshScale(PxVec3(AbsS.X, AbsS.Y, AbsS.Z), PxQuat(PxIdentity));
            PxTriangleMeshGeometry Geom(BodySetup->TriMesh, MeshScale);
            PxShape* Shape = Physics->createShape(Geom, *Mat, true);
            if (Shape)
            {
                PhysicsActor->attachShape(*Shape);
                Shapes.Add(Shape);
                UE_LOG("[Physics] Attached triangle mesh shape");
            }
        }
        else
        {
            UE_LOG("[Physics] WARNING: Triangle mesh collision requested but actor is dynamic! "
                   "Triangle meshes only work with static actors. Use convex mesh instead.");
        }
    }

	// If dynamic, compute mass/inertia now that shapes are attached
	if (PhysicsActor && PhysicsActor->is<PxRigidDynamic>())
	{
		PxRigidDynamic* Dyn = PhysicsActor->is<PxRigidDynamic>();
		PxRigidBodyExt::updateMassAndInertia(*Dyn, 1000.0f);
		Dyn->wakeUp();
	}
}

void FBodyInstance::AddSimpleShape(const FShape& S)
{
    if (!PhysicsActor) return;
    PxMaterial* Mat = MaterialOverride ? MaterialOverride : PHYSICS.GetDefaultMaterial();
    if (!Mat) return;

    PxPhysics* Physics = PHYSICS.GetPhysics();
    if (!Physics) return;

    switch (S.Kind)
    {
    case EShapeKind::Sphere:
    {
        const float Radius = S.Sphere.SphereRadius;
        if (Radius <= 0.0f) break; // PhysX requires positive radius
        PxSphereGeometry Geom(Radius);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (!Shape) break;
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
        break;
    }
    case EShapeKind::Box:
    {
        const FVector& E = S.Box.BoxExtent; // half extents
        if (E.X <= 0.0f || E.Y <= 0.0f || E.Z <= 0.0f) break; // PhysX requires positive extents
        PxBoxGeometry Geom(E.X, E.Y, E.Z);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (!Shape) break;
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
        break;
    }
    case EShapeKind::Capsule:
    {
        const float Radius = S.Capsule.CapsuleRadius;
        const float Half = S.Capsule.CapsuleHalfHeight;
        if (Radius <= 0.0f || Half <= 0.0f) break; // PhysX requires positive dimensions
        PxCapsuleGeometry Geom(Radius, Half);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (!Shape) break;
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
        break;
    }
    default:
        break;
    }

	// If dynamic, recompute mass/inertia and wake
	if (PhysicsActor && PhysicsActor->is<PxRigidDynamic>())
	{
		PxRigidDynamic* Dyn = PhysicsActor->is<PxRigidDynamic>();
		PxRigidBodyExt::updateMassAndInertia(*Dyn, 1000.0f);
		Dyn->wakeUp();
	}
}

bool FBodyInstance::IsDynamic() const
{
    return PhysicsActor && PhysicsActor->is<PxRigidDynamic>();
}

void FBodyInstance::TermBody()
{
	// Detach & release created shapes first
	for (PxShape* Shape : Shapes)
	{
		if (Shape)
		{
			if (PhysicsActor)
			{
				PhysicsActor->detachShape(*Shape);
			}
			Shape->release();
		}
	}
	Shapes.clear();

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
