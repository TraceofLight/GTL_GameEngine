-- ===================================================================
-- Character Animation Binding Script
-- 캐릭터의 속도를 AnimInstance의 "Speed" 파라미터에 바인딩
-- ===================================================================

-- 성능을 위해 매 틱마다 GetMesh 등을 호출하지 않고 변수에 캐싱합니다.
local Character = nil
local SkeletalMesh = nil
local AnimInstance = nil

function BeginPlay()
    print("=== Animation Binding Started ===")

    -- 1. Obj를 Character로 간주 (C++에서 ACharacter로 바인딩 되었다고 가정)
    Character = Obj

    if Character then
        -- 2. USkeletalMeshComponent 가져오기 (GetMesh 함수 사용)
        -- C++: GetMesh() -> Lua: :GetMesh()
        if Character.GetMesh then
            SkeletalMesh = Character:GetMesh()
        else
            print("[Error] Obj does not have GetMesh function. Is it ACharacter?")
            return
        end
    end

    if SkeletalMesh then
        print(" -> SkeletalMesh found.")

        -- 3. UAnimInstance 가져오기 (GetAnimInstance 함수 사용)
        -- C++: GetAnimInstance() -> Lua: :GetAnimInstance()
        if SkeletalMesh.GetAnimInstance then
            AnimInstance = SkeletalMesh:GetAnimInstance()
        else
            print("[Error] SkeletalMesh does not have GetAnimInstance function.")
        end
    end

    if AnimInstance then
        print(" -> AnimInstance found and cached successfully.")
    else
        print("[Warning] AnimInstance is nil. Check if Animation Blueprint is assigned.")
    end
end

function Tick(dt)
    -- AnimInstance와 Character가 유효할 때만 로직 수행
    if Character and AnimInstance then
        
        -- 4. Character의 Velocity 가져오기
        local velocity = Character:GetVelocity()

        -- 5. 속력(Speed) 계산: 벡터의 길이 (Magnitude)
        -- V = sqrt(x^2 + y^2 + z^2)
        -- (만약 Vector 클래스에 :Size()나 :Length()가 바인딩 되어 있다면 그것을 써도 됩니다)
        local speed = math.sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z)

        -- 디버깅용 (필요시 주석 해제)
        -- print("Current Speed:", speed)

        -- 6. AnimInstance에 "Speed" 파라미터 업데이트 (SetFloat 함수 사용)
        -- C++: SetFloat(FName Key, float Value) -> Lua: :SetFloat("Key", Value)
        AnimInstance:SetFloat("Speed", speed)
    end
end

function EndPlay()
    print("=== Animation Binding Ended ===")
    -- 참조 해제
    Character = nil
    SkeletalMesh = nil
    AnimInstance = nil
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end