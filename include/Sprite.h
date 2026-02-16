#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include "Block.h" // Include Block to use Block* in the scripts vector

class Sprite {
private:
    // Coordinate system logic
    float x;
    float y;
    double direction; // In degrees

    // Logic: List of "Hat" blocks that start scripts for this sprite
    std::vector<Block*> scripts;

    // Appearance: List of loaded textures
    std::vector<SDL_Texture*> costumes;
    int currentCostumeIndex;

public:
    Sprite();
    ~Sprite(); // Destructor to clean up textures and blocks

    // Renders the current costume at (x, y) with rotation
    void draw(SDL_Renderer* renderer);

    // Adds a new script (starting with a Hat block) to this sprite
    void addScript(Block* block);

    // Updates the sprite's position
    void setPosition(float newX, float newY);
    
    // Getter for direction (likely needed for Move blocks later)
    double getDirection() const { return direction; }
};