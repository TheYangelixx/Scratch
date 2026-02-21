#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
// TTF_Font forward declaration (SDL_ttf.h included in .cpp files only)
struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;
#include <string>
#include <vector>
#include <functional>

#include "block.h"

// ============================================================
//  ابعاد پنجره و نواحی
// ============================================================

constexpr int WINDOW_WIDTH      = 1280;
constexpr int WINDOW_HEIGHT     = 720;
constexpr int TOOLBAR_HEIGHT    = 40;
constexpr int CATEGORY_WIDTH    = 100;
constexpr int PALETTE_WIDTH     = 220;
constexpr int STAGE_WIDTH       = 480;
constexpr int STAGE_HEIGHT      = 360;

// ============================================================
//  ساختار رنگ
// ============================================================

struct Color {
    Uint8 r, g, b, a;
};

// ============================================================
//  ثابت‌های رنگ - کامل
// ============================================================

// رنگ‌های پس‌زمینه
constexpr Color CLR_TOOLBAR         = {50, 50, 60, 255};
constexpr Color CLR_CATEGORY_BG     = {40, 40, 50, 255};
constexpr Color CLR_PALETTE_BG      = {55, 55, 65, 255};
constexpr Color CLR_SCRIPT_BG       = {70, 70, 80, 255};
constexpr Color CLR_STAGE_BG        = {255, 255, 255, 255};

// رنگ‌های متن
constexpr Color CLR_TEXT            = {255, 255, 255, 255};
constexpr Color CLR_TEXT_DARK       = {40, 40, 40, 255};
constexpr Color CLR_BTN_TEXT        = {255, 255, 255, 255};

// رنگ‌های کمکی
constexpr Color CLR_GRID            = {60, 60, 70, 100};
constexpr Color CLR_HIGHLIGHT       = {100, 180, 255, 200};
constexpr Color CLR_SNAP_GUIDE      = {255, 200, 50, 255};

// رنگ‌های لیست اسپرایت
constexpr Color CLR_SPRITE_LIST_BG  = {45, 45, 55, 255};
constexpr Color CLR_SPRITE          = {80, 80, 100, 255};
constexpr Color CLR_SPRITE_SELECTED = {100, 150, 255, 255};

// رنگ‌های اضافی
constexpr Color CLR_DRAG_GHOST      = {255, 255, 255, 100};
constexpr Color CLR_ERROR           = {255, 80, 80, 255};
constexpr Color CLR_SUCCESS         = {80, 255, 80, 255};

// ============================================================
//  ساختار دکمه
// ============================================================

struct UIButton {
    SDL_Rect rect       = {0, 0, 80, 30};
    std::string label   = "";
    Color bg_color      = {70, 70, 90, 255};
    Color text_color    = CLR_BTN_TEXT;
    bool hovered        = false;
    bool pressed        = false;
    bool visible        = true;
    std::function<void()> on_click = nullptr;
};

// ============================================================
//  ساختار دسته‌بندی
// ============================================================

struct UICategory {
    BlockCategory cat;
    std::string name;
    Color color;
    SDL_Rect rect;
    bool selected   = false;
    bool hovered    = false;
};

// ============================================================
//  آیتم پالت
// ============================================================

struct PaletteItem {
    Block proto;
    SDL_Rect rect;
    bool hovered = false;
};

// ============================================================
//  وضعیت اسکرول
// ============================================================

struct ScrollState {
    int offset_x        = 0;
    int offset_y        = 0;
    int content_height  = 0;
    int content_width   = 0;
    bool dragging       = false;
};

// ============================================================
//  وضعیت درگ
// ============================================================

struct DragState {
    bool active             = false;
    bool from_palette       = false;
    Block dragged_block;
    int block_id            = -1;
    int offset_x            = 0;
    int offset_y            = 0;
    int mouse_x             = 0;
    int mouse_y             = 0;
    bool snap_valid         = false;
    int snap_script_idx     = -1;
    int snap_position       = -1;
};

// ============================================================
//  وضعیت ویرایش فیلد
// ============================================================

struct FieldEditState {
    bool active             = false;
    int block_id            = -1;
    int field_index         = -1;
    std::string buffer      = "";
    int cursor_pos          = 0;
    SDL_Rect rect           = {0, 0, 0, 0};
};

// ============================================================
//  ساختار اصلی UI
// ============================================================

struct UI {
    // پنجره و رندرر SDL
    SDL_Window* window      = nullptr;
    SDL_Renderer* renderer  = nullptr;

    // فونت‌ها
    TTF_Font* font          = nullptr;
    TTF_Font* font_small    = nullptr;
    TTF_Font* font_bold     = nullptr;

    // وضعیت
    bool initialized        = false;
    Uint32 last_frame_time  = 0;
    float fps               = 0.0f;

    // نواحی اصلی
    SDL_Rect rect_toolbar;
    SDL_Rect rect_categories;
    SDL_Rect rect_palette;
    SDL_Rect rect_scripts;
    SDL_Rect rect_stage;
    SDL_Rect rect_sprite_list;

    // دکمه‌های تولبار
    UIButton btn_green_flag;
    UIButton btn_stop;
    UIButton btn_save;
    UIButton btn_load;
    UIButton btn_undo;
    UIButton btn_redo;

    // دسته‌بندی‌ها
    std::vector<UICategory> categories;
    int selected_category = 0;

    // پالت
    std::vector<PaletteItem> palette_items;
    ScrollState palette_scroll;

    // اسکریپت
    ScrollState script_scroll;

    // درگ
    DragState drag;

    // ویرایش فیلد
    FieldEditState field_edit;
};

// ============================================================
//  پیش‌اعلان توابع
// ============================================================

struct AppState;  // forward declaration

void ui_handle_event(UI* ui, AppState* state, SDL_Event* event);
// توابع اصلی
bool ui_init(UI& ui);
void ui_shutdown(UI& ui);
void ui_render(UI& ui, AppState& state);

// توابع کمکی رنگ
SDL_Color ui_to_sdl_color(Color c);
Color ui_get_category_color(BlockCategory cat);
std::string ui_get_category_name(BlockCategory cat);

// توابع هندسی
bool ui_point_in_rect(int px, int py, SDL_Rect rect);

// توابع رسم
void ui_draw_rounded_rect(SDL_Renderer* renderer, SDL_Rect rect, Color color, int radius);
void ui_draw_rounded_rect_outline(SDL_Renderer* renderer, SDL_Rect rect, Color color, int radius);
void ui_draw_text(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, Color color);
void ui_draw_text_centered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Rect rect, Color color);
void ui_draw_button(UI& ui, UIButton& btn);

// توابع رندر بخش‌ها
void ui_render_toolbar(UI& ui, AppState& state);
void ui_render_categories(UI& ui);
void ui_render_palette(UI& ui);
void ui_render_scripts(UI& ui, AppState& state);
void ui_render_stage(UI& ui, AppState& state);
void ui_render_sprite_list(UI& ui, AppState& state);
void ui_render_block(UI& ui, const Block& block, int x, int y, bool ghost = false);

// پالت
void ui_populate_palette(UI& ui, int category_index);

#endif // UI_H
