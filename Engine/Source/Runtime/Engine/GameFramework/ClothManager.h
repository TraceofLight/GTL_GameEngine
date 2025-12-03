#pragma once
 
#include "NvCloth/Factory.h"
#include "NvCloth/Fabric.h"
#include "NvCloth/Cloth.h"
#include "NvCloth/Solver.h"
#include "NvCloth/Callbacks.h"
#include "NvClothExt/ClothFabricCooker.h"
#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxErrorCallback.h"
  

class FClothManager
{
public:
	static FClothManager& GetInstance()
	{
		static FClothManager Instance;
		return Instance;
	}

	void Initialize();
	void Shutdown();

	void CreateSolver();
	void CreateFactory();
	void ClothSimulation(float DeltaSeconds);

	void AddClothToSolver(nv::cloth::Cloth* Cloth); 

	nv::cloth::Factory* GetFactory() { return factory;} 
	nv::cloth::Solver* GetSolver() { return solver;}  
protected:

	nv::cloth::Factory* factory;
	nv::cloth::Solver* solver;
};
