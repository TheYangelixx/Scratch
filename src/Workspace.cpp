#include "Workspace.h"

// پیاده‌سازی تابع آماده‌سازی
void initWorkspace(Workspace* ws) {
    if (ws) {
        // وکتور به صورت خودکار ساخته می‌شود اما برای اطمینان آن را خالی می‌کنیم
        ws->blocks.clear();
    }
}

// پیاده‌سازی تابع پاکسازی (Destructor سابق)
void cleanUpWorkspace(Workspace* ws) {
    if (ws) {
        // آزادسازی حافظه تک تک بلوک‌ها
        for (Block* block : ws->blocks) {
            // چون Block ها را با new ساخته‌ایم، باید delete شوند
            delete block;
        }
        // خالی کردن لیست
        ws->blocks.clear();
    }
}

// پیاده‌سازی اضافه کردن بلوک
void addBlockToWorkspace(Workspace* ws, Block* b) {
    if (ws && b) {
        ws->blocks.push_back(b);
    }
}

// پیاده‌سازی رسم همه بلوک‌ها
void drawAllBlocks(const Workspace* ws, SDL_Renderer* renderer) {
    if (ws) {
        // حلقه روی تمام بلوک‌های موجود در لیست
        for (Block* block : ws->blocks) {
            if (block) {
                // نکته مهم: چون Block دیگر کلاس نیست، متد draw() ندارد.
                // باید تابع drawBlock که در Block.h تعریف کردیم را صدا بزنیم.
                drawBlock(block, renderer);
            }
        }
    }
}