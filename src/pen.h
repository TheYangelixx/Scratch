#ifndef PEN_H
#define PEN_H

#include <SDL2/SDL.h>
#include <vector>

/* نقطه قلم */
struct PenPoint {
    float x, y;
    int r, g, b, a;
    float size;
};

/* خط قلم */
struct PenLine {
    float x1, y1;
    float x2, y2;
    int r, g, b, a;
    float thickness;
};

/* تصویر Stamp */
struct StampImage {
    float x, y;
    float size;
    int costume_index;
    int sprite_id;
};

/* بوم قلم */
struct PenCanvas {
    std::vector<PenLine> lines;
    std::vector<StampImage> stamps;
    bool needs_redraw;
    SDL_Texture* canvas_texture;
    int canvas_width;
    int canvas_height;
    bool texture_valid;
};

/* توابع */
void pen_canvas_init(PenCanvas& pc, SDL_Renderer* renderer, int width, int height);
void pen_canvas_shutdown(PenCanvas& pc);
void pen_canvas_clear(PenCanvas& pc);
void pen_draw_line(PenCanvas& pc, float x1, float y1, float x2, float y2,
                   int r, int g, int b, int a, float thickness);
void pen_stamp(PenCanvas& pc, int sprite_id, float x, float y, float size, int costume_idx);
void pen_canvas_render(SDL_Renderer* renderer, const PenCanvas& pc,
                       int stage_x, int stage_y, int stage_w, int stage_h);
void pen_hsb_to_rgb(float hue, float saturation, float brightness,
                    int& out_r, int& out_g, int& out_b);
int pen_canvas_line_count(const PenCanvas& pc);

#endif /* PEN_H */
