#ifndef MOVEBLOCK_H
#define MOVEBLOCK_H

#include "Block.h"

struct Block* createMoveBlock(float x, float y);

void drawMoveBlock(SDL_Renderer*renderer, struct Block* block);

#endif // MOVEBLOCK_H