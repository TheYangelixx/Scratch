#include "..\include\MoveBlock.h"

MoveBlock::MoveBlock(int x, int y) : Block(x, y) {
}

void MoveBlock::draw(SDL_Renderer* renderer) {
    // 1. Draw the Block Body (Blue Rectangle)
    // Scratch Motion Blue approx: R:76, G:151, B:255
    SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255);
    
    SDL_Rect blockRect = { x, y, 100, 40 };
    SDL_RenderFillRect(renderer, &blockRect);

    // 2. Draw a Border (Optional, for better visibility)
    SDL_SetRenderDrawColor(renderer, 60, 120, 200, 255); // Slightly darker blue
    SDL_RenderDrawRect(renderer, &blockRect);

    // 3. Draw "Text Placeholder" (White line/box)
    // Since we aren't using SDL_ttf yet, we mimic text with a white strip
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White
    
    // Simulate "Move 10 Steps" text roughly centered
    SDL_Rect textPlaceholder = { x + 10, y + 18, 80, 4 }; 
    SDL_RenderFillRect(renderer, &textPlaceholder);
}