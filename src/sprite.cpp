#include "sprite.h"
#include "logger.h"
#include <bits/stdc++.h>
#include <SDL2/SDL.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_next_sprite_id = 1;

Sprite sprite_create(const std::string& name, int id) {
    Sprite s;
    s.id   = (id >= 0) ? id : g_next_sprite_id++;
    s.name = name;
    s.x    = 0.0f;
    s.y    = 0.0f;
    s.direction = 90.0f;
    s.size = 100.0f;
    s.visible = true;
    return s;
}

void sprite_move_steps(Sprite& s, float steps) {
    float rad = (s.direction - 90.0f) * (float)M_PI / 180.0f;
    s.x += steps * cosf(rad);
    s.y += steps * sinf(rad);
}

void sprite_go_to(Sprite& s, float x, float y) {
    s.x = x;
    s.y = y;
}

void sprite_glide_to(Sprite& s, float x, float y, float /*duration*/) {
    // ساده‌سازی: فوری انتقال (پیاده‌سازی واقعی نیاز به coroutine دارد)
    s.x = x;
    s.y = y;
}

void sprite_set_direction(Sprite& s, float degrees) {
    s.direction = fmodf(degrees, 360.0f);
    if (s.direction < 0) s.direction += 360.0f;
}

void sprite_turn_right(Sprite& s, float degrees) {
    sprite_set_direction(s, s.direction + degrees);
}

void sprite_turn_left(Sprite& s, float degrees) {
    sprite_set_direction(s, s.direction - degrees);
}

void sprite_point_towards(Sprite& s, float target_x, float target_y) {
    float dx = target_x - s.x;
    float dy = target_y - s.y;
    float angle = atan2f(dy, dx) * 180.0f / (float)M_PI + 90.0f;
    sprite_set_direction(s, angle);
}

void sprite_set_size(Sprite& s, float percent) {
    s.size = percent;
    if (s.size < 1.0f) s.size = 1.0f;
}

void sprite_change_size(Sprite& s, float delta) {
    sprite_set_size(s, s.size + delta);
}

void sprite_show(Sprite& s) { s.visible = true; }
void sprite_hide(Sprite& s) { s.visible = false; }

void sprite_next_costume(Sprite& s) {
    if (s.costumes.empty()) return;
    s.current_costume = (s.current_costume + 1) % (int)s.costumes.size();
}

void sprite_set_costume(Sprite& s, int index) {
    if (index >= 0 && index < (int)s.costumes.size()) {
        s.current_costume = index;
    }
}

void sprite_say(Sprite& s, const std::string& text, float seconds) {
    s.speech_bubble   = text;
    s.speech_is_think = false;
    if (seconds > 0) {
        s.speech_end_time = SDL_GetTicks() + (Uint32)(seconds * 1000);
    } else {
        s.speech_end_time = 0;
    }
}

void sprite_think(Sprite& s, const std::string& text, float seconds) {
    s.speech_bubble   = text;
    s.speech_is_think = true;
    if (seconds > 0) {
        s.speech_end_time = SDL_GetTicks() + (Uint32)(seconds * 1000);
    } else {
        s.speech_end_time = 0;
    }
}

void sprite_go_to_front(Sprite& s) {
    s.layer = 9999; // مقدار بزرگ = جلوتر
}

void sprite_go_to_back(Sprite& s) {
    s.layer = 0;
}

bool sprite_touching_edge(const Sprite& s, int stage_w, int stage_h) {
    float hw = stage_w / 2.0f;
    float hh = stage_h / 2.0f;
    return (s.x < -hw || s.x > hw || s.y < -hh || s.y > hh);
}

void sprite_bounce_on_edge(Sprite& s, int stage_w, int stage_h) {
    float hw = stage_w / 2.0f;
    float hh = stage_h / 2.0f;

    if (s.x > hw)  { s.x = hw;  s.direction = 180.0f - s.direction; }
    if (s.x < -hw) { s.x = -hw; s.direction = 180.0f - s.direction; }
    if (s.y > hh)  { s.y = hh;  s.direction = -s.direction; }
    if (s.y < -hh) { s.y = -hh; s.direction = -s.direction; }
}
