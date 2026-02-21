#include "engine.h"
#include "logger.h"
#include <bits/stdc++.h>
#include <SDL2/SDL.h>

void engine_init(AppState& state) {
    log_info("Engine: Initialized.");
    (void)state;
}

void engine_tick(AppState& state) {
    if (!state.running || state.paused) return;

    for (auto& sprite : state.sprites) {
        for (auto& script : sprite.scripts) {
            if (!script.running) continue;
            if (script.current_step >= (int)script.block_ids.size()) {
                script.running = false;
                script.current_step = 0;
                continue;
            }

            int block_id = script.block_ids[script.current_step];
            engine_execute_block(state, sprite, block_id);
            script.current_step++;
        }
    }
}

void engine_execute_block(AppState& state, Sprite& sprite, int block_id) {
    Block* b = state.block_manager.get(block_id);
    if (!b) return;

    const std::string& op = b->opcode;

    // --- Motion ---
    if (op == "motion_move") {
        float steps = block_get_field_float(*b, 0, 10.0f);
        sprite_move_steps(sprite, steps);
    }
    else if (op == "motion_turn_right") {
        float deg = block_get_field_float(*b, 0, 15.0f);
        sprite_turn_right(sprite, deg);
    }
    else if (op == "motion_turn_left") {
        float deg = block_get_field_float(*b, 0, 15.0f);
        sprite_turn_left(sprite, deg);
    }
    else if (op == "motion_goto_xy") {
        float gx = block_get_field_float(*b, 0, 0.0f);
        float gy = block_get_field_float(*b, 1, 0.0f);
        sprite_go_to(sprite, gx, gy);
    }
    else if (op == "motion_set_x") {
        sprite.x = block_get_field_float(*b, 0, 0.0f);
    }
    else if (op == "motion_set_y") {
        sprite.y = block_get_field_float(*b, 0, 0.0f);
    }
    else if (op == "motion_change_x") {
        sprite.x += block_get_field_float(*b, 0, 10.0f);
    }
    else if (op == "motion_change_y") {
        sprite.y += block_get_field_float(*b, 0, 10.0f);
    }
    else if (op == "motion_point_dir") {
        sprite_set_direction(sprite, block_get_field_float(*b, 0, 90.0f));
    }
    else if (op == "motion_bounce") {
        sprite_bounce_on_edge(sprite, state.stage_width, state.stage_height);
    }
        // --- Looks ---
    else if (op == "looks_say") {
        std::string text = block_get_field_string(*b, 0, "Hello!");
        sprite_say(sprite, text);
    }
    else if (op == "looks_say_sec") {
        std::string text = block_get_field_string(*b, 0, "Hello!");
        float sec = block_get_field_float(*b, 1, 2.0f);
        sprite_say(sprite, text, sec);
    }
    else if (op == "looks_think") {
        std::string text = block_get_field_string(*b, 0, "Hmm...");
        sprite_think(sprite, text);
    }
    else if (op == "looks_show") {
        sprite_show(sprite);
    }
    else if (op == "looks_hide") {
        sprite_hide(sprite);
    }
    else if (op == "looks_next_costume") {
        sprite_next_costume(sprite);
    }
    else if (op == "looks_set_size") {
        sprite_set_size(sprite, block_get_field_float(*b, 0, 100.0f));
    }
    else if (op == "looks_change_size") {
        sprite_change_size(sprite, block_get_field_float(*b, 0, 10.0f));
    }
        // --- Control ---
    else if (op == "control_wait") {
        // ساده‌سازی: در واقع باید yield کنیم
        float sec = block_get_field_float(*b, 0, 1.0f);
        SDL_Delay((Uint32)(sec * 1000));
    }
    else if (op == "control_stop_all") {
        app_state_stop(state);
    }
        // --- Events ---
        // event ها معمولاً فقط trigger هستند و کد اجرایی ندارند
    else {
        // بلوک ناشناخته - لاگ در debug mode
        // log_debug("Engine: Unknown opcode: " + op);
    }
}

void engine_start_script(AppState& state, Sprite& sprite, int script_index) {
    if (script_index < 0 || script_index >= (int)sprite.scripts.size()) return;
    Script& sc = sprite.scripts[script_index];
    sc.running      = true;
    sc.current_step = 0;
    (void)state;
}

void engine_shutdown(AppState& state) {
    app_state_stop(state);
    log_info("Engine: Shutdown.");
}
