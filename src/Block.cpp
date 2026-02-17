#include "..\include\Block.h"
#include <iostream> // For debug logging if needed

void drawBlock(SDL_Renderer*renderer, Block*block) {
    if(block==nullptr) return;

    SDL_Rect rect={(int)block->x, (int)block->y, 100, 40};
    SDL_SetRenderDrawColor(renderer, block->color.r, block->color.g, block->color.b, 255);
    SDL_RenderFillRect(renderer, &rect);
}