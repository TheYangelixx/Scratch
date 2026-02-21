#ifndef MYSCRATCH_APP_STATE_H
#define MYSCRATCH_APP_STATE_H

#include "sprite.h"
#include "block_manager.h"
#include <string>
#include <vector>
#include <functional>
#include "pen.h"
#include "sound_manager.h"

// ============================================================
//  وضعیت عمومی برنامه
// ============================================================

// ورودی Undo
struct UndoEntry {
    std::string description;
    // می‌توان snapshot کامل ذخیره کرد یا از command pattern استفاده کرد
    // فعلاً ساده نگه می‌داریم
};

struct AppState {

    PenCanvas pen;
    SoundManager sound_manager;
    // --- اسپرایت‌ها ---
    std::vector<Sprite> sprites;
    int current_sprite_index = 0;

    // --- صحنه ---
    int stage_width  = 480;
    int stage_height = 360;
    std::string backdrop_path;
    SDL_Texture* backdrop_texture = nullptr;

    // --- متغیرهای سراسری ---
    std::vector<std::pair<std::string, std::string>> global_variables;

    // --- وضعیت اجرا ---
    bool running       = false;    // آیا پروژه در حال اجراست
    bool paused        = false;
    float execution_speed = 1.0f;  // سرعت اجرا (ضریب)

    // --- Undo / Redo ---
    std::vector<UndoEntry> undo_stack;
    std::vector<UndoEntry> redo_stack;
    static const int MAX_UNDO = 100;

    // --- پروژه ---
    std::string project_name = "Untitled";
    std::string project_path;
    bool        modified = false;

    // --- مدیر بلوک ---
    BlockManager block_manager;

    // --- Turbo mode ---
    bool turbo_mode = false;
};

// ============================================================
//  توابع مدیریت حالت
// ============================================================

// مقداردهی اولیه
void app_state_init(AppState& state);

// دسترسی به اسپرایت فعلی
Sprite* app_state_current_sprite(AppState& state);
const Sprite* app_state_current_sprite(const AppState& state);

// اضافه کردن اسپرایت جدید
int  app_state_add_sprite(AppState& state, const std::string& name);

// حذف اسپرایت
void app_state_remove_sprite(AppState& state, int index);

// انتخاب اسپرایت
void app_state_select_sprite(AppState& state, int index);

// Undo/Redo
void app_state_push_undo(AppState& state, const std::string& description);
void app_state_undo(AppState& state);
void app_state_redo(AppState& state);
bool app_state_can_undo(const AppState& state);
bool app_state_can_redo(const AppState& state);

// اجرا
void app_state_start(AppState& state);
void app_state_stop(AppState& state);
void app_state_pause(AppState& state);
void app_state_resume(AppState& state);

#endif // MYSCRATCH_APP_STATE_H
