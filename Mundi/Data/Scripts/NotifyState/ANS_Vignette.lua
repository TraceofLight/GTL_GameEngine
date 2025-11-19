-- ANS_Vignette.lua
-- Vignette effect during animation state (damage, buff, focus, etc.)

NotifyStateClass = {
    DisplayName = "Vignette Effect",
    Description = "Screen vignette during animation state (damage, buff, debuff)",

    -- Properties that can be edited in the Timeline UI
    Properties = {
        { Name = "Radius", Type = "Float", Default = 0.5 },
        { Name = "Softness", Type = "Float", Default = 0.5 },
        { Name = "Intensity", Type = "Float", Default = 0.4 },
        { Name = "Roundness", Type = "Float", Default = 1.0 },
        { Name = "ColorR", Type = "Float", Default = 0.8 },
        { Name = "ColorG", Type = "Float", Default = 0.0 },
        { Name = "ColorB", Type = "Float", Default = 0.0 },
        { Name = "FadeInTime", Type = "Float", Default = 0.2 },
        { Name = "FadeOutTime", Type = "Float", Default = 0.3 },
        { Name = "Priority", Type = "Int", Default = 0 }
    }
}

function NotifyStateClass:NotifyBegin(MeshComp, Time)
    local Radius = self.Radius or 0.5
    local Softness = self.Softness or 0.5
    local Intensity = self.Intensity or 0.4
    local Roundness = self.Roundness or 1.0
    local ColorR = self.ColorR or 0.8
    local ColorG = self.ColorG or 0.0
    local ColorB = self.ColorB or 0.0
    local Priority = self.Priority or 0

    local FadeInTime = self.FadeInTime or 0.2
    local FadeOutTime = self.FadeOutTime or 0.3

    -- Get PlayerCameraManager
    local Owner = MeshComp:GetOwner()
    if not Owner then
        print("[ANS_Vignette] Error: MeshComp has no owner")
        return
    end

    local World = Owner:GetWorld()
    if not World then
        print("[ANS_Vignette] Error: Owner has no world")
        return
    end

    local CameraManager = World:GetPlayerCameraManager()
    if not CameraManager then
        print("[ANS_Vignette] Error: World has no PlayerCameraManager")
        return
    end

    -- Create FLinearColor using global Color() function
    local VignetteColor = Color(ColorR, ColorG, ColorB, 1.0)

    -- Start vignette with long duration (will be manually deleted in NotifyEnd)
    -- Duration은 ANS 전체 길이보다 충분히 길게 설정 (페이드 인/아웃 포함)
    local VignetteID = CameraManager:StartVignette(999.0, Radius, Softness, Intensity, Roundness, VignetteColor, Priority)

    -- State를 self에 저장 (NotifyEnd에서 사용)
    self.CameraManager = CameraManager
    self.VignetteID = VignetteID
    self.ElapsedTime = 0.0
    self.TotalDuration = 0.0

    print(string.format("[ANS_Vignette] Started | Radius=%.2f, Intensity=%.2f, Color=(%.1f,%.1f,%.1f)",
          Radius, Intensity, ColorR, ColorG, ColorB))
end

function NotifyStateClass:NotifyTick(MeshComp, Time, DeltaTime)
    if not self.ElapsedTime then
        self.ElapsedTime = 0.0
    end
    if not self.TotalDuration then
        self.TotalDuration = 0.0
    end

    self.ElapsedTime = self.ElapsedTime + DeltaTime
    self.TotalDuration = self.TotalDuration + DeltaTime

    -- Optional: Fade in/out logic
    -- NotifyBegin에서 StartVignette로 이미 시작했으므로
    -- 여기서는 추가적인 강도 조절만 필요하면 구현
    -- 현재는 C++ CameraModifier가 자체 Duration으로 페이드 처리 중
end

function NotifyStateClass:NotifyEnd(MeshComp, Time)
    -- self에 저장된 CameraManager 사용 (캐싱 덕분에 같은 인스턴스 유지됨)
    if self.CameraManager then
        self.CameraManager:DeleteVignette()
        local Duration = self.TotalDuration or 0.0
        print(string.format("[ANS_Vignette] Ended after %.2fs", Duration))
    else
        print("[ANS_Vignette] NotifyEnd: CameraManager not found in self!")
    end

    -- Reset state
    self.VignetteID = nil
    self.CameraManager = nil
    self.ElapsedTime = nil
    self.TotalDuration = nil
end

return NotifyStateClass
