#include "Workspace.h"
#include "..\include\MoveBlock.h"

void initWorkspace(struct Workspace* ws) {
    // Vector initializes itself automatically
    if(ws!=nullptr) ws->blocks.clear();
}

void clearWorkspace(struct Workspace* ws) {
    if(ws==nullptr) return;
    for(Block*b : ws->blocks) {
        delete b;
    }
    ws->blocks.clear();
}

void addBlockToWorkspace(struct Workspace*ws, struct Block*b) {
    if(b!=nullptr && ws!=nullptr) {
        ws->blocks.push_back(b);
    }
}

void drawWorkspace(SDL_Renderer*renderer, struct Workspace*ws) {
    if (ws == nullptr || renderer == nullptr) return;

    for (Block *b: ws->blocks) {
        if (b != nullptr) {
            drawMoveBlock(renderer, b);
        }
    }
}