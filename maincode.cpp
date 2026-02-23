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


struct Block {
    int id = 0;
    SDL_Rect rect{};
    SDL_Color color = {60, 150, 220, 255};

    bool dragging = false;
    int offX = 0, offY = 0;

    string cmd = "MOVE";
    double a = 40.0;
    double b = 0.0;
    string s1 = "";
    string s2 = "";
    int i1 = 0;

    string opt = "";
    SDL_Color pickColor = {0, 255, 0, 255};


    string outSel = "last";
};

struct Workspace {
    SDL_Rect bounds{};
    vector<Block> blocks;
    int nextId = 1;

    void reset() {
        blocks.clear();
        nextId = 1;
    }

    void addBlock(int x, int y) {
        Block b;
        b.id = nextId++;
        b.rect = SDL_Rect{x, y, 240, 52};
        blocks.push_back(b);
    }

    int hitTest(int mx, int my) const {
        for (int i = (int)blocks.size() - 1; i >= 0; --i) {
            if (pointInRect(mx, my, blocks[i].rect)) return i;
        }
        return -1;
    }

    void bringToFront(int idx) {
        if (idx < 0 || idx >= (int)blocks.size()) return;
        Block b = blocks[idx];
        blocks.erase(blocks.begin() + idx);
        blocks.push_back(b);
    }

    void clampIntoBounds(Block& b) const {
        b.rect.x = clampT(b.rect.x, bounds.x, bounds.x + bounds.w - b.rect.w);
        b.rect.y = clampT(b.rect.y, bounds.y, bounds.y + bounds.h - b.rect.h);
    }

    void update(const InputState& in, Logger& log) {
        if (in.mousePressed) {
            int hit = hitTest(in.mx, in.my);
            if (hit != -1) {
                bringToFront(hit);
                Block& top = blocks.back();
                top.dragging = true;
                top.offX = in.mx - top.rect.x;
                top.offY = in.my - top.rect.y;
                log.log("DRAG", "Pick block id=" + to_string(top.id));
            }
        }

        if (in.mouseDown) {
            for (size_t i = 0; i < blocks.size(); i++) {
                Block& b = blocks[i];
                if (!b.dragging) continue;

                b.rect.x = in.mx - b.offX;
                b.rect.y = in.my - b.offY;
            }
        }

        if (in.mouseReleased) {
            for (int i = (int)blocks.size() - 1; i >= 0; --i) {
                Block& b = blocks[i];
                if (b.dragging) {
                    b.dragging = false;


                    if (!pointInRect(in.mx, in.my, bounds)) {
                        log.log("DRAG", "Deleted block id=" + to_string(b.id) + " (dropped outside)");
                        blocks.erase(blocks.begin() + i);
                    } else {
                        int snapDist = 35;
                        for (size_t j = 0; j < blocks.size(); ++j) {
                            if (i == (int)j) continue;
                            const Block& other = blocks[j];


                            if (abs(b.rect.x - other.rect.x) < snapDist &&
                                abs(b.rect.y - (other.rect.y + other.rect.h)) < snapDist) {
                                b.rect.x = other.rect.x;
                                b.rect.y = other.rect.y + other.rect.h;
                                break;
                            }
                            if (abs(b.rect.x - other.rect.x) < snapDist &&
                                abs((b.rect.y + b.rect.h) - other.rect.y) < snapDist) {
                                b.rect.x = other.rect.x;
                                b.rect.y = other.rect.y - b.rect.h;
                                break;
                            }
                        }
                        clampIntoBounds(b);
                        log.log("DRAG", "Drop block id=" + to_string(b.id));
                    }
                }
            }
            std::stable_sort(blocks.begin(), blocks.end(), [](const Block& a1, const Block& a2) {
                return a1.rect.y < a2.rect.y;
            });
        }
    }

    void draw(SDL_Renderer* r) const {
        for (size_t i = 0; i < blocks.size(); i++) {
            const Block& b = blocks[i];
            SDL_SetRenderDrawColor(r, b.color.r, b.color.g, b.color.b, b.color.a);
            SDL_RenderFillRect(r, &b.rect);
            SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
            SDL_RenderDrawRect(r, &b.rect);
        }
    }
};

struct PenSegment {
    double x1=0, y1=0, x2=0, y2=0;
    SDL_Color c{0,255,0,255};
    int size = 3;
};

struct PenStamp {
    double x=0, y=0;
    int costumeIndex = 0;
    double dirDeg = 90.0;
    double sizePct = 100.0;
};


struct TextureAsset {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
};

struct CloneSprite {
    double x = 0.0, y = 0.0;
    double dirDeg = 90.0;
    bool visible = true;
    double sizePct = 100.0;
};


static bool dirExists(const string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st{};
    return (stat(path.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
#endif
}

static bool makeDir(const string& path) {
    if (dirExists(path)) return true;
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0;
#else
    return mkdir(path.c_str(), 0777) == 0;
#endif
}

static string sanitizeStem(string s) {
    const string bad = "\\/:*?\"<>|";
    string out;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if ((unsigned char)c < 32) continue;
        if (bad.find(c) != string::npos) continue;
        out.push_back(c);
    }
    while (!out.empty() && (out.front() == ' ' || out.front() == '\t')) out.erase(out.begin());
    while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();

    if (out.size() >= 4) {
        string tail = out.substr(out.size() - 4);
        for (size_t i = 0; i < tail.size(); i++) tail[i] = (char)tolower(tail[i]);
        if (tail == ".txt") out = out.substr(0, out.size() - 4);
    }

    if (out.empty()) out = "untitled";
    return out;
}

static string getSaveDir() {
    const string dir = "SMemory";
    makeDir(dir);
    return dir;
}

static string buildSavePath(const string& stem) {
    string safe = sanitizeStem(stem);
#ifdef _WIN32
    return getSaveDir() + "\\" + safe + ".txt";
#else
    return getSaveDir() + "/" + safe + ".txt";
#endif
}

static vector<string> listSaveStems() {
    vector<string> out;
    string dir = getSaveDir();

#ifdef _WIN32
    string pattern = dir + "\\*.txt";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;

    do {
        string name = fd.cFileName;
        if (name.size() >= 4) {
            string tail = name.substr(name.size() - 4);
            for (size_t i = 0; i < tail.size(); i++) tail[i] = (char)tolower(tail[i]);
            if (tail == ".txt") out.push_back(name.substr(0, name.size() - 4));
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
#else
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (auto* ent = readdir(d)) {
        string name = ent->d_name;
        if (name.size() >= 4) {
            string tail = name.substr(name.size() - 4);
            for (size_t i = 0; i < tail.size(); i++) tail[i] = (char)tolower(tail[i]);
            if (tail == ".txt") out.push_back(name.substr(0, name.size() - 4));
        }
    }
    closedir(d);
#endif

    sort(out.begin(), out.end());
    out.erase(unique(out.begin(), out.end()), out.end());
    return out;
}


struct Value {
    bool isNum = true;
    double num = 0.0;
    string str;

    static Value Num(double v) { Value x; x.isNum = true; x.num = v; return x; }
    static Value Str(const string& s) { Value x; x.isNum = false; x.str = s; return x; }

    string toString() const {
        if (isNum) {
            ostringstream ss;
            ss << num;
            return ss.str();
        }
        return str;
    }

    bool truthy() const {
        if (isNum) return num != 0.0;
        return !str.empty();
    }
};

static double asNum(const Value& v) {
    if (v.isNum) return v.num;
    char* endp = nullptr;
    double x = strtod(v.str.c_str(), &endp);
    if (endp && endp != v.str.c_str()) return x;
    return 0.0;
}

// هر اسپرایت یه شخصیت مستقله با تصویر، موقعیت، کد و وضعیت اجرای خودش
struct Sprite {
    string name = "Sprite";
    TextureAsset icon; // تصویر کوچیک پایین صفحه

    // برای وقتی که کاربر اسپرایت رو با موس میکشه
    bool isDragging = false;
    double dragOffX = 0.0;
    double dragOffY = 0.0;

    // وضعیت اسپرایت قبل از Run — بعد از Stop برمیگرده به اینا
    double backupX = 0.0;
    double backupY = 0.0;
    double backupDirDeg = 90.0;
    bool backupVisible = true;
    double backupSizePct = 100.0;
    double backupColorEffect = 0.0;
    int backupCostumeIndex = 0;
    int backupZOrder = 0;

    // موقعیت، جهت، اندازه و ظاهر اسپرایت روی Stage
    double x = 0.0;
    double y = 0.0;
    double dirDeg = 90.0;
    bool visible = true;
    double sizePct = 100.0;
    double colorEffect = 0.0;
    int costumeIndex = 0;
    int zOrder = 0;

    // محیط کدنویسی اختصاصی این اسپرایت (بلاک‌هاش اینجاست)
    Workspace ws;

    // وضعیت اجرای کد — scriptPC میگه الان کدوم بلاک داره اجرا میشه
    bool scriptRunning = false;
    int scriptPC = 0;
    bool stepRequested = false;
    uint32_t waitUntilMs = 0;
    bool waiting = false;

    // جداول jump برای IF، REPEAT، FOREVER — موتور اجرا از اینا استفاده میکنه
    vector<int> jumpTo;
    vector<int> jumpElse;
    vector<int> jumpEnd;
    vector<int> loopEnd;
    vector<int> loopStart;
    vector<int> repeatCounter;

    // پشتیبانی از تابع‌های دلخواه (My Blocks)
    map<string, int> funcDefs; // اسم تابع → شماره بلاک شروعش
    vector<int> callStack;     // برای برگشتن بعد از اتمام تابع
    double currentParam = 0.0; // مقداری که به تابع پاس داده شده
};


// AppState حافظه مرکزی کل برنامه‌ست — همه چیز اینجا نگه داشته میشه
struct AppState {

    // لیست همه اسپرایت‌ها + اینکه الان کدوم انتخابه
    vector<Sprite> sprites;
    int activeSprite = 0;

    // دسترسی سریع به اسپرایت فعال
    Sprite& getActive() { return sprites[activeSprite]; }
    const Sprite& getActive() const { return sprites[activeSprite]; }

    bool quit = false;
    bool isPaused = false;

    // تنظیمات: سرعت اجرا، پنجره Settings
    bool settingsOpen = false;
    int  runSpeedMs = 30;
    bool drawActorWhenStopped = true;
    uint32_t nextStepAtMs = 0;

    // تصاویر لباس‌ها و پس‌زمینه‌ها
    vector<TextureAsset> costumes;
    vector<TextureAsset> backdrops;
    string actorIconFile = "costume0.bmp";

    InputState in;               // وضعیت موس و کیبورد این فریم
    Logger log = Logger("log.txt");

    bool isFullscreen = false;
    SDL_Rect stageBounds {};     // مختصات کادر سفید Stage
    vector<Button> buttons;      // دکمه‌های نوار بالا
    string baseTitle = "YKP Base (SDL2)";
    TTF_Font* uiFont = nullptr;

    // وضعیت دیالوگ‌های Save، Load و Rename
    bool saveDialogOpen = false;
    bool loadDialogOpen = false;
    bool renameDialogOpen = false;
    string renameInput = "";
    string saveNameInput = "";
    vector<string> saveList;
    int loadHoverIndex = -1;
    int loadScroll = 0;

    // وضعیت منوی Help و پنل لاگ‌ها
    bool helpMenuOpen = false;
    SDL_Rect helpButtonRect{0,0,0,0};
    bool showLogsPanel = false;
    int logsScroll = 0;
    bool debugStepMode = false;

    int backdropIndex = 0; // کدوم پس‌زمینه الان نمایش داده میشه

    // حباب Say/Think — متن، نوع و زمان نمایش
    string bubbleText = "";
    bool bubbleThink = false;
    uint32_t bubbleUntilMs = 0;

    // دیالوگ Ask — سوال، ورودی کاربر، آخرین جواب
    bool askDialogOpen = false;
    string askQuestion = "";
    string askInput = "";
    string lastAnswer = "";
    int askResumePC = -1;

    uint32_t timerStartMs = 0; // زمان شروع تایمر Scratch

    // تنظیمات صدا
    int soundVolume = 100;
    bool soundMuted = false;
    bool bgmReady = false;
    SDL_AudioDeviceID bgmDev = 0;
    SDL_AudioSpec bgmSpec{};
    Uint8* bgmBuf = nullptr; // buffer موسیقی که تبدیل فرمت شده
    Uint32 bgmLen = 0;
    Uint32 bgmPos = 0;
    int  musicVolume = 100;
    bool musicMuted  = false;
    string bgmFile = "bgm.wav";

    // متغیرهای Scratch و آخرین مقدار محاسبه شده
    map<string, Value> vars;
    map<string, bool> varVisible;
    Value lastValue = Value::Num(0.0);

    string lastBroadcast = ""; // آخرین broadcast ارسال شده

    // وضعیت کتابخانه افزونه‌ها و Pen
    bool extensionLibraryOpen = false;
    bool penExtensionEnabled = false;

    // palette بلاک‌ها در پنل چپ — paletteDirty یعنی باید دوباره ساخته بشه
    vector<Button> palette;
    bool paletteDirty = true;
    int paletteLastW = 0, paletteLastH = 0;
    bool paletteLastPenEnabled = false;
    int paletteScroll = 0;
    int paletteMaxScroll = 0;
    vector<pair<int,string>> paletteCats;

    // وضعیت قلم: پایین/بالا، رنگ (HSV+RGB)، ضخامت، خطوط و stampها
    bool penDown = false;
    double penHue = 120.0;
    double penSat = 100.0;
    double penBri = 100.0;
    SDL_Color penRGB{0,255,0,255};
    int penSize = 3;
    vector<PenSegment> penSegs;
    vector<PenStamp> penStamps;
    bool penColorPickerOpen = false;
    int  penColorPickerBlockIndex = -1;

    // وضعیت منوی Function I/O
    bool funcIOMenuOpen = false;
    int  funcIOMenuBlockIndex = -1;

    TextureAsset actorIcon; // آیکون پیش‌فرض اسپرایت

    // سیستم صدا برای جلوه‌های صوتی (WAV)
    bool audioReady = false;
    SDL_AudioDeviceID audioDev = 0;
    SDL_AudioSpec audioSpec{};
    uint32_t soundBusyUntilMs = 0;

    // لیست‌های Scratch
    map<string, vector<Value>> lists;
    map<string, bool> listVisible;

    // کلون‌های اسپرایت
    vector<CloneSprite> clones;
};


// دستگاه صوتی جداگانه برای موسیقی پس‌زمینه باز میکنه (44100Hz استریو)
static bool initBGMSystem(AppState& st) {
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 4096;
    want.callback = nullptr; // push mode — داده رو دستی میفرستیم

    SDL_AudioSpec have{};
    st.bgmDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!st.bgmDev) {
        st.bgmReady = false;
        st.log.warn(-1, "BGM", "OpenAudioDevice failed", SDL_GetError());
        return false;
    }

    st.bgmSpec = have;
    st.bgmReady = true;
    SDL_PauseAudioDevice(st.bgmDev, 0); // شروع پخش

    st.log.info(-1, "BGM", "BGM device ready",
                "freq=" + to_string(have.freq) + " ch=" + to_string((int)have.channels));
    return true;
}

// فایل WAV رو لود میکنه و اگه فرمتش با دستگاه فرق داشت تبدیلش میکنه
static bool loadBGM(AppState& st, const string& wavFile) {
    if (!st.bgmReady || !st.bgmDev) return false;

    // buffer قدیمی رو آزاد کن
    if (st.bgmBuf) { SDL_free(st.bgmBuf); st.bgmBuf = nullptr; }
    st.bgmLen = 0;
    st.bgmPos = 0;

    SDL_AudioSpec srcSpec{};
    Uint8* srcBuf = nullptr;
    Uint32 srcLen = 0;

    if (!SDL_LoadWAV(wavFile.c_str(), &srcSpec, &srcBuf, &srcLen)) {
        st.log.warn(-1, "BGM", "LoadWAV failed", wavFile + " err=" + SDL_GetError());
        return false;
    }

    Uint8* outBuf = srcBuf;
    Uint32 outLen = srcLen;

    // اگه فرمت WAV با دستگاه صوتی فرق داشت، تبدیلش کن
    if (srcSpec.format != st.bgmSpec.format ||
        srcSpec.channels != st.bgmSpec.channels ||
        srcSpec.freq != st.bgmSpec.freq) {

        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt,
                              srcSpec.format, srcSpec.channels, srcSpec.freq,
                              st.bgmSpec.format, st.bgmSpec.channels, st.bgmSpec.freq) < 0) {
            st.log.warn(-1, "BGM", "BuildAudioCVT failed", wavFile);
            SDL_FreeWAV(srcBuf);
            return false;
        }

        if (cvt.needed) {
            cvt.len = (int)srcLen;
            Uint8* cvtBuf = (Uint8*)SDL_malloc((size_t)cvt.len * (size_t)cvt.len_mult);
            if (!cvtBuf) {
                st.log.warn(-1, "BGM", "malloc failed", "cvt");
                SDL_FreeWAV(srcBuf);
                return false;
            }

            SDL_memcpy(cvtBuf, srcBuf, srcLen);
            cvt.buf = cvtBuf;

            if (SDL_ConvertAudio(&cvt) != 0) {
                st.log.warn(-1, "BGM", "ConvertAudio failed", wavFile);
                SDL_free(cvtBuf);
                SDL_FreeWAV(srcBuf);
                return false;
            }

            outBuf = cvtBuf;
            outLen = (Uint32)cvt.len_cvt;
            SDL_FreeWAV(srcBuf);
        }
    }

    // یه buffer یکدست میسازیم که همیشه با SDL_free آزاد بشه
    Uint8* finalBuf = (Uint8*)SDL_malloc(outLen);
    if (!finalBuf) {
        st.log.warn(-1, "BGM", "malloc failed", "finalBuf");
        if (outBuf == srcBuf) SDL_FreeWAV(srcBuf);
        else SDL_free(outBuf);
        return false;
    }
    SDL_memcpy(finalBuf, outBuf, outLen);

    if (outBuf == srcBuf) SDL_FreeWAV(srcBuf);
    else SDL_free(outBuf);

    st.bgmBuf = finalBuf;
    st.bgmLen = outLen;
    st.bgmPos = 0;

    st.log.info(-1, "BGM", "Loaded BGM", wavFile + " bytes=" + to_string(outLen));
    return true;
}

// دستگاه صوتی BGM رو میبنده و حافظه‌اش رو آزاد میکنه
static void shutdownBGMSystem(AppState& st) {
    if (st.bgmDev) {
        SDL_ClearQueuedAudio(st.bgmDev);
        SDL_CloseAudioDevice(st.bgmDev);
    }
    st.bgmDev = 0;
    st.bgmReady = false;

    if (st.bgmBuf) {
        SDL_free(st.bgmBuf);
        st.bgmBuf = nullptr;
    }
    st.bgmLen = 0;
    st.bgmPos = 0;
}

// queue صوتی BGM رو خالی میکنه — مثلاً وقتی صدا رو mute میکنیم
static void bgmClearQueue(AppState& st) {
    if (st.bgmReady && st.bgmDev) SDL_ClearQueuedAudio(st.bgmDev);
}