-- 애니메이션 바인딩 스크립트
local Obj = nil
local CharacterMoveComp = nil
local AnimInstance = nil
local tickCounter = 0

function Start()
    Obj = GetOwner()
    if Obj then
        CharacterMoveComp = Obj:GetCharacterMovement()
        local mesh = Obj:GetMesh()
        if mesh then
            AnimInstance = mesh:GetAnimInstance()
            print("[AnimBinding] Start: AnimInstance initialized successfully")
        else
            print("[AnimBinding] ERROR: Mesh is nil")
        end
    else
        print("[AnimBinding] ERROR: Owner is nil")
    end
end

function Tick(dt)
    if not AnimInstance then
        if tickCounter % 60 == 0 then
            print("[AnimBinding] ERROR: AnimInstance is still nil at tick " .. tickCounter)
        end
        tickCounter = tickCounter + 1
        return
    end

    if not CharacterMoveComp then
        if tickCounter % 60 == 0 then
            print("[AnimBinding] ERROR: CharacterMoveComp is nil at tick " .. tickCounter)
        end
        tickCounter = tickCounter + 1
        return
    end

    -- 속도 벡터 가져오기
    local velocity = CharacterMoveComp:GetVelocity()
    local speed = math.sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z)

    -- 캐릭터의 Forward, Right 벡터 가져오기
    local forward = Obj:GetActorForwardVector()
    local right = Obj:GetActorRightVector()

    -- 속도 벡터 정규화 (C++의 LocalVelocity.Normalize()와 동일)
    local normalizedVelX = 0
    local normalizedVelY = 0
    local normalizedVelZ = 0

    if speed > 0.01 then
        normalizedVelX = velocity.X / speed
        normalizedVelY = velocity.Y / speed
        normalizedVelZ = velocity.Z / speed
    end

    -- 내적 계산 (C++의 Dot Product와 동일)
    local forwardDot = normalizedVelX * forward.X + normalizedVelY * forward.Y + normalizedVelZ * forward.Z
    local rightDot = normalizedVelX * right.X + normalizedVelY * right.Y + normalizedVelZ * right.Z

    -- Signed Speed 계산 (후진 시 음수)
    local signedSpeed = speed
    if forwardDot < 0 then
        signedSpeed = -speed
    end

    -- Direction 계산 (C++과 동일: atan2(RightDot, ForwardDot))
    local direction = math.atan(rightDot, forwardDot) * (180.0 / math.pi)

    -- AnimInstance에 값 설정
    AnimInstance:SetFloat("Speed", signedSpeed)
    AnimInstance:SetFloat("Direction", direction)

    -- 디버깅 로그 (60프레임마다)
    if tickCounter % 60 == 0 then
        print(string.format("[AnimBinding] Tick %d: Speed=%.1f (Raw=%.1f), Direction=%.1f",
            tickCounter, signedSpeed, speed, direction))
        print(string.format("  Velocity=(%.2f, %.2f, %.2f)", velocity.X, velocity.Y, velocity.Z))
        print(string.format("  Forward=(%.2f, %.2f, %.2f), Right=(%.2f, %.2f, %.2f)",
            forward.X, forward.Y, forward.Z, right.X, right.Y, right.Z))
        print(string.format("  ForwardDot=%.3f, RightDot=%.3f", forwardDot, rightDot))
    end

    tickCounter = tickCounter + 1
end
