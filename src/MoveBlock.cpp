#include "..\include\MoveBlock.h"

Block* createMoveBlock(float x, float y) {
    Block* newBlock= new Block();
    newBlock->x=x;
    newBlock->y=y;
    newBlock->type=BLOCK_MOVE;
    newBlock->next=nullptr;

    newBlock->color={76, 151, 255, 255};
    return newBlock;
}

void drawMoveBlock(SDL_Renderer* renderer, Block*block) {
    if(block==nullptr) return;

    SDL_Rect blockRect={(int)block->x, (int)block->y, 100, 40};
    SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255);
    SDL_RenderFillRect(renderer, &blockRect);

    SDL_SetRenderDrawColor(renderer, 60, 120, 200, 255);
    SDL_RenderDrawRect(renderer, &blockRect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect textLine={(int)block->x+10, (int)block->y+18, 80, 4};
    SDL_RenderFillRect(renderer, &textLine);
}