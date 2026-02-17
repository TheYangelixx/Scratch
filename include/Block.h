#ifndef BLOCK_H
#define BLOCK_H

#include <SDL2/SDL.h>
#include <string>

// انواع بلوک‌ها (برای تعیین رنگ و رفتار)
enum BlockType {
    BLOCK_MOVE, // آبی (حرکت)
    BLOCK_SAY   // بنفش (ظاهر) - فعلاً برای آینده
};

// ساختار داده برای یک بلوک (فقط متغیرها)
struct Block {
    float x;
    float y;
    int w;
    int h;
    BlockType type;
    std::string text; // متنی که روی بلوک نوشته می‌شود (مثلا "Move 10")
};

// --- توابع (Function Prototypes) ---

// مقداردهی اولیه یک بلوک جدید
void initBlock(Block* b, float x, float y, BlockType type, const std::string& text);

// رسم بلوک روی صفحه
void drawBlock(const Block* b, SDL_Renderer* renderer);

#endif // BLOCK_H