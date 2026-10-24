-- SimpleEngine demo: Lua changes named sprite animation clips.
function on_start()
    engine.play_animation("Idle")
    engine.log("Terrain & Clips demo running. Use WASD / arrow keys.")
end

function on_update(dt)
    local speed = engine.get_float("speed", 1.2)
    local movement = 0
    if engine.input_down("MoveLeft") then movement = movement - 1 end
    if engine.input_down("MoveRight") then movement = movement + 1 end
    if movement ~= 0 then
        if engine.current_animation() ~= "Run" then engine.play_animation("Run") end
        engine.translate(movement * speed * dt, 0)
    elseif engine.current_animation() ~= "Idle" then
        engine.play_animation("Idle")
    end
end
