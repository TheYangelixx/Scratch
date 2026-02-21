#include "ui.h"
#include "block.h"
#include "block_manager.h"
#include "sprite.h"
#include "app_state.h"
#include "logger.h"
#include "engine.h"
#include "project_io.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

// ============================================================
//  توابع کمکی رنگ و هندسه
// ============================================================

SDL_Color ui_to_sdl_color(Color c) {
    return SDL_Color{c.r, c.g, c.b, c.a};
}

bool ui_point_in_rect(int px, int py, SDL_Rect rect) {
    return (px >= rect.x && px < rect.x + rect.w &&
            py >= rect.y && py < rect.y + rect.h);
}

Color ui_get_category_color(BlockCategory cat) {
    switch (cat) {
        case CAT_MOTION:    return {66, 133, 244, 255};    // آبی
        case CAT_LOOKS:     return {147, 83, 211, 255};    // بنفش
        case CAT_SOUND:     return {207, 99, 207, 255};    // صورتی
        case CAT_EVENTS:    return {255, 191, 0, 255};     // زرد
        case CAT_CONTROL:   return {255, 171, 25, 255};    // نارنجی
        case CAT_SENSING:   return {92, 177, 214, 255};    // آبی روشن
        case CAT_OPERATORS: return {89, 192, 89, 255};     // سبز
        case CAT_VARIABLES: return {255, 140, 26, 255};    // نارنجی تیره
        case CAT_PEN:       return {14, 154, 108, 255};    // سبز تیره
        default:            return {128, 128, 128, 255};   // خاکستری
    }
}

std::string ui_get_category_name(BlockCategory cat) {
    switch (cat) {
        case CAT_MOTION:    return "Motion";
        case CAT_LOOKS:     return "Looks";
        case CAT_SOUND:     return "Sound";
        case CAT_EVENTS:    return "Events";
        case CAT_CONTROL:   return "Control";
        case CAT_SENSING:   return "Sensing";
        case CAT_OPERATORS: return "Operators";
        case CAT_VARIABLES: return "Variables";
        case CAT_PEN:       return "Pen";
        default:            return "Unknown";
    }
}

void ui_handle_event(UI* ui, AppState* state, SDL_Event* event)
{
    if (!ui || !state || !event) return;

    int mx = 0, my = 0;

    // ======================================================
    // موس موو - برای hover دکمه‌ها و پالت و دسته‌بندی‌ها
    // ======================================================
    if (event->type == SDL_MOUSEMOTION) {
        mx = event->motion.x;
        my = event->motion.y;

        // hover دکمه‌های تولبار
        ui->btn_green_flag.hovered = ui_point_in_rect(mx, my, ui->btn_green_flag.rect);
        ui->btn_stop.hovered       = ui_point_in_rect(mx, my, ui->btn_stop.rect);
        ui->btn_save.hovered       = ui_point_in_rect(mx, my, ui->btn_save.rect);
        ui->btn_load.hovered       = ui_point_in_rect(mx, my, ui->btn_load.rect);
        ui->btn_undo.hovered       = ui_point_in_rect(mx, my, ui->btn_undo.rect);
        ui->btn_redo.hovered       = ui_point_in_rect(mx, my, ui->btn_redo.rect);

        // hover دسته‌بندی‌ها
        for (auto& cat : ui->categories) {
            cat.hovered = ui_point_in_rect(mx, my, cat.rect);
        }

        // hover آیتم‌های پالت
        for (auto& item : ui->palette_items) {
            int draw_y = ui->rect_palette.y + item.rect.y - ui->palette_scroll.offset_y;
            SDL_Rect abs_rect = {
                    ui->rect_palette.x + item.rect.x,
                    draw_y,
                    item.rect.w, item.rect.h
            };
            item.hovered = ui_point_in_rect(mx, my, abs_rect);
        }

        // حرکت درگ و تشخیص Snap
        if (ui->drag.active) {
            ui->drag.mouse_x = mx;
            ui->drag.mouse_y = my;
            ui->drag.snap_valid = false;

            if (ui_point_in_rect(mx, my, ui->rect_scripts)) {
                Sprite* sprite = app_state_current_sprite(*state);
                if (sprite) {
                    int drag_x = mx - ui->drag.offset_x;
                    int drag_y = my - ui->drag.offset_y;

                    for (int s_idx = 0; s_idx < (int)sprite->scripts.size(); s_idx++) {
                        auto& script = sprite->scripts[s_idx];
                        int current_y = ui->rect_scripts.y + script.y - ui->script_scroll.offset_y;
                        int base_x = ui->rect_scripts.x + script.x - ui->script_scroll.offset_x;

                        for (int i = 0; i < (int)script.block_ids.size(); i++) {
                            int bid = script.block_ids[i];
                            if (bid == ui->drag.block_id) continue;

                            Block* b = state->block_manager.get(bid);
                            if (!b) continue;

                            int target_x = base_x;
                            int target_y = current_y + b->height; // پایین بلوک هدف

                            // اگر فاصله کمتر از 30 پیکسل بود (آهن‌ربا فعال می‌شه)
                            if (std::abs(drag_x - target_x) < 30 && std::abs(drag_y - target_y) < 30) {
                                ui->drag.snap_valid = true;
                                ui->drag.snap_script_idx = s_idx;
                                ui->drag.snap_position = i + 1; // اضافه شدن بعد از این بلوک

                                // شیفت دادن موس به نقطه اتصال برای حس آهن‌ربایی
                                ui->drag.mouse_x = target_x + ui->drag.offset_x;
                                ui->drag.mouse_y = target_y + ui->drag.offset_y;
                                break;
                            }
                            current_y += b->height + 2; // +2 همون فاصله بین بلوک‌هاست
                        }
                        if (ui->drag.snap_valid) break;
                    }
                }
            }
        }
    }

        // ======================================================
        // کلیک موس
        // ======================================================
    else if (event->type == SDL_MOUSEBUTTONDOWN) {
        mx = event->button.x;
        my = event->button.y;

        if (event->button.button == SDL_BUTTON_LEFT) {

            // --- دکمه اجرا (پرچم سبز) ---
            if (ui_point_in_rect(mx, my, ui->btn_green_flag.rect)) {
                ui->btn_green_flag.pressed = true;
                if (!state->running) {
                    app_state_start(*state);
                    // اجرای اسکریپت‌های green_flag برای همه اسپرایت‌ها
                    for (auto& sprite : state->sprites) {
                        for (auto& script : sprite.scripts) {
                            if (!script.block_ids.empty()) {
                                script.running = true;
                                script.current_step = 0;
                            }
                        }
                    }
                }
            }
                // --- دکمه توقف ---
            else if (ui_point_in_rect(mx, my, ui->btn_stop.rect)) {
                ui->btn_stop.pressed = true;
                app_state_stop(*state);
            }
                // --- دکمه Save ---
            else if (ui_point_in_rect(mx, my, ui->btn_save.rect)) {
                ui->btn_save.pressed = true;
                project_save(*state, "project.msc");
                log_info("UI: Project saved.");
            }
                // --- دکمه Load ---
            else if (ui_point_in_rect(mx, my, ui->btn_load.rect)) {
                ui->btn_load.pressed = true;
                project_load(*state, "project.msc");
                log_info("UI: Project loaded.");
            }
                // --- دکمه Undo ---
            else if (ui_point_in_rect(mx, my, ui->btn_undo.rect)) {
                ui->btn_undo.pressed = true;
                if (app_state_can_undo(*state)) {
                    app_state_undo(*state);
                }
            }
                // --- دکمه Redo ---
            else if (ui_point_in_rect(mx, my, ui->btn_redo.rect)) {
                ui->btn_redo.pressed = true;
                if (app_state_can_redo(*state)) {
                    app_state_redo(*state);
                }
            }
                // --- دسته‌بندی‌ها ---
            else if (ui_point_in_rect(mx, my, ui->rect_categories)) {
                for (int i = 0; i < (int) ui->categories.size(); i++) {
                    auto &cat = ui->categories[i];

                    if (ui_point_in_rect(mx, my, cat.rect)) {
                        for (auto &c: ui->categories) c.selected = false;
                        cat.selected = true;
                        ui->selected_category = i;
                        ui_populate_palette(*ui, i);
                        break;
                    }
                }
            }
                // --- پالت - شروع درگ بلوک ---
            else if (ui_point_in_rect(mx, my, ui->rect_palette)) {
                for (auto& item : ui->palette_items) {
                    int draw_y = ui->rect_palette.y + item.rect.y - ui->palette_scroll.offset_y;
                    SDL_Rect abs_rect = {
                            ui->rect_palette.x + item.rect.x,
                            draw_y,
                            item.rect.w, item.rect.h
                    };
                    if (ui_point_in_rect(mx, my, abs_rect)) {
                        // شروع درگ از پالت
                        ui->drag.active = true;
                        ui->drag.from_palette = true;
                        ui->drag.dragged_block = item.proto;
                        ui->drag.block_id = -1;
                        ui->drag.offset_x = mx - abs_rect.x;
                        ui->drag.offset_y = my - abs_rect.y;
                        ui->drag.mouse_x = mx;
                        ui->drag.mouse_y = my;
                        break;
                    }
                }
            }
                // --- ناحیه اسکریپت - کلیک روی بلوک ---
            else if (ui_point_in_rect(mx, my, ui->rect_scripts)) {
                Sprite* sprite = app_state_current_sprite(*state);
                if (sprite) {
                    bool found = false;
                    for (auto& script : sprite->scripts) {
                        if (found) break;
                        for (int bid : script.block_ids) {
                            Block* b = state->block_manager.get(bid);
                            if (!b) continue;
                            int bx = ui->rect_scripts.x + script.x + b->x - ui->script_scroll.offset_x;
                            int by = ui->rect_scripts.y + script.y + b->y - ui->script_scroll.offset_y;
                            SDL_Rect brect = {bx, by, b->width, b->height};
                            if (ui_point_in_rect(mx, my, brect)) {
                                ui->drag.active = true;
                                ui->drag.from_palette = false;
                                ui->drag.dragged_block = *b;
                                ui->drag.block_id = bid;
                                ui->drag.offset_x = mx - bx;
                                ui->drag.offset_y = my - by;
                                ui->drag.mouse_x = mx;
                                ui->drag.mouse_y = my;
                                found = true;
                                break;
                            }
                        }
                    }
                    // اگر روی فضای خالی اسکریپت کلیک شد، اسکرول شروع می‌شه
                    if (!found) {
                        ui->script_scroll.dragging = true;
                    }
                }
            }
                // --- لیست اسپرایت ---
            else if (ui_point_in_rect(mx, my, ui->rect_sprite_list)) {
                int sprite_h = 50;
                int y_off = ui->rect_sprite_list.y + 10;
                for (int i = 0; i < (int)state->sprites.size(); i++) {
                    SDL_Rect sr = {ui->rect_sprite_list.x + 10, y_off, ui->rect_sprite_list.w - 20, sprite_h - 4};
                    if (ui_point_in_rect(mx, my, sr)) {
                        app_state_select_sprite(*state, i);
                        break;
                    }
                    y_off += sprite_h;
                }
            }
        }
    }

        // ======================================================
        // رها کردن موس - پایان درگ
        // ======================================================
    else if (event->type == SDL_MOUSEBUTTONUP) {
        mx = event->button.x;
        my = event->button.y;

        // reset pressed دکمه‌ها
        ui->btn_green_flag.pressed = false;
        ui->btn_stop.pressed = false;
        ui->btn_save.pressed = false;
        ui->btn_load.pressed = false;
        ui->btn_undo.pressed = false;
        ui->btn_redo.pressed = false;
        ui->script_scroll.dragging = false;

        if (ui->drag.active && event->button.button == SDL_BUTTON_LEFT) {
            if (ui_point_in_rect(mx, my, ui->rect_scripts)) {
                Sprite* sprite = app_state_current_sprite(*state);
                if (sprite) {
                    int placed_id = ui->drag.block_id;

                    // ۱. اگر از پالت آمده، اول یک بلوک جدید بسازیم
                    if (ui->drag.from_palette) {
                        Block new_block = ui->drag.dragged_block;
                        new_block.id = -1;
                        Block created = block_create(new_block.opcode, new_block.text,
                                                     new_block.category, new_block.block_type);
                        created.fields = new_block.fields;
                        created.width  = new_block.width;
                        created.height = new_block.height;
                        created.x = 0; created.y = 0;
                        placed_id = state->block_manager.add(created);
                    }
                        // ۲. اگر از روی صفحه برداشتیم، اول از اسکریپت قبلی پاکش کنیم
                    else {
                        for (auto it = sprite->scripts.begin(); it != sprite->scripts.end(); ++it) {
                            auto& ids = it->block_ids;
                            auto pos = std::find(ids.begin(), ids.end(), placed_id);
                            if (pos != ids.end()) {
                                ids.erase(pos);
                                if (ids.empty()) sprite->scripts.erase(it);
                                break; // پیدا شد و حذف شد
                            }
                        }
                    }

                    // ۳. بررسی کنیم آیا باید به بلوک دیگری بچسبد؟
                    if (ui->drag.snap_valid && ui->drag.snap_script_idx < (int)sprite->scripts.size()) {
                        auto& target_script = sprite->scripts[ui->drag.snap_script_idx];
                        int pos = ui->drag.snap_position;
                        if (pos > (int)target_script.block_ids.size()) pos = (int)target_script.block_ids.size();
                        target_script.block_ids.insert(target_script.block_ids.begin() + pos, placed_id);
                    }
                        // ۴. اگر اسنپ نشده، یک اسکریپت مستقل جدید بسازیم
                    else {
                        Script new_script;
                        new_script.x = mx - ui->rect_scripts.x + ui->script_scroll.offset_x - ui->drag.offset_x;
                        new_script.y = my - ui->rect_scripts.y + ui->script_scroll.offset_y - ui->drag.offset_y;
                        new_script.block_ids.push_back(placed_id);
                        sprite->scripts.push_back(new_script);
                    }
                }
            } else {
                // ۵. رها کردن بیرون از ناحیه اسکریپت (سطل آشغال - حذف بلوک)
                if (!ui->drag.from_palette) {
                    Sprite* sprite = app_state_current_sprite(*state);
                    if (sprite) {
                        for (auto it = sprite->scripts.begin(); it != sprite->scripts.end(); ++it) {
                            auto& ids = it->block_ids;
                            auto pos = std::find(ids.begin(), ids.end(), ui->drag.block_id);
                            if (pos != ids.end()) {
                                state->block_manager.remove(ui->drag.block_id);
                                ids.erase(pos);
                                if (ids.empty()) sprite->scripts.erase(it);
                                break;
                            }
                        }
                    }
                }
            }

            // پایان وضعیت درگ
            ui->drag.active = false;
            ui->drag.snap_valid = false;
            ui->drag.block_id = -1;
        }
    }

        // ======================================================
        // اسکرول ماوس
        // ======================================================
    else if (event->type == SDL_MOUSEWHEEL) {
        SDL_GetMouseState(&mx, &my);
        int scroll_amount = event->wheel.y * 20;

        if (ui_point_in_rect(mx, my, ui->rect_palette)) {
            ui->palette_scroll.offset_y -= scroll_amount;
            if (ui->palette_scroll.offset_y < 0) ui->palette_scroll.offset_y = 0;
        }
        else if (ui_point_in_rect(mx, my, ui->rect_scripts)) {
            ui->script_scroll.offset_y -= scroll_amount;
            if (ui->script_scroll.offset_y < 0) ui->script_scroll.offset_y = 0;
        }
    }

        // ======================================================
        // کیبورد
        // ======================================================
    else if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            // لغو درگ با Escape
            if (ui->drag.active) {
                ui->drag.active = false;
                ui->drag.snap_valid = false;
            }
            // لغو ویرایش فیلد
            if (ui->field_edit.active) {
                ui->field_edit.active = false;
                ui->field_edit.buffer = "";
            }
        }
        else if (ui->field_edit.active) {
            // ویرایش فیلد بلوک با صفحه کلید
            if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER) {
                // تأیید ویرایش
                Block* b = state->block_manager.get(ui->field_edit.block_id);
                if (b && ui->field_edit.field_index < (int)b->fields.size()) {
                    b->fields[ui->field_edit.field_index].value = ui->field_edit.buffer;
                }
                ui->field_edit.active = false;
            }
            else if (event->key.keysym.sym == SDLK_BACKSPACE) {
                if (!ui->field_edit.buffer.empty()) {
                    ui->field_edit.buffer.pop_back();
                }
            }
        }
    }

        // ======================================================
        // ورودی متن برای ویرایش فیلد
        // ======================================================
    else if (event->type == SDL_TEXTINPUT) {
        if (ui->field_edit.active) {
            ui->field_edit.buffer += event->text.text;
        }
    }
}

// ============================================================
//  رسم مستطیل گرد
// ============================================================

void ui_draw_rounded_rect(SDL_Renderer* renderer, SDL_Rect rect, Color color, int radius) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // بدنه اصلی
    SDL_Rect body = {rect.x + radius, rect.y, rect.w - 2 * radius, rect.h};
    SDL_RenderFillRect(renderer, &body);

    SDL_Rect left = {rect.x, rect.y + radius, radius, rect.h - 2 * radius};
    SDL_RenderFillRect(renderer, &left);

    SDL_Rect right = {rect.x + rect.w - radius, rect.y + radius, radius, rect.h - 2 * radius};
    SDL_RenderFillRect(renderer, &right);

    // گوشه‌ها (ساده‌سازی با مربع)
    for (int i = 0; i < radius; i++) {
        for (int j = 0; j < radius; j++) {
            float dist = sqrtf((float)((radius - i) * (radius - i) + (radius - j) * (radius - j)));
            if (dist <= radius) {
                // گوشه بالا چپ
                SDL_RenderDrawPoint(renderer, rect.x + i, rect.y + j);
                // گوشه بالا راست
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - 1 - i, rect.y + j);
                // گوشه پایین چپ
                SDL_RenderDrawPoint(renderer, rect.x + i, rect.y + rect.h - 1 - j);
                // گوشه پایین راست
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - 1 - i, rect.y + rect.h - 1 - j);
            }
        }
    }
}

void ui_draw_rounded_rect_outline(SDL_Renderer* renderer, SDL_Rect rect, Color color, int radius) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // خطوط افقی
    SDL_RenderDrawLine(renderer, rect.x + radius, rect.y, rect.x + rect.w - radius, rect.y);
    SDL_RenderDrawLine(renderer, rect.x + radius, rect.y + rect.h - 1, rect.x + rect.w - radius, rect.y + rect.h - 1);

    // خطوط عمودی
    SDL_RenderDrawLine(renderer, rect.x, rect.y + radius, rect.x, rect.y + rect.h - radius);
    SDL_RenderDrawLine(renderer, rect.x + rect.w - 1, rect.y + radius, rect.x + rect.w - 1, rect.y + rect.h - radius);

    // گوشه‌ها (تقریبی)
    for (int angle = 0; angle < 90; angle++) {
        float rad = angle * 3.14159f / 180.0f;
        int dx = (int)(radius * cosf(rad));
        int dy = (int)(radius * sinf(rad));

        SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + radius - dy);
        SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + radius - dy);
        SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + rect.h - radius + dy - 1);
        SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + rect.h - radius + dy - 1);
    }
}

// ============================================================
//  رسم متن
// ============================================================

void ui_draw_text(SDL_Renderer* renderer, TTF_Font* font,
                  const std::string& text, int x, int y, Color color) {
    if (!font || text.empty()) return;

    SDL_Color sdl_color = ui_to_sdl_color(color);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), sdl_color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void ui_draw_text_centered(SDL_Renderer* renderer, TTF_Font* font,
                           const std::string& text, SDL_Rect rect, Color color) {
    if (!font || text.empty()) return;

    int tw = 0, th = 0;
    TTF_SizeUTF8(font, text.c_str(), &tw, &th);

    int x = rect.x + (rect.w - tw) / 2;
    int y = rect.y + (rect.h - th) / 2;

    ui_draw_text(renderer, font, text, x, y, color);
}

// ============================================================
//  رسم دکمه
// ============================================================

void ui_draw_button(UI& ui, UIButton& btn) {
    if (!btn.visible) return;

    Color bg = btn.bg_color;
    if (btn.pressed) {
        bg.r = (Uint8)(bg.r * 0.7f);
        bg.g = (Uint8)(bg.g * 0.7f);
        bg.b = (Uint8)(bg.b * 0.7f);
    } else if (btn.hovered) {
        bg.r = (Uint8)std::min(255, bg.r + 30);
        bg.g = (Uint8)std::min(255, bg.g + 30);
        bg.b = (Uint8)std::min(255, bg.b + 30);
    }

    ui_draw_rounded_rect(ui.renderer, btn.rect, bg, 5);
    ui_draw_text_centered(ui.renderer, ui.font, btn.label, btn.rect, btn.text_color);
}

// ============================================================
//  ساخت بلوک‌های پالت
// ============================================================

static std::vector<Block> create_motion_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create_with_field("motion_move", "move %1 steps",
                                             CAT_MOTION, BTYPE_STACK, "steps", "10"));

    blocks.push_back(block_create_with_field("motion_turn_right", "turn right %1 degrees",
                                             CAT_MOTION, BTYPE_STACK, "degrees", "15"));

    blocks.push_back(block_create_with_field("motion_turn_left", "turn left %1 degrees",
                                             CAT_MOTION, BTYPE_STACK, "degrees", "15"));

    blocks.push_back(block_create("motion_goto_random", "go to random position",
                                  CAT_MOTION, BTYPE_STACK));

    blocks.push_back(block_create_with_field("motion_goto_xy", "go to x: %1 y: %2",
                                             CAT_MOTION, BTYPE_STACK, "x", "0"));
    blocks.back().fields.push_back({"y", FIELD_NUMBER, "0", "0", {}, -9999, 9999, 0,0,0,0});

    blocks.push_back(block_create_with_field("motion_glide", "glide %1 secs to x: %2 y: %3",
                                             CAT_MOTION, BTYPE_STACK, "secs", "1"));
    blocks.back().fields.push_back({"x", FIELD_NUMBER, "0", "0", {}, -9999, 9999, 0,0,0,0});
    blocks.back().fields.push_back({"y", FIELD_NUMBER, "0", "0", {}, -9999, 9999, 0,0,0,0});

    blocks.push_back(block_create_with_field("motion_point_dir", "point in direction %1",
                                             CAT_MOTION, BTYPE_STACK, "direction", "90"));

    blocks.push_back(block_create("motion_bounce", "if on edge, bounce",
                                  CAT_MOTION, BTYPE_STACK));

    blocks.push_back(block_create_with_field("motion_set_x", "set x to %1",
                                             CAT_MOTION, BTYPE_STACK, "x", "0"));

    blocks.push_back(block_create_with_field("motion_set_y", "set y to %1",
                                             CAT_MOTION, BTYPE_STACK, "y", "0"));

    blocks.push_back(block_create_with_field("motion_change_x", "change x by %1",
                                             CAT_MOTION, BTYPE_STACK, "dx", "10"));

    blocks.push_back(block_create_with_field("motion_change_y", "change y by %1",
                                             CAT_MOTION, BTYPE_STACK, "dy", "10"));

    // Reporter blocks
    Block xpos = block_create("motion_xpos", "x position", CAT_MOTION, BTYPE_REPORTER);
    blocks.push_back(xpos);

    Block ypos = block_create("motion_ypos", "y position", CAT_MOTION, BTYPE_REPORTER);
    blocks.push_back(ypos);

    Block dir = block_create("motion_direction", "direction", CAT_MOTION, BTYPE_REPORTER);
    blocks.push_back(dir);

    return blocks;
}

static std::vector<Block> create_looks_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create_with_string_field("looks_say", "say %1",
                                                    CAT_LOOKS, BTYPE_STACK, "message", "Hello!"));

    blocks.push_back(block_create_with_string_field("looks_say_sec", "say %1 for %2 seconds",
                                                    CAT_LOOKS, BTYPE_STACK, "message", "Hello!"));
    blocks.back().fields.push_back({"secs", FIELD_NUMBER, "2", "2", {}, 0, 999, 0,0,0,0});

    blocks.push_back(block_create_with_string_field("looks_think", "think %1",
                                                    CAT_LOOKS, BTYPE_STACK, "message", "Hmm..."));

    blocks.push_back(block_create_with_string_field("looks_think_sec", "think %1 for %2 seconds",
                                                    CAT_LOOKS, BTYPE_STACK, "message", "Hmm..."));
    blocks.back().fields.push_back({"secs", FIELD_NUMBER, "2", "2", {}, 0, 999, 0,0,0,0});

    blocks.push_back(block_create("looks_show", "show", CAT_LOOKS, BTYPE_STACK));
    blocks.push_back(block_create("looks_hide", "hide", CAT_LOOKS, BTYPE_STACK));

    blocks.push_back(block_create("looks_next_costume", "next costume", CAT_LOOKS, BTYPE_STACK));
    blocks.push_back(block_create("looks_prev_costume", "previous costume", CAT_LOOKS, BTYPE_STACK));

    blocks.push_back(block_create_with_field("looks_set_size", "set size to %1 %",
                                             CAT_LOOKS, BTYPE_STACK, "size", "100"));

    blocks.push_back(block_create_with_field("looks_change_size", "change size by %1",
                                             CAT_LOOKS, BTYPE_STACK, "change", "10"));

    blocks.push_back(block_create("looks_go_front", "go to front layer", CAT_LOOKS, BTYPE_STACK));
    blocks.push_back(block_create("looks_go_back", "go to back layer", CAT_LOOKS, BTYPE_STACK));

    // Reporters
    blocks.push_back(block_create("looks_costume_num", "costume number", CAT_LOOKS, BTYPE_REPORTER));
    blocks.push_back(block_create("looks_size", "size", CAT_LOOKS, BTYPE_REPORTER));

    return blocks;
}

static std::vector<Block> create_sound_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create_with_dropdown("sound_play", "play sound %1",
                                                CAT_SOUND, BTYPE_STACK, "sound",
                                                {"pop", "meow", "drum"}, "pop"));

    blocks.push_back(block_create_with_dropdown("sound_play_until", "play sound %1 until done",
                                                CAT_SOUND, BTYPE_STACK, "sound",
                                                {"pop", "meow", "drum"}, "pop"));

    blocks.push_back(block_create("sound_stop", "stop all sounds", CAT_SOUND, BTYPE_STACK));

    blocks.push_back(block_create_with_field("sound_set_volume", "set volume to %1 %",
                                             CAT_SOUND, BTYPE_STACK, "volume", "100"));

    blocks.push_back(block_create_with_field("sound_change_volume", "change volume by %1",
                                             CAT_SOUND, BTYPE_STACK, "change", "-10"));

    blocks.push_back(block_create("sound_volume", "volume", CAT_SOUND, BTYPE_REPORTER));

    return blocks;
}

static std::vector<Block> create_events_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create("event_flag", "when green flag clicked",
                                  CAT_EVENTS, BTYPE_HAT));

    blocks.push_back(block_create_with_dropdown("event_key", "when %1 key pressed",
                                                CAT_EVENTS, BTYPE_HAT, "key",
                                                {"space", "up arrow", "down arrow", "left arrow", "right arrow", "any"}, "space"));

    blocks.push_back(block_create("event_clicked", "when this sprite clicked",
                                  CAT_EVENTS, BTYPE_HAT));

    blocks.push_back(block_create("event_backdrop", "when backdrop switches",
                                  CAT_EVENTS, BTYPE_HAT));

    blocks.push_back(block_create_with_string_field("event_broadcast", "broadcast %1",
                                                    CAT_EVENTS, BTYPE_STACK, "message", "message1"));

    blocks.push_back(block_create_with_string_field("event_broadcast_wait", "broadcast %1 and wait",
                                                    CAT_EVENTS, BTYPE_STACK, "message", "message1"));

    blocks.push_back(block_create_with_string_field("event_receive", "when I receive %1",
                                                    CAT_EVENTS, BTYPE_HAT, "message", "message1"));

    return blocks;
}

static std::vector<Block> create_control_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create_with_field("control_wait", "wait %1 seconds",
                                             CAT_CONTROL, BTYPE_STACK, "duration", "1"));

    Block repeat = block_create_with_field("control_repeat", "repeat %1",
                                           CAT_CONTROL, BTYPE_C_BLOCK, "times", "10");
    repeat.height = 80;
    blocks.push_back(repeat);

    Block forever = block_create("control_forever", "forever", CAT_CONTROL, BTYPE_C_BLOCK);
    forever.height = 80;
    blocks.push_back(forever);

    Block if_block = block_create("control_if", "if %1 then", CAT_CONTROL, BTYPE_C_BLOCK);
    if_block.height = 80;
    BlockField cond_field;
    cond_field.name = "condition";
    cond_field.type = FIELD_BOOLEAN_SLOT;
    if_block.fields.push_back(cond_field);
    blocks.push_back(if_block);

    Block if_else = block_create("control_if_else", "if %1 then ... else", CAT_CONTROL, BTYPE_C_BLOCK);
    if_else.height = 120;
    if_else.fields.push_back(cond_field);
    blocks.push_back(if_else);

    blocks.push_back(block_create("control_wait_until", "wait until %1", CAT_CONTROL, BTYPE_STACK));

    Block repeat_until = block_create("control_repeat_until", "repeat until %1",
                                      CAT_CONTROL, BTYPE_C_BLOCK);
    repeat_until.height = 80;
    blocks.push_back(repeat_until);

    blocks.push_back(block_create("control_stop_all", "stop all", CAT_CONTROL, BTYPE_CAP));
    blocks.push_back(block_create("control_stop_this", "stop this script", CAT_CONTROL, BTYPE_CAP));

    Block clone = block_create("control_clone", "create clone of myself", CAT_CONTROL, BTYPE_STACK);
    blocks.push_back(clone);

    blocks.push_back(block_create("control_delete_clone", "delete this clone", CAT_CONTROL, BTYPE_CAP));

    blocks.push_back(block_create("control_when_clone", "when I start as a clone",
                                  CAT_CONTROL, BTYPE_HAT));

    return blocks;
}

static std::vector<Block> create_sensing_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create_with_dropdown("sensing_touching", "touching %1?",
                                                CAT_SENSING, BTYPE_BOOLEAN, "object",
                                                {"mouse-pointer", "edge", "Sprite1"}, "mouse-pointer"));

    blocks.push_back(block_create_with_dropdown("sensing_touching_color", "touching color %1?",
                                                CAT_SENSING, BTYPE_BOOLEAN, "color",
                                                {}, "#FF0000"));

    blocks.push_back(block_create_with_dropdown("sensing_key_pressed", "key %1 pressed?",
                                                CAT_SENSING, BTYPE_BOOLEAN, "key",
                                                {"space", "up arrow", "down arrow", "left arrow", "right arrow", "a", "b"}, "space"));

    blocks.push_back(block_create("sensing_mouse_down", "mouse down?", CAT_SENSING, BTYPE_BOOLEAN));

    blocks.push_back(block_create("sensing_mouse_x", "mouse x", CAT_SENSING, BTYPE_REPORTER));
    blocks.push_back(block_create("sensing_mouse_y", "mouse y", CAT_SENSING, BTYPE_REPORTER));

    blocks.push_back(block_create("sensing_loudness", "loudness", CAT_SENSING, BTYPE_REPORTER));
    blocks.push_back(block_create("sensing_timer", "timer", CAT_SENSING, BTYPE_REPORTER));
    blocks.push_back(block_create("sensing_reset_timer", "reset timer", CAT_SENSING, BTYPE_STACK));

    blocks.push_back(block_create_with_string_field("sensing_ask", "ask %1 and wait",
                                                    CAT_SENSING, BTYPE_STACK, "question", "What's your name?"));

    blocks.push_back(block_create("sensing_answer", "answer", CAT_SENSING, BTYPE_REPORTER));

    blocks.push_back(block_create("sensing_days_since_2000", "days since 2000", CAT_SENSING, BTYPE_REPORTER));
    blocks.push_back(block_create("sensing_username", "username", CAT_SENSING, BTYPE_REPORTER));

    return blocks;
}

static std::vector<Block> create_operators_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create_with_field("operator_add", "%1 + %2",
                                             CAT_OPERATORS, BTYPE_REPORTER, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "", "", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_subtract", "%1 - %2",
                                             CAT_OPERATORS, BTYPE_REPORTER, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "", "", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_multiply", "%1 * %2",
                                             CAT_OPERATORS, BTYPE_REPORTER, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "", "", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_divide", "%1 / %2",
                                             CAT_OPERATORS, BTYPE_REPORTER, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "", "", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_random", "pick random %1 to %2",
                                             CAT_OPERATORS, BTYPE_REPORTER, "from", "1"));
    blocks.back().fields.push_back({"to", FIELD_NUMBER, "10", "10", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_gt", "%1 > %2",
                                             CAT_OPERATORS, BTYPE_BOOLEAN, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "50", "50", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_lt", "%1 < %2",
                                             CAT_OPERATORS, BTYPE_BOOLEAN, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "50", "50", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_equals", "%1 = %2",
                                             CAT_OPERATORS, BTYPE_BOOLEAN, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "50", "50", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create("operator_and", "%1 and %2", CAT_OPERATORS, BTYPE_BOOLEAN));
    blocks.push_back(block_create("operator_or", "%1 or %2", CAT_OPERATORS, BTYPE_BOOLEAN));
    blocks.push_back(block_create("operator_not", "not %1", CAT_OPERATORS, BTYPE_BOOLEAN));

    blocks.push_back(block_create_with_string_field("operator_join", "join %1 %2",
                                                    CAT_OPERATORS, BTYPE_REPORTER, "a", "apple"));
    blocks.back().fields.push_back({"b", FIELD_STRING, "banana", "banana", {}, 0, 0, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_mod", "%1 mod %2",
                                             CAT_OPERATORS, BTYPE_REPORTER, "a", ""));
    blocks.back().fields.push_back({"b", FIELD_NUMBER, "", "", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("operator_round", "round %1",
                                             CAT_OPERATORS, BTYPE_REPORTER, "num", ""));

    blocks.push_back(block_create_with_dropdown("operator_mathop", "%1 of %2",
                                                CAT_OPERATORS, BTYPE_REPORTER, "op",
                                                {"abs", "floor", "ceiling", "sqrt", "sin", "cos", "tan", "asin", "acos", "atan", "ln", "log", "e^", "10^"}, "abs"));
    blocks.back().fields.push_back({"num", FIELD_NUMBER, "", "", {}, -99999, 99999, 0,0,0,0});

    return blocks;
}

static std::vector<Block> create_variables_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create_with_dropdown("data_variable", "my variable",
                                                CAT_VARIABLES, BTYPE_REPORTER, "var",
                                                {"my variable"}, "my variable"));

    blocks.push_back(block_create_with_field("data_set_var", "set %1 to %2",
                                             CAT_VARIABLES, BTYPE_STACK, "var", "my variable"));
    blocks.back().fields.push_back({"value", FIELD_NUMBER, "0", "0", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_field("data_change_var", "change %1 by %2",
                                             CAT_VARIABLES, BTYPE_STACK, "var", "my variable"));
    blocks.back().fields.push_back({"value", FIELD_NUMBER, "1", "1", {}, -99999, 99999, 0,0,0,0});

    blocks.push_back(block_create_with_dropdown("data_show_var", "show variable %1",
                                                CAT_VARIABLES, BTYPE_STACK, "var",
                                                {"my variable"}, "my variable"));

    blocks.push_back(block_create_with_dropdown("data_hide_var", "hide variable %1",
                                                CAT_VARIABLES, BTYPE_STACK, "var",
                                                {"my variable"}, "my variable"));

    // لیست‌ها
    blocks.push_back(block_create_with_string_field("data_add_to_list", "add %1 to %2",
                                                    CAT_VARIABLES, BTYPE_STACK, "item", "thing"));
    blocks.back().fields.push_back({"list", FIELD_DROPDOWN, "my list", "my list", {"my list"}, 0, 0, 0,0,0,0});

    blocks.push_back(block_create_with_field("data_delete_of_list", "delete %1 of %2",
                                             CAT_VARIABLES, BTYPE_STACK, "index", "1"));
    blocks.back().fields.push_back({"list", FIELD_DROPDOWN, "my list", "my list", {"my list"}, 0, 0, 0,0,0,0});

    blocks.push_back(block_create_with_field("data_item_of_list", "item %1 of %2",
                                             CAT_VARIABLES, BTYPE_REPORTER, "index", "1"));
    blocks.back().fields.push_back({"list", FIELD_DROPDOWN, "my list", "my list", {"my list"}, 0, 0, 0,0,0,0});

    blocks.push_back(block_create_with_dropdown("data_length_of_list", "length of %1",
                                                CAT_VARIABLES, BTYPE_REPORTER, "list",
                                                {"my list"}, "my list"));

    return blocks;
}

static std::vector<Block> create_pen_blocks() {
    std::vector<Block> blocks;

    blocks.push_back(block_create("pen_clear", "erase all", CAT_PEN, BTYPE_STACK));
    blocks.push_back(block_create("pen_stamp", "stamp", CAT_PEN, BTYPE_STACK));
    blocks.push_back(block_create("pen_down", "pen down", CAT_PEN, BTYPE_STACK));
    blocks.push_back(block_create("pen_up", "pen up", CAT_PEN, BTYPE_STACK));

    blocks.push_back(block_create_with_field("pen_set_color", "set pen color to %1",
                                             CAT_PEN, BTYPE_STACK, "color", "#0000FF"));

    blocks.push_back(block_create_with_field("pen_change_hue", "change pen color by %1",
                                             CAT_PEN, BTYPE_STACK, "value", "10"));

    blocks.push_back(block_create_with_field("pen_set_hue", "set pen color to %1",
                                             CAT_PEN, BTYPE_STACK, "value", "0"));

    blocks.push_back(block_create_with_field("pen_change_shade", "change pen shade by %1",
                                             CAT_PEN, BTYPE_STACK, "value", "10"));

    blocks.push_back(block_create_with_field("pen_set_shade", "set pen shade to %1",
                                             CAT_PEN, BTYPE_STACK, "value", "50"));

    blocks.push_back(block_create_with_field("pen_change_size", "change pen size by %1",
                                             CAT_PEN, BTYPE_STACK, "value", "1"));

    blocks.push_back(block_create_with_field("pen_set_size", "set pen size to %1",
                                             CAT_PEN, BTYPE_STACK, "value", "1"));

    return blocks;
}

// ============================================================
//  پر کردن پالت
// ============================================================

void ui_populate_palette(UI& ui, int category_index) {
    ui.palette_items.clear();
    ui.palette_scroll.offset_y = 0;

    std::vector<Block> blocks;

    switch (category_index) {
        case CAT_MOTION:    blocks = create_motion_blocks(); break;
        case CAT_LOOKS:     blocks = create_looks_blocks(); break;
        case CAT_SOUND:     blocks = create_sound_blocks(); break;
        case CAT_EVENTS:    blocks = create_events_blocks(); break;
        case CAT_CONTROL:   blocks = create_control_blocks(); break;
        case CAT_SENSING:   blocks = create_sensing_blocks(); break;
        case CAT_OPERATORS: blocks = create_operators_blocks(); break;
        case CAT_VARIABLES: blocks = create_variables_blocks(); break;
        case CAT_PEN:       blocks = create_pen_blocks(); break;
        default: break;
    }

    int y = 10;
    for (auto& block : blocks) {
        PaletteItem item;
        item.proto = block;
        item.rect = {10, y, block.width, block.height};
        item.hovered = false;
        ui.palette_items.push_back(item);
        y += block.height + 8;
    }
}

// ============================================================
//  مقداردهی اولیه UI
// ============================================================

bool ui_init(UI& ui) {
    log_info("UI: Initializing SDL...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        log_error("UI: SDL_Init failed: " + std::string(SDL_GetError()));
        return false;
    }

    if (TTF_Init() < 0) {
        log_error("UI: TTF_Init failed: " + std::string(TTF_GetError()));
        SDL_Quit();
        return false;
    }

    ui.window = SDL_CreateWindow(
            "MyScratch - Visual Programming",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH, WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!ui.window) {
        log_error("UI: SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    ui.renderer = SDL_CreateRenderer(ui.window, -1,
                                     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ui.renderer) {
        log_error("UI: SDL_CreateRenderer failed: " + std::string(SDL_GetError()));
        SDL_DestroyWindow(ui.window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    SDL_SetRenderDrawBlendMode(ui.renderer, SDL_BLENDMODE_BLEND);

    // بارگذاری فونت
    ui.font = TTF_OpenFont("assets/fonts/default.ttf", 14);
    if (!ui.font) {
        ui.font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 14);
    }
    if (!ui.font) {
        log_error("UI: Failed to load font!");
        SDL_DestroyRenderer(ui.renderer);
        SDL_DestroyWindow(ui.window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    ui.font_small = TTF_OpenFont("assets/fonts/default.ttf", 11);
    if (!ui.font_small) {
        ui.font_small = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 11);
    }
    if (!ui.font_small) ui.font_small = ui.font;

    ui.font_bold = TTF_OpenFont("assets/fonts/default.ttf", 14);
    if (!ui.font_bold) {
        ui.font_bold = TTF_OpenFont("C:/Windows/Fonts/arialbd.ttf", 14);
    }
    if (!ui.font_bold) ui.font_bold = ui.font;

    // تنظیم Layout
    ui.rect_toolbar = {0, 0, WINDOW_WIDTH, TOOLBAR_HEIGHT};
    ui.rect_categories = {0, TOOLBAR_HEIGHT, CATEGORY_WIDTH, WINDOW_HEIGHT - TOOLBAR_HEIGHT};
    ui.rect_palette = {CATEGORY_WIDTH, TOOLBAR_HEIGHT, PALETTE_WIDTH, WINDOW_HEIGHT - TOOLBAR_HEIGHT};

    int script_x = CATEGORY_WIDTH + PALETTE_WIDTH;
    int stage_x = WINDOW_WIDTH - STAGE_WIDTH;
    int script_w = stage_x - script_x;

    ui.rect_scripts = {script_x, TOOLBAR_HEIGHT, script_w, WINDOW_HEIGHT - TOOLBAR_HEIGHT};
    ui.rect_stage = {stage_x, TOOLBAR_HEIGHT, STAGE_WIDTH, STAGE_HEIGHT};
    ui.rect_sprite_list = {stage_x, TOOLBAR_HEIGHT + STAGE_HEIGHT, STAGE_WIDTH, WINDOW_HEIGHT - TOOLBAR_HEIGHT - STAGE_HEIGHT};

    // ساخت دسته‌بندی‌ها
    ui.categories.clear();
    int cat_y = 5;
    for (int i = 0; i < CAT_COUNT; i++) {
        UICategory cat;
        cat.cat = (BlockCategory)i;
        cat.name = ui_get_category_name(cat.cat);
        cat.color = ui_get_category_color(cat.cat);
        cat.rect = {5, TOOLBAR_HEIGHT + cat_y, CATEGORY_WIDTH - 10, 28};
        cat.selected = (i == 0);
        cat.hovered = false;
        ui.categories.push_back(cat);
        cat_y += 32;
    }

    // دکمه‌های تولبار
    int btn_x = 10;

    ui.btn_green_flag.rect = {btn_x, 6, 40, 28};
    ui.btn_green_flag.label = "▶";
    ui.btn_green_flag.bg_color = {34, 139, 34, 255};
    ui.btn_green_flag.text_color = CLR_BTN_TEXT;
    btn_x += 50;

    ui.btn_stop.rect = {btn_x, 6, 40, 28};
    ui.btn_stop.label = "■";
    ui.btn_stop.bg_color = {178, 34, 34, 255};
    ui.btn_stop.text_color = CLR_BTN_TEXT;
    btn_x += 60;

    ui.btn_save.rect = {btn_x, 6, 50, 28};
    ui.btn_save.label = "Save";
    ui.btn_save.bg_color = {70, 70, 90, 255};
    ui.btn_save.text_color = CLR_BTN_TEXT;
    btn_x += 60;

    ui.btn_load.rect = {btn_x, 6, 50, 28};
    ui.btn_load.label = "Load";
    ui.btn_load.bg_color = {70, 70, 90, 255};
    ui.btn_load.text_color = CLR_BTN_TEXT;
    btn_x += 60;

    ui.btn_undo.rect = {btn_x, 6, 50, 28};
    ui.btn_undo.label = "Undo";
    ui.btn_undo.bg_color = {70, 70, 90, 255};
    ui.btn_undo.text_color = CLR_BTN_TEXT;
    btn_x += 60;

    ui.btn_redo.rect = {btn_x, 6, 50, 28};
    ui.btn_redo.label = "Redo";
    ui.btn_redo.bg_color = {70, 70, 90, 255};
    ui.btn_redo.text_color = CLR_BTN_TEXT;

    // پر کردن پالت با دسته‌بندی اول
    ui.selected_category = 0;
    ui_populate_palette(ui, 0);

    ui.initialized = true;
    ui.last_frame_time = SDL_GetTicks();

    log_info("UI: Initialization complete.");
    return true;
}

// ============================================================
//  خاتمه UI
// ============================================================

void ui_shutdown(UI& ui) {
    if (ui.font && ui.font != ui.font_small && ui.font != ui.font_bold) {
        TTF_CloseFont(ui.font);
    }
    if (ui.font_small && ui.font_small != ui.font) {
        TTF_CloseFont(ui.font_small);
    }
    if (ui.font_bold && ui.font_bold != ui.font) {
        TTF_CloseFont(ui.font_bold);
    }

    if (ui.renderer) SDL_DestroyRenderer(ui.renderer);
    if (ui.window) SDL_DestroyWindow(ui.window);

    TTF_Quit();
    SDL_Quit();

    ui.initialized = false;
    log_info("UI: Shutdown complete.");
}

// ============================================================
//  رسم بلوک
// ============================================================

void ui_render_block(UI& ui, const Block& block, int x, int y, bool ghost) {
    Color color = ui_get_category_color(block.category);
    if (block.custom_r || block.custom_g || block.custom_b) {
        color = {block.custom_r, block.custom_g, block.custom_b, 255};
    }

    if (ghost) {
        color.a = 128;
    }
    if (block.highlighted) {
        color.r = (Uint8)std::min(255, color.r + 40);
        color.g = (Uint8)std::min(255, color.g + 40);
        color.b = (Uint8)std::min(255, color.b + 40);
    }

    SDL_Rect rect = {x, y, block.width, block.height};

    // رسم بر اساس نوع بلوک
    switch (block.block_type) {
        case BTYPE_HAT: {
            // بلوک Hat با بالای گرد
            ui_draw_rounded_rect(ui.renderer, rect, color, 10);
            // اضافه کردن شکل Hat
            SDL_SetRenderDrawColor(ui.renderer, color.r, color.g, color.b, color.a);
            for (int i = 0; i < 12; i++) {
                SDL_RenderDrawLine(ui.renderer, x + 20 + i, y - 12 + i, x + block.width - 20 - i, y - 12 + i);
            }
            break;
        }

        case BTYPE_CAP: {
            // بلوک Cap با پایین گرد
            ui_draw_rounded_rect(ui.renderer, rect, color, 8);
            break;
        }

        case BTYPE_REPORTER: {
            // بلوک بیضی‌شکل
            SDL_Rect oval = {x, y, block.width, block.height};
            ui_draw_rounded_rect(ui.renderer, oval, color, block.height / 2);
            break;
        }

        case BTYPE_BOOLEAN: {
            // بلوک شش‌ضلعی
            int hw = block.height / 2;
            SDL_Point points[7] = {
                    {x + hw, y},
                    {x + block.width - hw, y},
                    {x + block.width, y + hw},
                    {x + block.width - hw, y + block.height},
                    {x + hw, y + block.height},
                    {x, y + hw},
                    {x + hw, y}
            };
            SDL_SetRenderDrawColor(ui.renderer, color.r, color.g, color.b, color.a);
            // پر کردن (ساده‌سازی)
            for (int row = 0; row < block.height; row++) {
                int left = x;
                int right = x + block.width;
                if (row < hw) {
                    left = x + (hw - row);
                    right = x + block.width - (hw - row);
                } else if (row > block.height - hw) {
                    int d = row - (block.height - hw);
                    left = x + d;
                    right = x + block.width - d;
                }
                SDL_RenderDrawLine(ui.renderer, left, y + row, right, y + row);
            }
            break;
        }

        case BTYPE_C_BLOCK: {
            // بلوک C شکل
            int top_h = 30;
            int bot_h = 20;
            int body_h = block.height - top_h - bot_h;
            int notch_w = 20;
            int notch_d = 15;

            // بالا
            SDL_Rect top_rect = {x, y, block.width, top_h};
            ui_draw_rounded_rect(ui.renderer, top_rect, color, 6);

            // سمت چپ
            SDL_Rect left_rect = {x, y + top_h, notch_w, body_h};
            SDL_SetRenderDrawColor(ui.renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(ui.renderer, &left_rect);

            // پایین
            SDL_Rect bot_rect = {x, y + block.height - bot_h, block.width, bot_h};
            ui_draw_rounded_rect(ui.renderer, bot_rect, color, 6);

            // زبانه ورودی
            SDL_Rect notch = {x + notch_w + 15, y + top_h, 30, 6};
            Color darker = {(Uint8)(color.r * 0.7f), (Uint8)(color.g * 0.7f), (Uint8)(color.b * 0.7f), color.a};
            ui_draw_rounded_rect(ui.renderer, notch, darker, 3);

            break;
        }

        default: {
            // بلوک Stack معمولی
            ui_draw_rounded_rect(ui.renderer, rect, color, 6);

            // زبانه بالا
            SDL_Rect notch_top = {x + 15, y, 30, 6};
            Color darker = {(Uint8)(color.r * 0.7f), (Uint8)(color.g * 0.7f), (Uint8)(color.b * 0.7f), color.a};
            ui_draw_rounded_rect(ui.renderer, notch_top, darker, 3);

            // زبانه پایین
            SDL_Rect notch_bot = {x + 15, y + block.height - 3, 30, 6};
            ui_draw_rounded_rect(ui.renderer, notch_bot, color, 3);
            break;
        }
    }

    // رسم متن بلوک
    std::string display_text = block.text;

    // جایگزینی %1, %2 با مقادیر فیلدها
    for (size_t i = 0; i < block.fields.size() && i < 9; i++) {
        std::string placeholder = "%" + std::to_string(i + 1);
        size_t pos = display_text.find(placeholder);
        if (pos != std::string::npos) {
            std::string val = block.fields[i].value;
            if (val.empty()) val = "[ ]";
            display_text.replace(pos, placeholder.length(), val);
        }
    }

    int text_x = x + 10;
    int text_y = y + (block.height - 16) / 2;

    if (block.block_type == BTYPE_C_BLOCK) {
        text_y = y + 7;
    }

    ui_draw_text(ui.renderer, ui.font, display_text, text_x, text_y, CLR_TEXT);
}

// ============================================================
//  رسم تولبار
// ============================================================

void ui_render_toolbar(UI& ui, AppState& state) {
    SDL_SetRenderDrawColor(ui.renderer, CLR_TOOLBAR.r, CLR_TOOLBAR.g, CLR_TOOLBAR.b, CLR_TOOLBAR.a);
    SDL_RenderFillRect(ui.renderer, &ui.rect_toolbar);

    // به‌روزرسانی رنگ دکمه‌ها بر اساس وضعیت
    if (state.running) {
        ui.btn_green_flag.bg_color = {20, 100, 20, 255};
        ui.btn_stop.bg_color = {178, 34, 34, 255};
    } else {
        ui.btn_green_flag.bg_color = {34, 139, 34, 255};
        ui.btn_stop.bg_color = {100, 30, 30, 255};
    }

    ui_draw_button(ui, ui.btn_green_flag);
    ui_draw_button(ui, ui.btn_stop);
    ui_draw_button(ui, ui.btn_save);
    ui_draw_button(ui, ui.btn_load);
    ui_draw_button(ui, ui.btn_undo);
    ui_draw_button(ui, ui.btn_redo);

    // نام پروژه
    std::string title = state.project_name;
    if (state.modified) title += " *";
    ui_draw_text(ui.renderer, ui.font_bold, title,
                 WINDOW_WIDTH / 2 - 50, 12, CLR_TEXT);

    // FPS
    std::ostringstream fps_str;
    fps_str << std::fixed << std::setprecision(1) << ui.fps << " FPS";
    ui_draw_text(ui.renderer, ui.font_small, fps_str.str(),
                 WINDOW_WIDTH - 80, 14, {180, 180, 180, 255});
}

// ============================================================
//  رسم دسته‌بندی‌ها
// ============================================================

void ui_render_categories(UI& ui) {
    SDL_SetRenderDrawColor(ui.renderer, CLR_CATEGORY_BG.r, CLR_CATEGORY_BG.g, CLR_CATEGORY_BG.b, CLR_CATEGORY_BG.a);
    SDL_RenderFillRect(ui.renderer, &ui.rect_categories);

    for (auto& cat : ui.categories) {
        Color bg = cat.color;
        if (cat.selected) {
            bg.r = (Uint8)std::min(255, bg.r + 50);
            bg.g = (Uint8)std::min(255, bg.g + 50);
            bg.b = (Uint8)std::min(255, bg.b + 50);
        } else if (cat.hovered) {
            bg.r = (Uint8)std::min(255, bg.r + 25);
            bg.g = (Uint8)std::min(255, bg.g + 25);
            bg.b = (Uint8)std::min(255, bg.b + 25);
        }

        ui_draw_rounded_rect(ui.renderer, cat.rect, bg, 5);
        ui_draw_text_centered(ui.renderer, ui.font_small, cat.name, cat.rect, CLR_TEXT);
    }
}

// ============================================================
//  رسم پالت
// ============================================================

void ui_render_palette(UI& ui) {
    SDL_SetRenderDrawColor(ui.renderer, CLR_PALETTE_BG.r, CLR_PALETTE_BG.g, CLR_PALETTE_BG.b, CLR_PALETTE_BG.a);
    SDL_RenderFillRect(ui.renderer, &ui.rect_palette);

    // فعال کردن clipping
    SDL_RenderSetClipRect(ui.renderer, &ui.rect_palette);

    for (auto& item : ui.palette_items) {
        int draw_x = ui.rect_palette.x + item.rect.x;
        int draw_y = ui.rect_palette.y + item.rect.y - ui.palette_scroll.offset_y;

        // بررسی دیده شدن
        if (draw_y + item.rect.h < ui.rect_palette.y) continue;
        if (draw_y > ui.rect_palette.y + ui.rect_palette.h) continue;

        // هایلایت هاور
        if (item.hovered) {
            SDL_Rect hover_rect = {draw_x - 2, draw_y - 2, item.rect.w + 4, item.rect.h + 4};
            ui_draw_rounded_rect_outline(ui.renderer, hover_rect, CLR_HIGHLIGHT, 6);
        }

        ui_render_block(ui, item.proto, draw_x, draw_y, false);
    }

    SDL_RenderSetClipRect(ui.renderer, nullptr);
}

// ============================================================
//  رسم ناحیه اسکریپت
// ============================================================

void ui_render_scripts(UI& ui, AppState& state) {
    SDL_SetRenderDrawColor(ui.renderer, CLR_SCRIPT_BG.r, CLR_SCRIPT_BG.g, CLR_SCRIPT_BG.b, CLR_SCRIPT_BG.a);
    SDL_RenderFillRect(ui.renderer, &ui.rect_scripts);

    // رسم شبکه پس‌زمینه
    SDL_SetRenderDrawColor(ui.renderer, CLR_GRID.r, CLR_GRID.g, CLR_GRID.b, CLR_GRID.a);
    int grid_size = 20;
    for (int gx = ui.rect_scripts.x; gx < ui.rect_scripts.x + ui.rect_scripts.w; gx += grid_size) {
        SDL_RenderDrawLine(ui.renderer, gx, ui.rect_scripts.y, gx, ui.rect_scripts.y + ui.rect_scripts.h);
    }
    for (int gy = ui.rect_scripts.y; gy < ui.rect_scripts.y + ui.rect_scripts.h; gy += grid_size) {
        SDL_RenderDrawLine(ui.renderer, ui.rect_scripts.x, gy, ui.rect_scripts.x + ui.rect_scripts.w, gy);
    }

    // Clipping
    SDL_RenderSetClipRect(ui.renderer, &ui.rect_scripts);

    // دریافت اسپرایت فعلی
    Sprite* sprite = app_state_current_sprite(state);
    if (sprite) {
        // رسم اسکریپت‌ها
        for (auto& script : sprite->scripts) {
            int base_x = ui.rect_scripts.x + script.x - ui.script_scroll.offset_x;
            int base_y = ui.rect_scripts.y + script.y - ui.script_scroll.offset_y;

            int y_offset = 0;
            for (int bid : script.block_ids) {
                Block* b = state.block_manager.get(bid);
                if (!b) continue;

                ui_render_block(ui, *b, base_x, base_y + y_offset, false);
                y_offset += b->height + 2;
            }
        }
    }

    // راهنمای Snap
    if (ui.drag.active && ui.drag.snap_valid) {
        SDL_Rect snap_rect = {
                ui.drag.mouse_x - 5,
                ui.drag.mouse_y - 5,
                10, 10
        };
        ui_draw_rounded_rect(ui.renderer, snap_rect, CLR_SNAP_GUIDE, 3);
    }

    SDL_RenderSetClipRect(ui.renderer, nullptr);
}

//============================================================
//رسم صحنه
// ============================================================

void ui_render_stage(UI& ui, AppState& state) {
    // پس‌زمینه سفید
    SDL_SetRenderDrawColor(ui.renderer, CLR_STAGE_BG.r, CLR_STAGE_BG.g, CLR_STAGE_BG.b, CLR_STAGE_BG.a);
    SDL_RenderFillRect(ui.renderer, &ui.rect_stage);

    // حاشیه
    SDL_SetRenderDrawColor(ui.renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(ui.renderer, &ui.rect_stage);

    // مرکز صحنه
    int cx = ui.rect_stage.x + ui.rect_stage.w / 2;
    int cy = ui.rect_stage.y + ui.rect_stage.h / 2;

    // === رسم خطوط قلم (مرحله ۳) ===
    // تبدیل مختصات (مرکز صفحه به عنوان نقطه 0,0 در نظر گرفته میشه)
    int center_x = ui.rect_stage.x + ui.rect_stage.w / 2;
    int center_y = ui.rect_stage.y + ui.rect_stage.h / 2;

    for (const auto& line : state.pen.lines) {
        SDL_SetRenderDrawColor(ui.renderer, line.r, line.g, line.b, line.a);

        // تبدیل مختصات اسکرچ به مختصات صفحه ویندوز (محور Y برعکسه)
        int px1 = center_x + (int)line.x1;
        int py1 = center_y - (int)line.y1;
        int px2 = center_x + (int)line.x2;
        int py2 = center_y - (int)line.y2;

        SDL_RenderDrawLine(ui.renderer, px1, py1, px2, py2);

        // یه حقه ساده برای ضخیم‌تر کردن خطوط
        if (line.thickness > 1.0f) {
            SDL_RenderDrawLine(ui.renderer, px1+1, py1, px2+1, py2);
            SDL_RenderDrawLine(ui.renderer, px1, py1+1, px2, py2+1);
        }
    }

    // رسم اسپرایت‌ها
    for (auto& sprite : state.sprites) {
        if (!sprite.visible) continue;

        // تبدیل مختصات Scratch به مختصات صفحه
        int sx = cx + (int)sprite.x;
        int sy = cy - (int)sprite.y;  // Y معکوس در Scratch

        // اندازه اسپرایت
        int size = (int)(40 * sprite.size / 100.0f);
        if (size < 5) size = 5;

        SDL_Rect sprite_rect = {
                sx - size / 2,
                sy - size / 2,
                size,
                size
        };

//        Color sprite_color = (app_state_current_sprite(state) && &sprite == app_state_current_sprite(state))
//                             ? CLR_SPRITE_SELECTED
//                             : CLR_SPRITE;
        const Sprite* cur = app_state_current_sprite(state);
        Color sprite_color = (cur != nullptr && sprite.id == cur->id)
                             ? CLR_SPRITE_SELECTED
                             : CLR_SPRITE;

        SDL_SetRenderDrawColor(
                ui.renderer,
                sprite_color.r,
                sprite_color.g,
                sprite_color.b,
                sprite_color.a
        );

        SDL_RenderFillRect(ui.renderer, &sprite_rect);
        // === اضافه شده برای مرحله ۱: رسم حباب گفتگو ===
        if (!sprite.speech_bubble.empty()) {
            Uint32 now = SDL_GetTicks();
            // بررسی اینکه زمان نمایش حباب تموم شده یا نه (0 یعنی نامحدود)
            if (sprite.speech_end_time == 0 || now < sprite.speech_end_time) {
                int tw = 0, th = 0;
                TTF_SizeUTF8(ui.font, sprite.speech_bubble.c_str(), &tw, &th);

                int padding = 8;
                SDL_Rect bubble_rect = {
                        sx + size / 2,             // کمی متمایل به راستِ اسپرایت
                        sy - size / 2 - th - 20,   // بالای سر اسپرایت
                        tw + padding * 2,
                        th + padding * 2
                };

                // رسم حاشیه و پس‌زمینه حباب
                Color borderColor = {180, 180, 180, 255};
                Color bgColor = {255, 255, 255, 255};
                ui_draw_rounded_rect(ui.renderer, bubble_rect, borderColor, 8);

                SDL_Rect inner = {bubble_rect.x + 1, bubble_rect.y + 1, bubble_rect.w - 2, bubble_rect.h - 2};
                ui_draw_rounded_rect(ui.renderer, inner, bgColor, 7);

                // رسم دمِ حباب
                SDL_SetRenderDrawColor(ui.renderer, 180, 180, 180, 255);
                if (sprite.speech_is_think) {
                    // برای فکر کردن (یه نقطه زیر حباب)
                    SDL_Rect dot = {sx + size / 2 + 5, sy - size / 2 - 8, 6, 6};
                    ui_draw_rounded_rect(ui.renderer, dot, borderColor, 3);
                } else {
                    // برای حرف زدن (یه خط ساده وصل به اسپرایت)
                    SDL_RenderDrawLine(ui.renderer, bubble_rect.x + 10, bubble_rect.y + bubble_rect.h, sx + size / 2, sy - size / 2);
                }

                // چاپ متن (رنگ متن تیره روی حباب سفید)
                ui_draw_text(ui.renderer, ui.font, sprite.speech_bubble, bubble_rect.x + padding, bubble_rect.y + padding, CLR_TEXT_DARK);
            } else {
                // اگر زمانش تموم شده بود، خالیش می‌کنیم تا دیگه رندر نشه
                sprite.speech_bubble = "";
            }
        }
        // === اضافه شده برای مرحله ۲: رسم نشانگر جهت (چرخش) ===
        // ۱. پیدا کردن نقطه مرکز مربع اسپرایت
        int cx = sx + size / 2;
        int cy = sy + size / 2;

        // ۲. تبدیل زاویه اسکرچ به رادیان (دقیقاً طبق منطق sprite_move_steps خودتون)
        float rad = (sprite.direction - 90.0f) * M_PI / 180.0f;

        // ۳. محاسبه مختصات سر خط (شعاع خط = نصف طول مربع)
        int line_length = size / 2;
        int end_x = cx + (int)(cos(rad) * line_length);
        int end_y = cy + (int)(sin(rad) * line_length);

        // ۴. رسم خط (عقربه) با رنگ مشکی
        SDL_SetRenderDrawColor(ui.renderer, 0, 0, 0, 255);
        SDL_RenderDrawLine(ui.renderer, cx, cy, end_x, end_y);

        // ضخیم‌تر کردن خط با رسم خطوط موازی (چون SDL_RenderDrawLine پیش‌فرض ۱ پیکسله)
        SDL_RenderDrawLine(ui.renderer, cx+1, cy, end_x+1, end_y);
        SDL_RenderDrawLine(ui.renderer, cx, cy+1, end_x, end_y+1);

        // ۵. رسم یه نقطه کوچیک تو مرکز برای قشنگی کار
        SDL_Rect center_dot = {cx - 3, cy - 3, 6, 6};
        SDL_RenderFillRect(ui.renderer, &center_dot);
    }
}


void ui_render_sprite_list(UI& ui, AppState& state) {
    SDL_SetRenderDrawColor(
            ui.renderer,
            CLR_SPRITE_LIST_BG.r,
            CLR_SPRITE_LIST_BG.g,
            CLR_SPRITE_LIST_BG.b,
            CLR_SPRITE_LIST_BG.a
    );
    SDL_RenderFillRect(ui.renderer, &ui.rect_sprite_list);

    int y = ui.rect_sprite_list.y + 10;
    for (size_t i = 0; i < state.sprites.size(); i++) {
        SDL_Rect r = {
                ui.rect_sprite_list.x + 10,
                y,
                ui.rect_sprite_list.w - 20,
                30
        };

        Color bg = (i == state.current_sprite_index)
                   ? CLR_SPRITE_SELECTED
                   : CLR_SPRITE;

        ui_draw_rounded_rect(ui.renderer, r, bg, 5);
        ui_draw_text(
                ui.renderer,
                ui.font,
                state.sprites[i].name,
                r.x + 8,
                r.y + 7,
                CLR_TEXT
        );

        y += 36;
    }
}

void ui_render(UI& ui, AppState& state) {
    if (!ui.initialized) return;

    ui_render_toolbar(ui, state);
    ui_render_categories(ui);
    ui_render_palette(ui);
    ui_render_scripts(ui, state);
    ui_render_stage(ui, state);
    ui_render_sprite_list(ui, state);

    // ---> رسم بلوکی که به موس چسبیده (در حال درگ) <---
    if (ui.drag.active) {
        int draw_x = ui.drag.mouse_x - ui.drag.offset_x;
        int draw_y = ui.drag.mouse_y - ui.drag.offset_y;
        ui_render_block(ui, ui.drag.dragged_block, draw_x, draw_y, false);
    }

    SDL_RenderPresent(ui.renderer);

    Uint32 now = SDL_GetTicks();
    float delta = (now - ui.last_frame_time) / 1000.0f;
    ui.last_frame_time = now;
    ui.fps = (delta > 0.0f) ? (1.0f / delta) : 0.0f;
}