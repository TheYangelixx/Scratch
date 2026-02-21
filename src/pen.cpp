#include "pen.h"
#include <cmath>
#include <algorithm>

void pen_canvas_init(PenCanvas& pc, SDL_Renderer* renderer, int width, int height) {
    pc.lines.clear();
    pc.stamps.clear();
    pc.needs_redraw = false;
    pc.canvas_width = width;
    pc.canvas_height = height;
    pc.texture_valid = false;
    pc.canvas_texture = nullptr;

    if (renderer) {
        pc.canvas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                              SDL_TEXTUREACCESS_TARGET, width, height);
        if (pc.canvas_texture) {
            SDL_SetTextureBlendMode(pc.canvas_texture, SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(renderer, pc.canvas_texture);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);
            SDL_SetRenderTarget(renderer, nullptr);
            pc.texture_valid = true;
        }
    }
}

void pen_canvas_shutdown(PenCanvas& pc) {
    if (pc.canvas_texture) {
        SDL_DestroyTexture(pc.canvas_texture);
        pc.canvas_texture = nullptr;
    }
    pc.texture_valid = false;
    pc.lines.clear();
    pc.stamps.clear();
}

void pen_canvas_clear(PenCanvas& pc) {
    pc.lines.clear();
    pc.stamps.clear();
    pc.needs_redraw = true;
}

void pen_draw_line(PenCanvas& pc, float x1, float y1, float x2, float y2,
                   int r, int g, int b, int a, float thickness) {
    PenLine line;
    line.x1 = x1;
    line.y1 = y1;
    line.x2 = x2;
    line.y2 = y2;
    line.r = r;
    line.g = g;
    line.b = b;
    line.a = a;
    line.thickness = thickness;
    pc.lines.push_back(line);
    pc.needs_redraw = true;
}

void pen_stamp(PenCanvas& pc, int sprite_id, float x, float y, float size, int costume_idx) {
    StampImage si;
    si.sprite_id = sprite_id;
    si.x = x;
    si.y = y;
    si.size = size;
    si.costume_index = costume_idx;
    pc.stamps.push_back(si);
    pc.needs_redraw = true;
}

static void draw_thick_line(SDL_Renderer* renderer, float x1, float y1, float x2, float y2,
                            float thickness) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);

    if (length < 0.001f) {
        SDL_Rect dot;
        dot.x = (int)(x1 - thickness / 2.0f);
        dot.y = (int)(y1 - thickness / 2.0f);
        dot.w = (int)thickness;
        dot.h = (int)thickness;
        if (dot.w < 1) dot.w = 1;
        if (dot.h < 1) dot.h = 1;
        SDL_RenderFillRect(renderer, &dot);
        return;
    }

    if (thickness <= 1.5f) {
        SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)x2, (int)y2);
        return;
    }

    float nx = -dy / length;
    float ny = dx / length;
    int half = (int)(thickness / 2.0f);

    for (int i = -half; i <= half; i++) {
        float ox = nx * (float)i;
        float oy = ny * (float)i;
        SDL_RenderDrawLine(renderer,
                           (int)(x1 + ox), (int)(y1 + oy),
                           (int)(x2 + ox), (int)(y2 + oy));
    }
}

void pen_canvas_render(SDL_Renderer* renderer, const PenCanvas& pc,
                       int stage_x, int stage_y, int stage_w, int stage_h) {
    float cx = stage_x + stage_w / 2.0f;
    float cy = stage_y + stage_h / 2.0f;

    SDL_Rect clip = {stage_x, stage_y, stage_w, stage_h};
    SDL_RenderSetClipRect(renderer, &clip);

    for (int i = 0; i < (int)pc.lines.size(); i++) {
        const PenLine& line = pc.lines[i];
        SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, line.a);

        float sx1 = cx + line.x1;
        float sy1 = cy - line.y1;
        float sx2 = cx + line.x2;
        float sy2 = cy - line.y2;

        draw_thick_line(renderer, sx1, sy1, sx2, sy2, line.thickness);
    }

    for (int i = 0; i < (int)pc.stamps.size(); i++) {
        const StampImage& si = pc.stamps[i];
        int sx = (int)(cx + si.x - si.size / 4.0f);
        int sy = (int)(cy - si.y - si.size / 4.0f);
        int sw = (int)(si.size / 2.0f);
        int sh = (int)(si.size / 2.0f);
        if (sw < 4) sw = 4;
        if (sh < 4) sh = 4;

        SDL_Rect stamp_rect = {sx, sy, sw, sh};
        SDL_SetRenderDrawColor(renderer, 100, 100, 200, 200);
        SDL_RenderFillRect(renderer, &stamp_rect);
        SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255);
        SDL_RenderDrawRect(renderer, &stamp_rect);
    }

    SDL_RenderSetClipRect(renderer, nullptr);
}

void pen_hsb_to_rgb(float hue, float saturation, float brightness,
                    int& out_r, int& out_g, int& out_b) {
    float h = std::fmod(hue, 360.0f);
    if (h < 0) h += 360.0f;
    float s = saturation / 100.0f;
    float v = brightness / 100.0f;

    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r1 = 0, g1 = 0, b1 = 0;
    if (h < 60)       { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
    else              { r1 = c; g1 = 0; b1 = x; }

    out_r = (int)((r1 + m) * 255.0f);
    out_g = (int)((g1 + m) * 255.0f);
    out_b = (int)((b1 + m) * 255.0f);

    if (out_r < 0) out_r = 0; if (out_r > 255) out_r = 255;
    if (out_g < 0) out_g = 0; if (out_g > 255) out_g = 255;
    if (out_b < 0) out_b = 0; if (out_b > 255) out_b = 255;
}

int pen_canvas_line_count(const PenCanvas& pc) {
    return (int)pc.lines.size();
}
