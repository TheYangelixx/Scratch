#ifndef BLOCK_H
#define BLOCK_H

#include <SDL2/SDL.h>

static const int BLOCK_COMMAND=0;
static const int BLOCK_MOVE=1;
static const int BLOCK_HAT=2;
static const int BLOCK_CONTROL=3;

struct Block {
    float x;
    float y;
    int type;
    struct Block* next;
    SDL_Color color;
};

void drawBlock(SDL_Renderer*renderer, struct Block* block);


#endif