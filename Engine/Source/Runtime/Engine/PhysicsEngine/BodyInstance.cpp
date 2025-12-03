#include "pch.h"
#include "PrimitiveComponent.h"
#include "BodyInstance.h"
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
    if (!PhysicsActor || !BodySetup)
        return;

    PxMaterial* Mat = MaterialOverride ? MaterialOverride : PHYSICS.GetDefaultMaterial();
    if (!Mat)
        return;

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

    // Cooked convex mesh
    if (BodySetup->ConvexMesh)
    {
        PxMeshScale MeshScale(PxVec3(AbsS.X, AbsS.Y, AbsS.Z), PxQuat(PxIdentity));
        PxConvexMeshGeometry Geom(BodySetup->ConvexMesh, MeshScale);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (Shape)
        {
            PhysicsActor->attachShape(*Shape);
            Shapes.Add(Shape);
        }
    }

    // Cooked triangle mesh (static actors only)
    if (BodySetup->TriMesh && PhysicsActor->is<PxRigidStatic>())
    {
        PxMeshScale MeshScale(PxVec3(AbsS.X, AbsS.Y, AbsS.Z), PxQuat(PxIdentity));
        PxTriangleMeshGeometry Geom(BodySetup->TriMesh, MeshScale);
        PxShape* Shape = Physics->createShape(Geom, *Mat, true);
        if (Shape)
        {
            PhysicsActor->attachShape(*Shape);
            Shapes.Add(Shape);
        }
    }

    // If dynamic, compute mass/inertia and set damping for stability
    if (PhysicsActor && PhysicsActor->is<PxRigidDynamic>())
    {
        PxRigidDynamic* Dyn = PhysicsActor->is<PxRigidDynamic>();

        // 밀도 기반 질량 계산
        PxRigidBodyExt::updateMassAndInertia(*Dyn, 10.0f);

        // 최소 질량 보장 (작은 스케일 모델에서도 적절한 물리 동작을 위해)
        constexpr float MinMass = 1.0f;  // kg
        float CurrentMass = Dyn->getMass();
        if (CurrentMass < MinMass)
        {
            // 질량을 최소값으로 설정하고 관성도 비례하여 스케일
            float MassScale = MinMass / std::max(CurrentMass, 0.0001f);
            PxVec3 CurrentInertia = Dyn->getMassSpaceInertiaTensor();
            Dyn->setMass(MinMass);
            Dyn->setMassSpaceInertiaTensor(CurrentInertia * MassScale);
        }

        // Damping 설정 - 흔들림 감소
        Dyn->setLinearDamping(0.5f);
        Dyn->setAngularDamping(0.5f);

        // Solver Iteration 증가 - Joint 안정성 향상
        Dyn->setSolverIterationCounts(8, 4);  // positionIters, velocityIters

        Dyn->wakeUp();
    }

    // 충돌 필터 설정
    // Collision Groups:
    //   0x00000001 = CHASSIS (차량 본체)
    //   0x00000002 = GROUND (지면/정적 객체)
    //   0x00000004 = DYNAMIC (동적 물리 객체)
    //   0x00001000 = WHEEL (차량 휠)
    if (PhysicsActor)
    {
        const bool bIsStatic = PhysicsActor->is<PxRigidStatic>();

        // 1. Scene Query Filter (Vehicle 서스펜션 레이캐스트용)
        // Static Actor: DRIVABLE_SURFACE (word3 = 1)
        // Dynamic Actor: UNDRIVABLE_SURFACE (word3 = 0)
        PxFilterData QueryFilterData;
        QueryFilterData.word0 = 0;
        QueryFilterData.word1 = 0;
        QueryFilterData.word2 = 0;
        QueryFilterData.word3 = bIsStatic ? 1 : 0;

        // 2. Simulation Filter (물리 충돌용)
        PxFilterData SimFilterData;
        if (bIsStatic)
        {
            // 지면: GROUND 그룹, 모든 것과 충돌 시도
            SimFilterData.word0 = ECollisionGroup::Ground;
            SimFilterData.word1 = ECollisionGroup::AllMask;
        }
        else
        {
            // 동적 객체: DYNAMIC 그룹, 지면 및 다른 동적 객체와 충돌
            SimFilterData.word0 = ECollisionGroup::Dynamic;
            SimFilterData.word1 = ECollisionGroup::GroundAndDynamic;
        }
        SimFilterData.word2 = 0;
        SimFilterData.word3 = 0;

        for (PxShape* Shape : Shapes)
        {
            if (Shape)
            {
                Shape->setQueryFilterData(QueryFilterData);
                Shape->setSimulationFilterData(SimFilterData);
            }
        }
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
		PxRigidBodyExt::updateMassAndInertia(*Dyn, 10.0f);

		// 최소 질량 보장
		constexpr float MinMass = 1.0f;
		float CurrentMass = Dyn->getMass();
		if (CurrentMass < MinMass)
		{
			float MassScale = MinMass / std::max(CurrentMass, 0.0001f);
			PxVec3 CurrentInertia = Dyn->getMassSpaceInertiaTensor();
			Dyn->setMass(MinMass);
			Dyn->setMassSpaceInertiaTensor(CurrentInertia * MassScale);
		}

		Dyn->setLinearDamping(0.5f);
		Dyn->setAngularDamping(0.5f);
		Dyn->setSolverIterationCounts(8, 4);
		Dyn->wakeUp();
	}
}

bool FBodyInstance::IsDynamic() const
{
    return PhysicsActor && PhysicsActor->is<PxRigidDynamic>();
}

FTransform FBodyInstance::GetWorldTransform() const
{
    if (!PhysicsActor)
    {
        return FTransform();
    }

    PxTransform T = PhysicsActor->getGlobalPose();
    return FTransform(
        FVector(T.p.x, T.p.y, T.p.z),
        FQuat(T.q.x, T.q.y, T.q.z, T.q.w),
        FVector(1.0f, 1.0f, 1.0f)  // Scale은 physics에서 관리 안 함
    );
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
        PxRigidBodyExt::updateMassAndInertia(*dyn, 10.0f);

        // 최소 질량 보장
        constexpr float MinMass = 1.0f;
        float CurrentMass = dyn->getMass();
        if (CurrentMass < MinMass)
        {
            float MassScale = MinMass / std::max(CurrentMass, 0.0001f);
            PxVec3 CurrentInertia = dyn->getMassSpaceInertiaTensor();
            dyn->setMass(MinMass);
            dyn->setMassSpaceInertiaTensor(CurrentInertia * MassScale);
        }

        dyn->setLinearDamping(0.5f);
        dyn->setAngularDamping(0.5f);
        dyn->setSolverIterationCounts(8, 4);
        dyn->wakeUp();
    }
}
