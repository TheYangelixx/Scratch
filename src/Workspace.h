#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <vector>
#include <SDL2/SDL.h> // Adjust based on your include path (e.g., <SDL.h>)
#include "..\include\Block.h"

class Workspace {
private:
    // A container to hold pointers to all blocks currently in the workspace
    std::vector<Block*> blocks;

public:
    Workspace();

    // Destructor to clean up memory
    ~Workspace();

    // Adds a new block to the workspace
    void addBlock(Block* b);

    // Iterates through all blocks and renders them
    void drawAll(SDL_Renderer* renderer);
};

#endif // WORKSPACE_H