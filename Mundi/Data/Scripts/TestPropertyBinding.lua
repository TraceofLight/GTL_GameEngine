-- ===================================================================
-- Property Binding Test Script
-- 자동 생성된 프로퍼티 바인딩 테스트
-- ===================================================================

function BeginPlay()
    print("=== Property Binding Test Started ===")

    -- ===== 1. 기본 프로퍼티 테스트 (AActor) =====
    print("\n[1] AActor Properties:")
    print("  Obj.Tag =", Obj.Tag)
    print("  Obj.bActorHiddenInGame =", Obj.bActorHiddenInGame)
    print("  Obj.bActorIsActive =", Obj.bActorIsActive)

    -- 프로퍼티 수정 테스트
    Obj.Tag = "TestActor_Modified"
    print("  After modification, Obj.Tag =", Obj.Tag)


    -- ===== 2. UProjectileMovementComponent 테스트 =====
    print("\n[2] UProjectileMovementComponent Properties:")
    local projectile = GetComponent(Obj, "UProjectileMovementComponent")
    if projectile then
        print("  InitialSpeed =", projectile.InitialSpeed)
        print("  MaxSpeed =", projectile.MaxSpeed)
        print("  Gravity =", projectile.Gravity.X, projectile.Gravity.Y, projectile.Gravity.Z)
        print("  bIsHomingProjectile =", projectile.bIsHomingProjectile)

        -- 수정 테스트
        projectile.InitialSpeed = 1000.0
        projectile.MaxSpeed = 2000.0
        projectile.bIsHomingProjectile = true
        print("  After modification:")
        print("    InitialSpeed =", projectile.InitialSpeed)
        print("    bIsHomingProjectile =", projectile.bIsHomingProjectile)
    else
        print("  (No ProjectileMovementComponent found)")
    end


    -- ===== 3. USceneComponent 테스트 =====
    print("\n[3] USceneComponent Properties:")
    local scene = GetComponent(Obj, "USceneComponent")
    if scene then
        print("  bIsVisible =", scene.bIsVisible)
        print("  RelativeLocation =", scene.RelativeLocation.X, scene.RelativeLocation.Y, scene.RelativeLocation.Z)
        print("  RelativeScale =", scene.RelativeScale.X, scene.RelativeScale.Y, scene.RelativeScale.Z)

        -- 수정 테스트
        scene.bIsVisible = false
        scene.RelativeLocation = Vector(10, 20, 30)
        print("  After modification:")
        print("    bIsVisible =", scene.bIsVisible)
        print("    RelativeLocation =", scene.RelativeLocation.X, scene.RelativeLocation.Y, scene.RelativeLocation.Z)
    else
        print("  (No SceneComponent found)")
    end


    -- ===== 4. UPointLightComponent 테스트 =====
    print("\n[4] UPointLightComponent Properties:")
    local light = GetComponent(Obj, "UPointLightComponent")
    if light then
        print("  Intensity =", light.Intensity)
        print("  LightColor =", light.LightColor.R, light.LightColor.G, light.LightColor.B)
        print("  bCastShadows =", light.bCastShadows)
        print("  AttenuationRadius =", light.AttenuationRadius)

        -- 수정 테스트
        light.Intensity = 500.0
        light.AttenuationRadius = 1000.0
        print("  After modification:")
        print("    Intensity =", light.Intensity)
        print("    AttenuationRadius =", light.AttenuationRadius)
    else
        print("  (No PointLightComponent found)")
    end


    -- ===== 5. UAudioComponent 테스트 (TArray<USound*>) =====
    print("\n[5] UAudioComponent Properties (with TArray):")
    local audio = GetComponent(Obj, "UAudioComponent")
    if audio then
        print("  Volume =", audio.Volume)
        print("  Pitch =", audio.Pitch)
        print("  bIsLooping =", audio.bIsLooping)
        print("  bAutoPlay =", audio.bAutoPlay)

        -- TArray<USound*> 테스트
        print("  Sounds array:")
        local sounds = audio.Sounds
        if sounds then
            print("    Array size:", #sounds)
            for i, sound in ipairs(sounds) do
                print("    Sound[" .. i .. "] =", sound)
            end
        end

        -- 수정 테스트
        audio.Volume = 0.8
        audio.Pitch = 1.5
        audio.bIsLooping = true
        print("  After modification:")
        print("    Volume =", audio.Volume)
        print("    Pitch =", audio.Pitch)
    else
        print("  (No AudioComponent found)")
    end


    -- ===== 6. UCameraComponent 테스트 =====
    print("\n[6] UCameraComponent Properties:")
    local camera = GetComponent(Obj, "UCameraComponent")
    if camera then
        print("  FieldOfView =", camera.FieldOfView)
        print("  AspectRatio =", camera.AspectRatio)
        print("  NearClip =", camera.NearClip)
        print("  FarClip =", camera.FarClip)

        -- 수정 테스트
        camera.FieldOfView = 90.0
        camera.NearClip = 0.1
        camera.FarClip = 5000.0
        print("  After modification:")
        print("    FieldOfView =", camera.FieldOfView)
        print("    FarClip =", camera.FarClip)
    else
        print("  (No CameraComponent found)")
    end


    -- ===== 7. URotatingMovementComponent 테스트 =====
    print("\n[7] URotatingMovementComponent Properties:")
    local rotating = GetComponent(Obj, "URotatingMovementComponent")
    if rotating then
        print("  RotationRate =", rotating.RotationRate.X, rotating.RotationRate.Y, rotating.RotationRate.Z)
        print("  bRotationInLocalSpace =", rotating.bRotationInLocalSpace)

        -- 수정 테스트
        rotating.RotationRate = Vector(0, 90, 0)
        rotating.bRotationInLocalSpace = true
        print("  After modification:")
        print("    RotationRate =", rotating.RotationRate.X, rotating.RotationRate.Y, rotating.RotationRate.Z)
    else
        print("  (No RotatingMovementComponent found)")
    end


    print("\n=== Property Binding Test Completed ===")
end

function Tick(dt)
    -- 실시간 프로퍼티 변경 테스트
    local light = GetComponent(Obj, "UPointLightComponent")
    if light then
        -- 라이트 강도를 사인파로 변화
        local time = os.clock()
        light.Intensity = 100.0 + math.sin(time * 2.0) * 50.0
    end
end

function EndPlay()
    print("=== Property Binding Test Ended ===")
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end
