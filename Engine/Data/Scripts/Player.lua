

function BeginPlay()


end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function OnEndOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
	Rotate()
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end



local YawSensitivity        = 0.0005
local PitchSensitivity      = 0.0025
UpVector = Vector(0, 0, 1)
local ForwardVector = Vector(1, 0, 0)

local function RotateAroundAxis(VectorIn, Axis, Angle)
    local UnitAxis = Axis:GetNormalized()
    local CosA, SinA = math.cos(Angle), math.sin(Angle)
    local AxisCrossVector = FVector.Cross(UnitAxis, VectorIn)
    local AxisDotVector   = FVector.Dot(UnitAxis, VectorIn)
    
    return VectorIn * CosA + AxisCrossVector * SinA + UnitAxis * (AxisDotVector * (1.0 - CosA))
end

function Rotate()
    local MouseDelta = InputManager:GetMouseDelta()
    local MouseDeltaX = MouseDelta.X
    local MouseDeltaY = MouseDelta.Y

    local Yaw = MouseDeltaX * YawSensitivity
    ForwardVector = RotateAroundAxis(ForwardVector, UpVector, Yaw)
	ForwardVector:Normalize()

    local RightVector = FVector.Cross(UpVector, ForwardVector)
	RightVector:Normalize()

    local Pitch = MouseDeltaY * PitchSensitivity
    local Candidate = RotateAroundAxis(ForwardVector, RightVector, Pitch)

    -- 수직 잠김 방지
    if (Candidate.Z > 0.4) then -- 아래로 각도 제한
        Candidate.Z = 0.4
    end
    if (Candidate.Z < -0.75) then -- 위로 각도 제한
        Candidate.Z = -0.75
    end

    ForwardVector = Candidate:GetNormalized()

    LootAt = Vector(-ForwardVector.X, -ForwardVector.Y, 0)
    SetPlayerForward(Obj, LootAt)
end
