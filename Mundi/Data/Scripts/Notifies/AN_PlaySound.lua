-- Notify_PlaySound.lua
-- Plays a sound at the actor's location

NotifyClass = {
    DisplayName = "Play Sound",
    Description = "Plays a sound at actor location",

    -- Properties that can be edited in the Timeline UI
    Properties = {
        { Name = "SoundPath", Type = "String", Default = "Data/Audio/CGC1.wav" },
        { Name = "Volume", Type = "Float", Default = 1.0 },
        { Name = "Pitch", Type = "Float", Default = 1.0 }
    }
}

function NotifyClass:Notify(meshComp, time)
    print(string.format("[Notify_PlaySound] Triggered at time %.2f", time))

    -- SoundPath가 설정되지 않았으면 기본값 사용
    local soundPath = self.SoundPath
    if not soundPath or soundPath == "" or soundPath == "None" then
        soundPath = "Data/Audio/CGC1.wav"  -- 기본 사운드 (Properties의 Default와 동일)
    end

    print(string.format("[Notify_PlaySound] Using SoundPath: %s", soundPath))

    -- Lua에서 사운드 경로를 Delegate로 전달, C++에서 FAudioDevice::PlaySound3D() 호출
    meshComp:TriggerAnimNotify(soundPath, time, 0.0)
end

return NotifyClass
