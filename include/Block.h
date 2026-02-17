#pragma once

#include <SDL2/SDL.h>

// Enum to categorize block types for logic and rendering
enum class BlockType {
    Command,    // Stackable blocks (e.g., Move, Turn)
    Reporter,   // Value blocks (e.g., X Position, variable)
    Hat,        // Event starters (e.g., When Flag Clicked)
    Control     // C-shape blocks (e.g., Repeat, If)
};

class Block {
public:
    // Screen coordinates for rendering the block in the workspace
    float x;
    float y;

    // Pointer to the next block in the stack (Linked List structure)
    Block* next;

    // The category of this block
    BlockType type;

    // Constructor: Initializes pointers to nullptr to prevent crashes
    Block(BlockType t) : x(0), y(0), next(nullptr), type(t) {}

    // Virtual destructor is crucial for abstract base classes to prevent memory leaks
    virtual ~Block() {}

    // Pure virtual method: Every specific block must define its own logic
    virtual void execute() = 0;

    // Pure virtual method: Every block must know how to draw itself
    virtual void draw(SDL_Renderer* renderer) = 0;
};