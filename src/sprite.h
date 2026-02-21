#ifndef MYSCRATCH_SPRITE_H
#define MYSCRATCH_SPRITE_H

#include <string>
#include <vector>
#include <SDL2/SDL.h>

// ============================================================
//  سبک چرخش
// ============================================================
enum RotationStyle {
    ROTATION_ALL_AROUND = 0,
    ROTATION_LEFT_RIGHT,
    ROTATION_DONT_ROTATE
};

// ============================================================
//  یک اسکریپت (زنجیره بلوک‌ها)
// ============================================================
struct Script {
    std::vector<int> block_ids;     // لیست id بلوک‌ها به ترتیب
    int x = 50, y = 50;            // موقعیت اسکریپت در ناحیه اسکریپت
    bool running = false;           // آیا در حال اجراست
    int  current_step = 0;          // مرحله فعلی اجرا
};

// ============================================================
//  لباس (Costume)
// ============================================================
struct Costume {
    std::string name;
    std::string file_path;
    SDL_Texture* texture = nullptr;
    int width  = 0;
    int height = 0;
    int center_x = 0;              // نقطه مرکز (pivot)
    int center_y = 0;
};

// ============================================================
//  صدا (Sound)
// ============================================================
struct SoundClip {
    std::string name;
    std::string file_path;
    // Mix_Chunk* chunk = nullptr;  // فعال کنید وقتی SDL_mixer دارید
};

// ============================================================
//  اسپرایت
// ============================================================
struct Sprite {
    int         id = -1;
    std::string name = "Sprite1";

    // موقعیت صحنه
    float x = 0.0f;
    float y = 0.0f;
    float direction = 90.0f;        // جهت (درجه، 0=بالا، 90=راست)
    float size = 100.0f;            // اندازه (درصد)

    // نمایش
    bool  visible = true;
    int   layer   = 0;              // لایه (ترتیب نمایش)
    RotationStyle rotation_style = ROTATION_ALL_AROUND;

    // لباس‌ها
    std::vector<Costume> costumes;
    int current_costume = 0;

    // صداها
    std::vector<SoundClip> sounds;
    float volume = 100.0f;

    // اسکریپت‌ها
    std::vector<Script> scripts;

    // قلم (Pen)
    bool  pen_down   = false;
    int   pen_color_r = 0, pen_color_g = 0, pen_color_b = 255;
    float pen_size   = 1.0f;

    // حباب گفتار
    std::string speech_bubble;
    bool        speech_is_think = false;
    Uint32      speech_end_time = 0;      // 0 = بدون محدودیت

    // متغیرهای محلی
    std::vector<std::pair<std::string, std::string>> local_variables;

    // Drag state در صحنه
    bool  draggable = false;
};

// ============================================================
//  توابع اسپرایت
// ============================================================

Sprite sprite_create(const std::string& name, int id);

// حرکت
void sprite_move_steps(Sprite& s, float steps);
void sprite_go_to(Sprite& s, float x, float y);
void sprite_glide_to(Sprite& s, float x, float y, float duration);
void sprite_set_direction(Sprite& s, float degrees);
void sprite_turn_right(Sprite& s, float degrees);
void sprite_turn_left(Sprite& s, float degrees);
void sprite_point_towards(Sprite& s, float target_x, float target_y);

// ظاهر
void sprite_set_size(Sprite& s, float percent);
void sprite_change_size(Sprite& s, float delta);
void sprite_show(Sprite& s);
void sprite_hide(Sprite& s);
void sprite_next_costume(Sprite& s);
void sprite_set_costume(Sprite& s, int index);
void sprite_say(Sprite& s, const std::string& text, float seconds = 0);
void sprite_think(Sprite& s, const std::string& text, float seconds = 0);

// لایه
void sprite_go_to_front(Sprite& s);
void sprite_go_to_back(Sprite& s);

// برخورد
bool sprite_touching_edge(const Sprite& s, int stage_w, int stage_h);
void sprite_bounce_on_edge(Sprite& s, int stage_w, int stage_h);

#endif // MYSCRATCH_SPRITE_H
