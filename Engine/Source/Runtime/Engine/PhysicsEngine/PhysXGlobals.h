 #pragma once

 #include "PxPhysicsAPI.h"

 namespace PhysXGlobals
 {
     // Initialize PhysX SDK and create a default scene and material.
     // If bEnablePvd is true, connects to PhysX Visual Debugger (localhost:5425).
     bool InitializePhysX(bool bEnablePvd = false);

     // Release PhysX SDK objects created in InitializePhysX.
     void ShutdownPhysX();
 }

