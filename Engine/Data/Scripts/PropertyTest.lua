-- PropertyTest.lua
-- 새로운 LuaBindHelpers 바인딩 시스템 테스트
-- 포인터와 배열 프로퍼티 바인딩 검증

local TestStarted = false

function BeginPlay()
    print("========================================")
    print("=== Property Binding System Test ===")
    print("========================================")

    TestStarted = true

    -- ===== 1. 기본 GameObject 프로퍼티 =====
   


    -- ===== 2. AudioComponent - TArray<USound*> 테스트 =====
    print("\n[Test 2] AudioComponent - Array Property:")
    local audio = GetComponent(Obj, "UAudioComponent")
    if audio then
        print("  >> AudioComponent found!")
		audio.Volume=0.1
       -- print("  Volume:", audio.Volume)
      --  print("  Pitch:", audio.Pitch)
       -- print("  bIsLooping:", audio.bIsLooping)
       -- print("  bAutoPlay:", audio.bAutoPlay)

        -- TArray<USound*> 접근
        print("\n  >> Testing TArray<USound*> Sounds:")
        local sounds = audio.Sounds
		    print("\n  >> Checking TArray<USound*> Sounds:")
        if sounds then
            print("    Type:", type(sounds))
            print("    Array size:", #sounds)

            if #sounds > 0 then
                for i = 1, #sounds do
                    print("    Sounds[" .. i .. "]:", sounds[i])
                end
            else
                print("    (Array is empty)")
            end
        else
            print("    (Sounds property is nil)")
        end

        -- 프로퍼티 수정 테스트
        audio.Volume = 0.8
        audio.Pitch = 1.3
        audio.bIsLooping = true
        print("\n  >> After modification:")
        print("    Volume:", audio.Volume)
        print("    Pitch:", audio.Pitch)
        print("    bIsLooping:", audio.bIsLooping)

        -- 함수 호출 테스트
        audio:Play()
        print("  >> Audio:Play() called!")
    else
        print("  >> No AudioComponent found on this actor")
    end


    -- ===== 3. StaticMeshComponent - 포인터 프로퍼티 테스트 =====
    print("\n[Test 3] StaticMeshComponent - Pointer Property:")
    local mesh = GetComponent(Obj, "UStaticMeshComponent")
    if mesh then
        print("  >> StaticMeshComponent found!")
        print("  StaticMesh (UStaticMesh*):", mesh.StaticMesh)

        if mesh.StaticMesh ~= nil then
            print("    >> StaticMesh pointer is valid!")
        else
            print("    >> StaticMesh is nil")
        end
    else
        print("  >> No StaticMeshComponent found")
    end


    -- ===== 4. RotatingMovementComponent - Vector 프로퍼티 =====
    print("\n[Test 4] RotatingMovementComponent - Vector Property:")
    local rotating = GetComponent(Obj, "URotatingMovementComponent")
    if rotating then
        print("  >> RotatingMovementComponent found!")
        print("  RotationRate:", rotating.RotationRate.X, rotating.RotationRate.Y, rotating.RotationRate.Z)
        print("  bRotationInLocalSpace:", rotating.bRotationInLocalSpace)

        -- Vector 수정
        rotating.RotationRate = Vector(0, 45, 0)
        rotating.bRotationInLocalSpace = true
        print("  >> After modification:")
        print("    RotationRate:", rotating.RotationRate.X, rotating.RotationRate.Y, rotating.RotationRate.Z)
        print("    bRotationInLocalSpace:", rotating.bRotationInLocalSpace)
    else
        print("  >> No RotatingMovementComponent found")
    end


    -- ===== 5. PointLightComponent - Color & Range 프로퍼티 =====
    print("\n[Test 5] PointLightComponent - Color Property:")
    local light = GetComponent(Obj, "UPointLightComponent")
    if light then
        print("  >> PointLightComponent found!")
        print("  Intensity:", light.Intensity)
        print("  LightColor (R,G,B,A):", light.LightColor.R, light.LightColor.G, light.LightColor.B, light.LightColor.A)
        print("  AttenuationRadius:", light.AttenuationRadius)
        print("  bCastShadows:", light.bCastShadows)

        -- 프로퍼티 수정
        light.Intensity = 800.0
        light.AttenuationRadius = 1000.0
        light.bCastShadows = true
        print("  >> After modification:")
        print("    Intensity:", light.Intensity)
        print("    AttenuationRadius:", light.AttenuationRadius)
    else
        print("  >> No PointLightComponent found")
    end


    -- ===== 6. CameraComponent - Range 프로퍼티 =====
    print("\n[Test 6] CameraComponent - Range Properties:")
    local camera = GetComponent(Obj, "UCameraComponent")
    if camera then
        print("  >> CameraComponent found!")
        print("  FieldOfView:", camera.FieldOfView)
        print("  AspectRatio:", camera.AspectRatio)
        print("  NearClip:", camera.NearClip)
        print("  FarClip:", camera.FarClip)

        -- 프로퍼티 수정
        camera.FieldOfView = 80.0
        camera.FarClip = 8000.0
        print("  >> After modification:")
        print("    FieldOfView:", camera.FieldOfView)
        print("    FarClip:", camera.FarClip)
    else
        print("  >> No CameraComponent found")
    end


    -- ===== 7. ProjectileMovementComponent - 복합 테스트 =====
    print("\n[Test 7] ProjectileMovementComponent:")
    local projectile = GetComponent(Obj, "UProjectileMovementComponent")
    if projectile then
        print("  >> ProjectileMovementComponent found!")
        print("  InitialSpeed:", projectile.InitialSpeed)
        print("  MaxSpeed:", projectile.MaxSpeed)
       -- print("  Gravity:", projectile.Gravity.X, projectile.Gravity.Y, projectile.Gravity.Z)
        print("  bIsHomingProjectile:", projectile.bIsHomingProjectile)

        -- 프로퍼티 수정
        projectile.InitialSpeed = 1500.0
        projectile.MaxSpeed = 2500.0
        projectile.Gravity = Vector(0, 0, -980)
        projectile.bIsHomingProjectile = true
        print("  >> After modification:")
        print("    InitialSpeed:", projectile.InitialSpeed)
        print("    MaxSpeed:", projectile.MaxSpeed)
     --   print("    Gravity:", projectile.Gravity.X, projectile.Gravity.Y, projectile.Gravity.Z)
        print("    bIsHomingProjectile:", projectile.bIsHomingProjectile)
    else
        print("  >> No ProjectileMovementComponent found")
    end


    -- ===== 8. TestAutoBindComponent - 자동 바인딩 테스트 =====
    print("\n[Test 8] TestAutoBindComponent - Auto Binding:")
    local testComp = GetComponent(Obj, "UTestAutoBindComponent")
    if testComp then
        print("  >> UTestAutoBindComponent found!")
        print("  Intensity:", testComp.Intensity)
        print("  Percentage:", testComp.Percentage)
        print("  bEnabled:", testComp.bEnabled)
        print("  Position:", testComp.Position.X, testComp.Position.Y, testComp.Position.Z)

        -- 프로퍼티 수정
        testComp.Intensity = 5.5
        testComp.Percentage = 75.0
        testComp.bEnabled = false
        testComp.Position = Vector(100, 200, 300)

        -- 함수 호출
        testComp:SetIntensity(10.0)
        local intensity = testComp:GetIntensity()
        print("  >> After SetIntensity(10.0):")
        print("    GetIntensity():", intensity)

        testComp:Enable()
        print("  >> After Enable():")
        print("    bEnabled:", testComp.bEnabled)

        testComp:Disable()
        print("  >> After Disable():")
        print("    bEnabled:", testComp.bEnabled)
    else
        print("  >> No UTestAutoBindComponent found")
    end


    print("\n========================================")
    print("=== All Tests Completed! ===")
    print("========================================\n")
end


function Tick(dt)
    if not TestStarted then
        return
    end

    -- 실시간 프로퍼티 변경 테스트
  --  local light = GetComponent(Obj, "UPointLightComponent")
    --if light then
        -- 라이트 강도를 사인파로 변화
   --     local time = os.clock()
    --    light.Intensity = 600.0 + math.sin(time * 2.0) * 200.0
  --  end

  --  local rotating = GetComponent(Obj, "URotatingMovementComponent")
  --  if rotating then
        -- 회전 속도를 동적으로 변경
      --  local speed = 30.0 + math.sin(os.clock() * 0.5) * 20.0
   --     rotating.RotationRate = Vector(0, speed, 0)
  --  end
end


function EndPlay()
    print("=== PropertyTest EndPlay ===")
    TestStarted = false
end


function OnBeginOverlap(OtherActor)
    print("PropertyTest: Overlapped with", OtherActor.Tag)
end


function OnEndOverlap(OtherActor)
    print("PropertyTest: End overlap with", OtherActor.Tag)
end
