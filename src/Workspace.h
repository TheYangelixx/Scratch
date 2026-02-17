#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <vector>
#include <SDL2/SDL.h>
// پکیج Block را ایمپورت می‌کنیم تا کامپایلر بداند Block چیست
#include "Block.h"

// به جای class از struct استفاده می‌کنیم
struct Workspace {
    // این لیست اشاره‌گرهایی به بلوک‌ها را نگه می‌دارد
    std::vector<Block*> blocks;
};

// --- تعریف توابع (Function Prototypes) ---

// تابع برای آماده‌سازی اولیه (جایگزین Constructor)
void initWorkspace(Workspace* ws);

// تابع برای پاکسازی حافظه (جایگزین Destructor)
void cleanUpWorkspace(Workspace* ws);

// تابع برای اضافه کردن بلوک جدید
void addBlockToWorkspace(Workspace* ws, Block* b);

// تابع برای رسم تمام بلوک‌ها
void drawAllBlocks(const Workspace* ws, SDL_Renderer* renderer);

#endif // WORKSPACE_H