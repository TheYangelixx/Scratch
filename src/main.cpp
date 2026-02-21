#include "ui.h"
#include "app_state.h"
#include "engine.h"
#include "logger.h"
#include <SDL2/SDL.h>

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
