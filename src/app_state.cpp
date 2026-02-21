#include "app_state.h"
#include "logger.h"

void app_state_init(AppState& state) {
    state.sprites.clear();
    state.current_sprite_index = 0;
    state.running = false;
    state.paused  = false;
    state.modified = false;
    state.project_name = "Untitled";

    // ساختن یک اسپرایت پیش‌فرض
    app_state_add_sprite(state, "Sprite1");

    log_info("AppState: Initialized with default sprite.");
}

Sprite* app_state_current_sprite(AppState& state) {
    if (state.current_sprite_index >= 0 &&
        state.current_sprite_index < (int)state.sprites.size()) {
        return &state.sprites[state.current_sprite_index];
    }
    return nullptr;
}

const Sprite* app_state_current_sprite(const AppState& state) {
    if (state.current_sprite_index >= 0 &&
        state.current_sprite_index < (int)state.sprites.size()) {
        return &state.sprites[state.current_sprite_index];
    }
    return nullptr;
}

int app_state_add_sprite(AppState& state, const std::string& name) {
    int id = (int)state.sprites.size();
    Sprite s = sprite_create(name, id);
    state.sprites.push_back(s);
    state.current_sprite_index = id;
    state.modified = true;
    log_info("AppState: Added sprite '" + name + "' (id=" + std::to_string(id) + ")");
    return id;
}

void app_state_remove_sprite(AppState& state, int index) {
    if (index < 0 || index >= (int)state.sprites.size()) return;
    if (state.sprites.size() <= 1) {
        log_warn("AppState: Cannot remove last sprite.");
        return;
    }
    std::string name = state.sprites[index].name;
    state.sprites.erase(state.sprites.begin() + index);
    if (state.current_sprite_index >= (int)state.sprites.size()) {
        state.current_sprite_index = (int)state.sprites.size() - 1;
    }
    state.modified = true;
    log_info("AppState: Removed sprite '" + name + "'");
}

void app_state_select_sprite(AppState& state, int index) {
    if (index >= 0 && index < (int)state.sprites.size()) {
        state.current_sprite_index = index;
    }
}

void app_state_push_undo(AppState& state, const std::string& description) {
    UndoEntry entry;
    entry.description = description;
    state.undo_stack.push_back(entry);
    if ((int)state.undo_stack.size() > AppState::MAX_UNDO) {
        state.undo_stack.erase(state.undo_stack.begin());
    }
    state.redo_stack.clear();
    state.modified = true;
}

void app_state_undo(AppState& state) {
    if (state.undo_stack.empty()) return;
    UndoEntry entry = state.undo_stack.back();
    state.undo_stack.pop_back();
    state.redo_stack.push_back(entry);
    log_info("AppState: Undo - " + entry.description);
    // TODO: اعمال واقعی undo
}

void app_state_redo(AppState& state) {
    if (state.redo_stack.empty()) return;
    UndoEntry entry = state.redo_stack.back();
    state.redo_stack.pop_back();
    state.undo_stack.push_back(entry);
    log_info("AppState: Redo - " + entry.description);
    // TODO: اعمال واقعی redo
}

bool app_state_can_undo(const AppState& state) {
    return !state.undo_stack.empty();
}

bool app_state_can_redo(const AppState& state) {
    return !state.redo_stack.empty();
}

void app_state_start(AppState& state) {
    state.running = true;
    state.paused  = false;
    log_info("AppState: Execution started.");
}

void app_state_stop(AppState& state) {
    state.running = false;
    state.paused  = false;
    // ریست مرحله اجرای اسکریپت‌ها
    for (auto& sprite : state.sprites) {
        for (auto& script : sprite.scripts) {
            script.running      = false;
            script.current_step = 0;
        }
    }
    log_info("AppState: Execution stopped.");
}

void app_state_pause(AppState& state) {
    if (state.running) {
        state.paused = true;
        log_info("AppState: Execution paused.");
    }
}

void app_state_resume(AppState& state) {
    if (state.running && state.paused) {
        state.paused = false;
        log_info("AppState: Execution resumed.");
    }
}
