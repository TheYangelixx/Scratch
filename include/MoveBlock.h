#ifndef MOVEBLOCK_H
#define MOVEBLOCK_H

#include "Block.h"

class MoveBlock : public Block {
public:
    // Constructor passes coordinates to the base Block
    MoveBlock(int x, int y);

    // Override the draw method to render specific graphics
    void draw(SDL_Renderer* renderer) override;
};

#endif // MOVEBLOCK_H