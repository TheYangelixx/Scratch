#include "Workspace.h"

Workspace::Workspace() {
    // Vector initializes itself automatically
}

Workspace::~Workspace() {
    // Clean up memory: Delete all Block objects stored in the vector
    for (Block* block : blocks) {
        delete block;
    }
    blocks.clear();
}

void Workspace::addBlock(Block* b) {
    if (b != nullptr) {
        blocks.push_back(b);
    }
}

void Workspace::drawAll(SDL_Renderer* renderer) {
    // Iterate through the vector and draw each block
    for (Block* block : blocks) {
        if (block != nullptr) {
            block->draw(renderer);
        }
    }
}