#include "ui.h"
#include "app_state.h"
#include "engine.h"
#include "logger.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

int main(int argc, char* argv[]) {
    log_init("myscratch.log");
    log_info("=== MyScratch Started ===");

    AppState state;
    app_state_init(state);

    engine_init(state);

    UI ui;
    if (!ui_init(ui)) {
        log_error("Failed to initialize UI. Exiting.");
        log_shutdown();
        return 1;
    }

    // === اضافه کردن عکس پیش‌فرض به اسپرایت اول ===
    if (!state.sprites.empty()) {
        Costume cost;
        cost.name = "cat";
        cost.file_path = "assets/cat.png"; // همون عکسی که تو پوشه assets گذاشتی
        cost.texture = IMG_LoadTexture(ui.renderer, cost.file_path.c_str());

        if (cost.texture) {
            SDL_QueryTexture(cost.texture, NULL, NULL, &cost.width, &cost.height);
            state.sprites[0].costumes.push_back(cost);
            state.sprites[0].current_costume = 0;
            log_info("Loaded costume: " + cost.file_path);
        } else {
            log_error("Failed to load costume: " + std::string(IMG_GetError()));
        }
    }
    // راه‌اندازی بوم قلم با ابعاد Stage
    pen_canvas_init(state.pen, ui.renderer, STAGE_WIDTH, STAGE_HEIGHT);

    bool quit = false;
    SDL_Event event;

    while (!quit) {
        // رویدادها
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            ui_handle_event(&ui, &state, &event);
        }

        // اجرای موتور
        if (state.running && !state.paused) {
            engine_tick(state);
        }

        // FPS
        Uint32 now = SDL_GetTicks();
        Uint32 delta = now - ui.last_frame_time;
        if (delta > 0) {
            ui.fps = 1000.0f / (float)delta;
        }
        ui.last_frame_time = now;

        // رسم
        ui_render(ui, state);

        // محدودیت فریم (حدود 60fps)
        Uint32 frame_time = SDL_GetTicks() - now;
        if (frame_time < 16) {
            SDL_Delay(16 - frame_time);
        }
    }

    ui_shutdown(ui);
    engine_shutdown(state);
    log_info("=== MyScratch Exiting ===");
    log_shutdown();

    return 0;
}
