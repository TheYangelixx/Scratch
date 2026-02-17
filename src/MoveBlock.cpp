#include "..\include\MoveBlock.h"

void drawMoveBlock(SDL_Renderer* renderer, struct MoveBlock block) {
    // 1. Draw the Block Body (Blue Rectangle)
    // استفاده از مقادیر داخل struct (مثل block.x)
    SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255);

    SDL_Rect blockRect = { block.x, block.y, block.w, block.h };
    SDL_RenderFillRect(renderer, &blockRect);

    // 2. Draw a Border (Slightly darker blue)
    SDL_SetRenderDrawColor(renderer, 60, 120, 200, 255);
    SDL_RenderDrawRect(renderer, &blockRect);

    // 3. Draw "Text Placeholder" (White line/box)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // تنظیم مکان مستطیل سفید کوچک بر اساس موقعیت بلوک اصلی
    SDL_Rect textPlaceholder = { block.x + 10, block.y + 18, 80, 4 };
    SDL_RenderFillRect(renderer, &textPlaceholder);
}