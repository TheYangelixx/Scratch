#ifndef SPRITE_H
#define SPRITE_H

#include <SDL2/SDL.h>
#include <vector>
#include "Block.h" // Include Block to use Block* in the scripts vector

struct Sprite {
    float x;
    float y;
    double direction;

    SDL_Color color;

    std::vector<struct Block*> scripts;
    std::vector<SDL_Texture*> costumes;
    int currentCostumeIndex;
};

void initSprite(struct Sprite* sprite);
void drawSprite(SDL_Renderer*renderer, struct Sprite* sprite);
void setSpritePosition(struct Sprite* sprite, float newX, float newY);
void moveSprite(struct Sprite*sprite, float steps);

#endif