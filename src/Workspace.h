#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <vector>
#include <SDL2/SDL.h> // Adjust based on your include path (e.g., <SDL.h>)
#include "..\include\Block.h"

struct Workspace {
    std::vector<Block*> blocks;
};

void initWorkspace(struct Workspace*ws);
void clearWorkspace(struct Workspace*ws);
void addBlockToWorkspace(struct Workspace*ws, struct Block*b);
void drawWorkspace(SDL_Renderer*renderer,struct Workspace*ws);


#endif // WORKSPACE_H