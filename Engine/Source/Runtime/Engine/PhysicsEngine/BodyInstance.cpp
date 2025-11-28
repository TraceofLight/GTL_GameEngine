#include "pch.h"
#include "PrimitiveComponent.h"
#include "BodyInstance.h"
#include "BodySetup.h"
#include "Source/Runtime/Engine/Components/ShapeComponent.h"
#include "PhysXGlobals.h"

FBodyInstance::~FBodyInstance()
{
	TermBody();
}

void FBodyInstance::InitBody(PxPhysics* Physics, PxScene* Scene, const FMatrix WorldMat, bool bIsDynamic)
{
	FMatrix FloatMat;

	memcpy(&FloatMat, &WorldMat, sizeof(float) * 16);

	PxMat44 PMat((float*)&FloatMat);
	PxTransform Trans(PMat);

	if (bIsDynamic)
	{
		PhysicsActor = Physics->createRigidDynamic(Trans);
	}
	else
	{
		PhysicsActor = Physics->createRigidStatic(Trans);
	}

	PhysicsActor->userData = OwnerComponent;
	Scene->addActor(*PhysicsActor);
}

void FBodyInstance::SyncPhysicsToComponent()
{
	if (!PhysicsActor || !OwnerComponent) return;

	// Dynamic일 때만 업데이트
	if (PhysicsActor->is<PxRigidDynamic>())
	{
		// PhysX 위치 가져오기
		PxTransform T = PhysicsActor->getGlobalPose();
		PxMat44 Mat(T);

		// PhysX Matrix를 FMatrix로 변환
		FMatrix NewWorldMatrix;
		memcpy(&NewWorldMatrix, &Mat, sizeof(float) * 16);

		// 컴포넌트에 반영
		OwnerComponent->UpdateWorldMatrixFromPhysics(NewWorldMatrix);
	}
}

void FBodyInstance::SetBodyTransform(const FMatrix& NewMatrix)
{
	if (!PhysicsActor) return;

	FMatrix FloatMat;
	memcpy(&FloatMat, &NewMatrix, sizeof(float) * 16);
	PxMat44 PMat((float*)&FloatMat);
	PxTransform Trans(PMat);

	PhysicsActor->setGlobalPose(Trans);
}

void FBodyInstance::CreateShapesFromBodySetup()
{
    if (!PhysicsActor || !BodySetup) return;

    PxMaterial* Mat = MaterialOverride ? MaterialOverride : gMaterial;
    if (!Mat) return;

    // Spheres
    for (const FKSphereElem& S : BodySetup->AggGeom.SphereElems)
    {
        PxSphereGeometry Geom(S.Radius);
        PxTransform LocalPose(PxVec3(S.Center.X, S.Center.Y, S.Center.Z));
        PxShape* Shape = gPhysics->createShape(Geom, *Mat, true);
        if (!Shape) continue;
        Shape->setLocalPose(LocalPose);
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
    }

    // Boxes
    for (const FKBoxElem& B : BodySetup->AggGeom.BoxElems)
    {
        PxBoxGeometry Geom(B.X, B.Y, B.Z);
        const FQuat R = FQuat::MakeFromEulerZYX(B.Rotation);
        PxQuat Q(R.X, R.Y, R.Z, R.W);
        PxTransform LocalPose(PxVec3(B.Center.X, B.Center.Y, B.Center.Z), Q);
        PxShape* Shape = gPhysics->createShape(Geom, *Mat, true);
        if (!Shape) continue;
        Shape->setLocalPose(LocalPose);
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
    }
}

void FBodyInstance::AddSimpleShape(const FShape& S)
{
    if (!PhysicsActor) return;
    PxMaterial* Mat = MaterialOverride ? MaterialOverride : gMaterial;
    if (!Mat) return;

    switch (S.Kind)
    {
    case EShapeKind::Sphere:
    {
        PxSphereGeometry Geom(S.Sphere.SphereRadius);
        PxShape* Shape = gPhysics->createShape(Geom, *Mat, true);
        if (!Shape) break;
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
        break;
    }
    case EShapeKind::Box:
    {
        const FVector& E = S.Box.BoxExtent; // half extents
        PxBoxGeometry Geom(E.X, E.Y, E.Z);
        PxShape* Shape = gPhysics->createShape(Geom, *Mat, true);
        if (!Shape) break;
        PhysicsActor->attachShape(*Shape);
        Shapes.Add(Shape);
        break;
    }
    case EShapeKind::Capsule:
    default:
        // TODO: Implement capsule when needed
        break;
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
