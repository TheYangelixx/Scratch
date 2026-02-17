#include <iostream>
#include <SDL2/SDL.h>

// اتصال به کدهای تیم (هدر فایل‌ها)
// نکته: اگر با روش CMake که گفتم عمل کردی، ../ را نیاز نداریم
#include "src\Workspace.h"
#include "../include/Sprite.h"
#include "../include/MoveBlock.h"

// ابعاد پنجره (Wide Screen برای جا شدن همه چیز)
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 800;

// مختصات خط جداکننده (مرز بین محیط کدنویسی و محیط اجرا)
const int STAGE_DIVIDER_X = 800;

int main(int argc, char* argv[]) {
    // 1. راه‌اندازی اولیه SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 2. ساخت پنجره و رندر
    SDL_Window* window = SDL_CreateWindow("CppScratch - Team Project",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_SHOWN);

    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 3. --- Initialization (ترکیب کارهای تیم) ---

    // الف) ساخت محیط کار (Workspace) - بخش یسنا
    // این شیء مسئول نگهداری تمام بلوک‌های روی صفحه است
    Workspace workspace;

    // تست: ساخت دستی یک بلوک حرکت و اضافه کردن به محیط
    // بلوک را در مختصات (100, 100) یعنی سمت چپ (محیط کدنویسی) می‌گذاریم
    workspace.addBlock(new MoveBlock(100, 100));

    // ب) ساخت کاراکتر (Sprite) - بخش الینا
    Sprite catSprite;
    // کاراکتر را می‌فرستیم سمت راست خط جداکننده (در محیط Stage) تا درست دیده شود
    catSprite.setPosition(950, 400);


    // 4. --- Main Loop (حلقه اصلی بازی) ---
    bool quit = false;
    SDL_Event e;

    while (!quit) {
        // مدیریت رویدادها (بستن پنجره)
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }

        // --- شروع نقاشی (Rendering) ---

        // 1. پاک کردن صفحه با رنگ سفید خالص
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        // 2. رسم خطوط محیط کاربری (UI Layout)
        // یک خط خاکستری عمودی که صفحه را دو قسمت می‌کند
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // رنگ طوسی
        SDL_RenderDrawLine(renderer, STAGE_DIVIDER_X, 0, STAGE_DIVIDER_X, SCREEN_HEIGHT);

        // (اختیاری) نوشتن متن یا کادر دور Stage برای زیبایی بیشتر
        SDL_Rect stageBorder = {STAGE_DIVIDER_X, 0, SCREEN_WIDTH - STAGE_DIVIDER_X, SCREEN_HEIGHT};
        SDL_RenderDrawRect(renderer, &stageBorder);

        // 3. رسم اشیاء
        // اول بلوک‌ها را می‌کشیم (باید در سمت چپ خط باشند)
        workspace.drawAll(renderer);

        // دوم کاراکتر را می‌کشیم (باید در سمت راست خط باشد)
        catSprite.draw(renderer);

        // 4. نمایش نهایی فریم
        SDL_RenderPresent(renderer);
    }

    // 5. پاکسازی حافظه
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}