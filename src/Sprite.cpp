#include "../include/Sprite.h"
#include <iostream>

// سازنده (Constructor): تنظیم مقادیر اولیه
Sprite::Sprite() {
    x = 0.0f;
    y = 0.0f;
    direction = 90.0;
    currentCostumeIndex = 0;
}

// مخرب (Destructor): پاکسازی حافظه (فعلاً خالی چون عکسی لود نکردیم)
Sprite::~Sprite() {
    // بعداً که عکس اضافه کردیم، اینجا باید SDL_DestroyTexture را صدا بزنیم
}

// تنظیم موقعیت جدید
void Sprite::setPosition(float newX, float newY) {
    x = newX;
    y = newY;
}

// اضافه کردن اسکریپت (کد) به اسپرایت
void Sprite::addScript(Block* block) {
    scripts.push_back(block);
}

// رسم کردن اسپرایت روی صفحه
void Sprite::draw(SDL_Renderer* renderer) {
    // 1. ذخیره رنگ فعلی رندر (برای اینکه رنگ بقیه چیزها خراب نشود)
    SDL_Color oldColor;
    SDL_GetRenderDrawColor(renderer, &oldColor.r, &oldColor.g, &oldColor.b, &oldColor.a);

    // 2. تنظیم رنگ به قرمز (Red)
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    // 3. تعریف مربع (مستطیل)
    SDL_Rect rect;
    rect.x = static_cast<int>(x); // تبدیل float به int برای رسم
    rect.y = static_cast<int>(y);
    rect.w = 50; // عرض 50 پیکسل
    rect.h = 50; // ارتفاع 50 پیکسل

    // 4. پر کردن مستطیل با رنگ قرمز
    SDL_RenderFillRect(renderer, &rect);

    // 5. برگرداندن رنگ به حالت قبلی
    SDL_SetRenderDrawColor(renderer, oldColor.r, oldColor.g, oldColor.b, oldColor.a);
}
