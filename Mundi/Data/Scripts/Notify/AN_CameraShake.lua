-- AN_CameraShake.lua
-- Triggers camera shake effect at a specific animation moment

NotifyClass = {
    DisplayName = "Camera Shake",
    Description = "Triggers instant camera shake (landing, hit, recoil)",

    -- Properties that can be edited in the Timeline UI
    Properties = {
        { Name = "Duration", Type = "Float", Default = 0.4 },
        { Name = "AmplitudeLoc", Type = "Float", Default = 0.5 },
        { Name = "AmplitudeRot", Type = "Float", Default = 0.1 },
        { Name = "Frequency", Type = "Float", Default = 30.0 },
        { Name = "Priority", Type = "Int", Default = 0 }
    }
}

function NotifyClass:Notify(MeshComp, Time)
    local Duration = self.Duration or 0.4
    local AmpLoc = self.AmplitudeLoc or 0.5
    local AmpRot = self.AmplitudeRot or 0.1
    local Freq = self.Frequency or 30.0
    local Priority = self.Priority or 0

    -- MeshComp → Owner → World → PlayerCameraManager
    local Owner = MeshComp:GetOwner()
    if not Owner then
        print("[AN_CameraShake] Error: MeshComp has no owner")
        return
    end

    local World = Owner:GetWorld()
    if not World then
        print("[AN_CameraShake] Error: Owner has no world")
        return
    end

    local CamMgr = World:GetPlayerCameraManager()
    if not CamMgr then
        print("[AN_CameraShake] Error: World has no PlayerCameraManager")
        return
    end

    -- C++ 메서드 호출
    CamMgr:StartCameraShake(Duration, AmpLoc, AmpRot, Freq, Priority)

    print(string.format("[AN_CameraShake] Triggered at %.2fs | Dur=%.2f, Loc=%.1f, Rot=%.1f, Freq=%.1f",
          Time, Duration, AmpLoc, AmpRot, Freq))
end

return NotifyClass
