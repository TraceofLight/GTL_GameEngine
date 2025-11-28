#include "pch.h"
#include "PrimitiveComponent.h"
#include "BodyInstance.h"

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
