#include "pch.h"
#include "ClothManager.h"

using namespace physx;
using namespace nv::cloth;

// 전역 상수
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TWO_PI (2.0f * M_PI)

// 익명 namespace로 정의해서 리플렉션 시스템 회피
namespace
{
	/**
	 * @brief NvCloth용 Allocator
	 */
	class NvClothAllocator : public physx::PxAllocatorCallback
	{
	public:
		void* allocate(size_t size, const char* typeName, const char* filename, int line) override
		{
			return _aligned_malloc(size, 16);
		}

		void deallocate(void* ptr) override
		{
			_aligned_free(ptr);
		}
	};

	/**
	 * @brief NvCloth용 ErrorCallback
	 */
	class NvClothErrorCallback : public physx::PxErrorCallback
	{
	public:
		void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override
		{
			// 에러 코드 문자열로 변환
			const char* errorCodeStr = "Unknown";
			switch (code)
			{
			case physx::PxErrorCode::eNO_ERROR:          errorCodeStr = "NoError"; break;
			case physx::PxErrorCode::eDEBUG_INFO:        errorCodeStr = "Info"; break;
			case physx::PxErrorCode::eDEBUG_WARNING:     errorCodeStr = "Warning"; break;
			case physx::PxErrorCode::eINVALID_PARAMETER: errorCodeStr = "InvalidParam"; break;
			case physx::PxErrorCode::eINVALID_OPERATION: errorCodeStr = "InvalidOp"; break;
			case physx::PxErrorCode::eOUT_OF_MEMORY:     errorCodeStr = "OutOfMemory"; break;
			case physx::PxErrorCode::eINTERNAL_ERROR:    errorCodeStr = "InternalError"; break;
			case physx::PxErrorCode::eABORT:             errorCodeStr = "Abort"; break;
			case physx::PxErrorCode::ePERF_WARNING:      errorCodeStr = "PerfWarning"; break;
			}

			UE_LOG("NvCloth: %s: %s (%s:%d)", errorCodeStr, message, file, line);
		}
	};

	/**
	 * @brief NvCloth용 AssertHandler
	 */
	class NvClothAssertHandler : public nv::cloth::PxAssertHandler
	{
	public:
		void operator()(const char* exp, const char* file, int line, bool& ignore) override
		{
			// UE_LOG("[NvCloth Assert] %s (%s:%d)\n", exp, file, line);
		}
	};

	// NvCloth 전역 콜백 인스턴스
	NvClothAllocator g_ClothAllocator;
	NvClothErrorCallback g_ClothErrorCallback;
	NvClothAssertHandler g_ClothAssertHandler;
	bool g_bNvClothInitialized = false;
}

void FClothManager::Initialize()
{
	nv::cloth::InitializeNvCloth(&g_ClothAllocator, &g_ClothErrorCallback, &g_ClothAssertHandler, nullptr);

	CreateFactory();
	CreateSolver();
}

void FClothManager::Shutdown()
{
	// 4. Solver 삭제
	if (solver)
	{
		UE_LOG("ClothManager: Shutdown: Deleting solver");
		NV_CLOTH_DELETE(solver);
		solver = nullptr;
	}


	// 6. Factory 해제
	if (factory)
	{
		UE_LOG("ClothManager: Shutdown: Destroying factory");
		NvClothDestroyFactory(factory);
		factory = nullptr;

	}

}

void FClothManager::CreateSolver()
{
	if (!factory)
		return;

	solver = factory->createSolver();

}

void FClothManager::CreateFactory()
{
	factory = NvClothCreateFactoryCPU();
	if (factory == nullptr)
	{
		UE_LOG("ClothManager: CreateFactory: Failed");
	}
}

void FClothManager::ClothSimulation(float DeltaSeconds)
{
	solver->beginSimulation(DeltaSeconds);

	for (int i = 0; i < solver->getSimulationChunkCount(); ++i)
	{
		// multi thread로 병렬화 가능
		solver->simulateChunk(i);
	}

	solver->endSimulation();
}

void FClothManager::AddClothToSolver(nv::cloth::Cloth* Cloth)
{
	if (solver)
	{
		solver->addCloth(Cloth);
	}
}
