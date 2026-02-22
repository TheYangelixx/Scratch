// maincode.cpp
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <SDL2/SDL_syswm.h>

//windows+open files

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <commdlg.h>

//linux+open file,...
#else
#include <sys/stat.h>
  #include <dirent.h>
#endif

using namespace std;

//window size
static const int WINDOW_W = 1200;
static const int WINDOW_H = 720;

//pannel and top bar size
static const int TOP_BAR_H = 52;
static const int LEFT_PANEL_W = 320; // was 280, now larger

//is mouse on the rect?
static bool pointInRect(int x, int y, const SDL_Rect& r) {
    return (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
}

//جلوگیری از خروج اسپرایت از محدوده
template <typename T>
static T clampT(T v, T lo, T hi) {
    return max(lo, min(v, hi));
}

//ERROR box
static void fatalBox(const string& title, const string& msg) {
    cerr << title << ": " << msg << "\n";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), msg.c_str(), nullptr);
}

//Information box
static void infoBox(const string& title, const string& msg) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), msg.c_str(), nullptr);
}

struct Logger {
    ofstream out;
    uint64_t cycle = 0;

    vector<string> mem;
    int maxMem = 3000;
    bool toConsole = true;

    explicit Logger(const string& path) {
        out.open(path.c_str(), ios::app);
        if (!out) cerr << "Failed to open log file: " << path << "\n";
    }

    void clear() { mem.clear(); }
    const vector<string>& lines() const { return mem; }

    string formatLine(const string& level, int blockIndex, const string& cmd,
                      const string& operation, const string& data) const {
        ostringstream ss;
        ss << "[Cycle:" << cycle << "] "
           << "[Line:" << blockIndex << "] "
           << "[CMD:" << cmd << "] "
           << "[Level:" << level << "] "
           << "[Op:" << operation << "] "
           << "[Data:" << data << "]";
        return ss.str();
    }

    void logLine(const string& level, int blockIndex, const string& cmd,
                 const string& operation, const string& data) {
        string line = formatLine(level, blockIndex, cmd, operation, data);

        mem.push_back(line);
        if ((int)mem.size() > maxMem) {
            mem.erase(mem.begin(), mem.begin() + ((int)mem.size() - maxMem));
        }

        if (toConsole) {
            if (level == "ERROR") cerr << line << "\n";
            else                 cout << line << "\n";
        }

        if (out) {
            out << line << "\n";
            out.flush();
        }
    }

    void log(const string& tag, const string& msg) {
        logLine("INFO", -1, tag, msg, "");
    }

    void info(int idx, const string& cmd, const string& op, const string& data) {
        logLine("INFO", idx, cmd, op, data);
    }
    void warn(int idx, const string& cmd, const string& op, const string& data) {
        logLine("WARNING", idx, cmd, op, data);
    }
    void error(int idx, const string& cmd, const string& op, const string& data) {
        logLine("ERROR", idx, cmd, op, data);
    }
};

struct InputState {
    int mx = 0, my = 0;
    bool mouseDown = false;
    bool mousePressed = false;
    bool mouseReleased = false;

    bool keyDown[SDL_NUM_SCANCODES];
    bool keyPressed[SDL_NUM_SCANCODES];

    InputState() {
        for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
            keyDown[i] = false;
            keyPressed[i] = false;
        }
    }

    void beginFrame() {
        mousePressed = false;
        mouseReleased = false;
        for (int i = 0; i < SDL_NUM_SCANCODES; i++) keyPressed[i] = false;
    }
};

// رسم متن در مختصات (x,y) با فونت و رنگ مشخص — نوشته‌های صفحه
static void renderText(SDL_Renderer* r, TTF_Font* font, const string& text, int x, int y, SDL_Color c) {
    if (!font || text.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), c);  // رندر نرم با anti-aliasing
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);  // انتقال به GPU
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_FreeSurface(s);  // حافظه RAM آزاد می‌شود
    if (!t) return;
    SDL_RenderCopy(r, t, nullptr, &dst);
    SDL_DestroyTexture(t);  // حافظه GPU آزاد می‌شود
}

// رسم متن دقیقاً وسط یک مستطیل — برای نوشته روی دکمه‌ها
static void renderTextCentered(SDL_Renderer* r, TTF_Font* font, const string& text, const SDL_Rect& box, int dy, SDL_Color c) {
    if (!font || text.empty()) return;
    int tw = 0, th = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &tw, &th) != 0) return;  // اندازه‌گیری متن
    int x = box.x + (box.w - tw) / 2;   // محاسبه مرکز افقی
    int y = box.y + (box.h - th) / 2 + dy;  // محاسبه مرکز عمودی + offset اختیاری
    renderText(r, font, text, x, y, c);
}

// جستجو بین فونت‌های سیستمی و لود اولین فونتی که پیدا شود
static TTF_Font* loadUIFont(int pt) {
#ifdef _WIN32
    const char* candidates[] = {
            "C:\\Windows\\Fonts\\consola.ttf",  // Consolas — مونواسپیس
            "C:\\Windows\\Fonts\\arial.ttf",
            "C:\\Windows\\Fonts\\tahoma.ttf"
    };
#else
    const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    };
#endif
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        TTF_Font* f = TTF_OpenFont(candidates[i], pt);
        if (f) return f;  // اولین فونت موجود را برمی‌گرداند
    }
    TTF_Font* f2 = TTF_OpenFont("font.ttf", pt);  // fallback: فایل کنار exe
    if (f2) return f2;
    return nullptr;  // هیچ فونتی پیدا نشد
}

// ساختار یک دکمه کامل: موقعیت، متن، میانبر، حالت hover/click و رویداد کلیک
struct Button {
    SDL_Rect rect{};
    string text;
    string sub;              // متن کوچک‌تر زیر (مثلاً "F5" برای Run)
    function<void()> onClick;

    bool hovered = false;    // موس روی دکمه است؟
    bool down = false;       // دکمه فشرده شده؟

    // هر فریم صدا می‌شود — وضعیت hover و کلیک را آپدیت می‌کند
    void update(const InputState& in) {
        hovered = pointInRect(in.mx, in.my, rect);
        if (hovered && in.mousePressed) down = true;
        if (down && in.mouseReleased) {
            down = false;
            if (hovered && onClick) onClick();  // فقط اگر هنوز روی دکمه بودیم
        }
        if (!in.mouseDown) down = false;  // جلوگیری از bug اگر موس از دکمه خارج شد
    }

    // رسم دکمه با سه حالت رنگی: عادی / hover / فشرده
    void draw(SDL_Renderer* r, TTF_Font* font) const {
        SDL_Color bg = {70, 70, 70, 255};
        if (down) bg = SDL_Color{120, 120, 120, 255};        // فشرده: روشن‌تر
        else if (hovered) bg = SDL_Color{90, 90, 90, 255};  // hover: کمی روشن‌تر

        SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(r, &rect);    // پر کردن داخل دکمه
        SDL_SetRenderDrawColor(r, 15, 15, 15, 255);
        SDL_RenderDrawRect(r, &rect);    // رسم کادر دور دکمه

        SDL_Color fg = {235, 235, 235, 255};
        renderTextCentered(r, font, text, rect, sub.empty() ? 0 : -7, fg);  // متن اصلی
        if (!sub.empty()) renderTextCentered(r, font, sub, rect, +10, fg);  // میانبر پایین
    }
};