#ifndef MOVEBLOCK_H
#define MOVEBLOCK_H

#include <SDL2/SDL.h>

// به جای کلاس از struct استفاده می‌کنیم
struct MoveBlock {
    int x;
    int y;
    int w;
    int h;
    SDL_Color color;
};

// تعریف تابع رسم که یک نمونه از struct را می‌گیرد
void drawMoveBlock(SDL_Renderer* renderer, struct MoveBlock block);

#endif