#ifndef MYSCRATCH_ENGINE_H
#define MYSCRATCH_ENGINE_H

#include "app_state.h"

// ============================================================
//  موتور اجرای بلوک‌ها
// ============================================================

// مقداردهی اولیه موتور
void engine_init(AppState& state);

// اجرای یک مرحله (tick) از تمام اسکریپت‌های فعال
void engine_tick(AppState& state);

// اجرای یک بلوک واحد
void engine_execute_block(AppState& state, Sprite& sprite, int block_id);

// شروع یک اسکریپت
void engine_start_script(AppState& state, Sprite& sprite, int script_index);

// پاکسازی
void engine_shutdown(AppState& state);

#endif // MYSCRATCH_ENGINE_H
