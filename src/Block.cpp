#include "Block.h"

void initBlock(Block* b, float x, float y, BlockType type, const std::string& text) {
    if (b) {
        b->x = x;
        b->y = y;
        // ابعاد استاندارد بلوک‌ها
        b->w = 120; // کمی عریض‌تر برای جا شدن متن
        b->h = 40;

        b->type = type;
        b->text = text;
    }
}

void drawBlock(const Block* b, SDL_Renderer* renderer) {
    if (!b || !renderer) return;

    // 1. تنظیم رنگ بر اساس نوع بلوک
    switch (b->type) {
        case BLOCK_MOVE:
            // رنگ آبی مخصوص اسکرچ (Motion Blue)
            SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255);
            break;
        case BLOCK_SAY:
            // رنگ بنفش (Looks Purple)
            SDL_SetRenderDrawColor(renderer, 153, 102, 255, 255);
            break;
        default:
            // رنگ خاکستری پیش‌فرض
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            break;
    }

    // 2. تعریف مستطیل بلوک
    SDL_Rect rect;
    rect.x = static_cast<int>(b->x);
    rect.y = static_cast<int>(b->y);
    rect.w = b->w;
    rect.h = b->h;

    // 3. رسم بلوک توپر
    SDL_RenderFillRect(renderer, &rect);

    // (نکته: فعلاً متن روی بلوک را رسم نمی‌کنیم چون نیاز به فونت دارد، اما جای آن محفوظ است)
}