// maincode.cpp
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <map>

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <direct.h>
#else
  #include <sys/stat.h>
  #include <dirent.h>
#endif

using namespace std;

// =========================
// Config
// =========================
static const int WINDOW_W = 1200;
static const int WINDOW_H = 720;

static const int TOP_BAR_H = 52;
static const int LEFT_PANEL_W = 320; // was 280, now larger

static bool pointInRect(int x, int y, const SDL_Rect& r) {
    return (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
}

template <typename T>
static T clampT(T v, T lo, T hi) {
    return max(lo, min(v, hi));
}

static void fatalBox(const string& title, const string& msg) {
    cerr << title << ": " << msg << "\n";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), msg.c_str(), nullptr);
}

static void infoBox(const string& title, const string& msg) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), msg.c_str(), nullptr);
}

// =========================
// Logger (Central)
// =========================
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

// =========================
// Input
// =========================
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

// =========================
// Text rendering (SDL_ttf)
// =========================
static void renderText(SDL_Renderer* r, TTF_Font* font, const string& text, int x, int y, SDL_Color c) {
    if (!font || text.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), c);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_FreeSurface(s);
    if (!t) return;
    SDL_RenderCopy(r, t, nullptr, &dst);
    SDL_DestroyTexture(t);
}

static void renderTextCentered(SDL_Renderer* r, TTF_Font* font, const string& text, const SDL_Rect& box, int dy, SDL_Color c) {
    if (!font || text.empty()) return;
    int tw = 0, th = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &tw, &th) != 0) return;
    int x = box.x + (box.w - tw) / 2;
    int y = box.y + (box.h - th) / 2 + dy;
    renderText(r, font, text, x, y, c);
}

static TTF_Font* loadUIFont(int pt) {
#ifdef _WIN32
    const char* candidates[] = {
        "C:\\Windows\\Fonts\\consola.ttf",
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
        if (f) return f;
    }

    TTF_Font* f2 = TTF_OpenFont("font.ttf", pt);
    if (f2) return f2;

    return nullptr;
}

// =========================
// UI Button
// =========================
struct Button {
    SDL_Rect rect{};
    string text;
    string sub;
    function<void()> onClick;

    bool hovered = false;
    bool down = false;

    void update(const InputState& in) {
        hovered = pointInRect(in.mx, in.my, rect);

        if (hovered && in.mousePressed) down = true;

        if (down && in.mouseReleased) {
            down = false;
            if (hovered && onClick) onClick();
        }

        if (!in.mouseDown) down = false;
    }

    void draw(SDL_Renderer* r, TTF_Font* font) const {
        SDL_Color bg = {70, 70, 70, 255};
        if (down) bg = SDL_Color{120, 120, 120, 255};
        else if (hovered) bg = SDL_Color{90, 90, 90, 255};

        SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(r, &rect);

        SDL_SetRenderDrawColor(r, 15, 15, 15, 255);
        SDL_RenderDrawRect(r, &rect);

        SDL_Color fg = {235, 235, 235, 255};
        renderTextCentered(r, font, text, rect, sub.empty() ? 0 : -7, fg);
        if (!sub.empty()) renderTextCentered(r, font, sub, rect, +10, fg);
    }
};

// =========================
// Workspace Blocks (Drag & Drop)
// =========================
struct Block {
    int id = 0;
    SDL_Rect rect{};
    SDL_Color color = {60, 150, 220, 255};

    bool dragging = false;
    int offX = 0, offY = 0;

    // --- Script/Interpreter minimal fields
    string cmd = "MOVE";   // extended commands in section 4
    double a = 40.0;       // numeric param A
    double b = 0.0;        // numeric param B
    string s1 = "";        // string param
    string s2 = "";        // string param 2
    int i1 = 0;            // int param

    // --- Extension: Pen extra params
    string opt = "";                 // dropdown: "COLOR", "SAT", "BRI"
    SDL_Color pickColor = {0, 255, 0, 255}; // for PEN_SET_COLOR block

    // --- Section 5: Function I/O
    // cmd = "FUNC_APPLY"
    // s1 = function name
    // inSel/outSel: selected input/output sources
    string inSel = "last";
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
                int beforeX = b.rect.x;
                int beforeY = b.rect.y;

                b.rect.x = in.mx - b.offX;
                b.rect.y = in.my - b.offY;
                clampIntoBounds(b);

                if (b.rect.x != beforeX || b.rect.y != beforeY) {
                    log.warn((int)i, "DRAG", "Block clamped to bounds",
                             "id=" + to_string(b.id) +
                             " (" + to_string(beforeX) + "," + to_string(beforeY) + ")->(" +
                             to_string(b.rect.x) + "," + to_string(b.rect.y) + ")");
                }
            }
        }

        if (in.mouseReleased) {
            for (size_t i = 0; i < blocks.size(); i++) {
                Block& b = blocks[i];
                if (b.dragging) {
                    b.dragging = false;
                    log.log("DRAG", "Drop block id=" + to_string(b.id));
                }
            }
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

// =========================
// Extension: Pen (Data)
// =========================
struct PenSegment {
    double x1=0, y1=0, x2=0, y2=0;
    SDL_Color c{0,255,0,255};
    int size = 3;
};

struct PenStamp {
    double x=0, y=0;
    int size = 12;
    SDL_Color fill{240,240,240,255};
    SDL_Color outline{10,10,10,255};
};

// =========================
// Section 6: Assets (Actor Icon) + Section 9: Clones (Data)
// =========================
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

// =========================
// Save & Load (SMemory folder)
// =========================
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

static bool saveProjectNamed(const string& saveStem, const Workspace& ws, Logger& log) {
    const string path = buildSavePath(saveStem);
    ofstream f(path.c_str());
    if (!f) {
        log.log("SAVE", "Cannot open: " + path);
        return false;
    }

    f << "# YKP_SAVE_V1\n";
    f << "BLOCKS " << ws.blocks.size() << "\n";
    for (size_t i = 0; i < ws.blocks.size(); i++) {
        const Block& b = ws.blocks[i];
        f << "BLOCK "
          << b.id << " "
          << b.rect.x << " " << b.rect.y << " "
          << b.rect.w << " " << b.rect.h << " "
          << (int)b.color.r << " " << (int)b.color.g << " " << (int)b.color.b
          << "\n";
    }

    log.log("SAVE", "Saved: " + path);
    return true;
}

static bool loadProjectNamed(const string& saveStem, Workspace& ws, Logger& log) {
    const string path = buildSavePath(saveStem);
    ifstream f(path.c_str());
    if (!f) {
        log.log("LOAD", "Cannot open: " + path);
        return false;
    }

    ws.reset();
    string line;
    int maxId = 0;

    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        string tag;
        ss >> tag;

        if (tag == "BLOCK") {
            Block b;
            int r=60,g=150,bl=220;
            ss >> b.id >> b.rect.x >> b.rect.y >> b.rect.w >> b.rect.h >> r >> g >> bl;
            b.color = SDL_Color{(Uint8)r, (Uint8)g, (Uint8)bl, 255};
            maxId = max(maxId, b.id);
            ws.blocks.push_back(b);
        }
    }

    ws.nextId = maxId + 1;
    log.log("LOAD", "Loaded: " + path);
    return true;
}

// =========================
// Value (for Operators/Variables) - minimal dynamic typing
// =========================
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

// =========================
// App State
// =========================
struct AppState {
    bool quit = false;
    // --- Yield/Wait state
    uint32_t waitUntilMs = 0;
    bool waiting = false;
    // ===== Settings Menu (NEW) =====
    bool settingsOpen = false;
    int  runSpeedMs = 30;        // delay بین اجرای هر بلاک (برای دیده شدن حرکت)
    bool drawActorWhenStopped = true;

    // زمان‌بندی runner (NEW)
    uint32_t nextStepAtMs = 0;

    // ===== Assets: use costumes/backdrops (NEW) =====
    vector<TextureAsset> costumes;
    vector<TextureAsset> backdrops;

    // بجای actor.bmp از costume0.bmp استفاده کن
    string actorIconFile = "costume0.bmp";

    InputState in;
    Logger log = Logger("log.txt");

    Workspace ws;
    vector<Button> buttons;

    string baseTitle = "YKP Base (SDL2)";

    TTF_Font* uiFont = nullptr;

    bool saveDialogOpen = false;
    bool loadDialogOpen = false;

    string saveNameInput = "";
    vector<string> saveList;
    int loadHoverIndex = -1;
    int loadScroll = 0;

    bool helpMenuOpen = false;
    SDL_Rect helpButtonRect{0,0,0,0};
    bool showLogsPanel = false;
    int logsScroll = 0;

    bool debugStepMode = false;

    // --- Runner
    bool scriptRunning = false;
    int scriptPC = 0;
    bool stepRequested = false;

    // Actor state (Section 4 base props)
    double actorX = 0.0;
    double actorY = 0.0;
    double actorDirDeg = 90.0;
    bool actorVisible = true;
    double actorSizePct = 100.0;
    double lookColorEffect = 0.0;
    int costumeIndex = 0;
    int backdropIndex = 0;

    // Looks speech bubble (minimal)
    string bubbleText = "";
    bool bubbleThink = false;
    uint32_t bubbleUntilMs = 0;

    // Sensing: ask/answer (minimal modal)
    bool askDialogOpen = false;
    string askQuestion = "";
    string askInput = "";
    string lastAnswer = "";
    int askResumePC = -1;

    // Sensing: timer
    uint32_t timerStartMs = 0;

    // Sound (minimal state; no real audio)
    int soundVolume = 100;
    bool soundMuted = false;
    bool bgmReady = false;
    SDL_AudioDeviceID bgmDev = 0;
    SDL_AudioSpec bgmSpec{};

    Uint8* bgmBuf = nullptr;   // converted buffer (in bgmSpec format)
    Uint32 bgmLen = 0;
    Uint32 bgmPos = 0;

    int  musicVolume = 100;    // 0..100 (BGM only)
    bool musicMuted  = false;

    // optional: file name
    string bgmFile = "bgm.wav";

    // Variables
    map<string, Value> vars;
    map<string, bool> varVisible;

    // Script last eval value
    Value lastValue = Value::Num(0.0);

    // Control pre-scan jumps
    vector<int> jumpTo;
    vector<int> jumpElse;
    vector<int> jumpEnd;
    vector<int> loopEnd;
    vector<int> loopStart;
    vector<int> repeatCounter;

    // Events minimal
    string lastBroadcast = "";

    // =========================
    // Extension: Library + Pen
    // =========================
    bool extensionLibraryOpen = false;
    bool penExtensionEnabled = false;

    vector<Button> palette;
    bool paletteDirty = true;
    int paletteLastW = 0, paletteLastH = 0;
    bool paletteLastPenEnabled = false;

    // Palette scroll + category labels
    int paletteScroll = 0;
    int paletteMaxScroll = 0;
    vector<pair<int,string>> paletteCats;

    // Pen state + persistent drawings
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

    // =========================
    // Section 5: Function I/O Menu
    // =========================
    bool funcIOMenuOpen = false;
    int  funcIOMenuBlockIndex = -1;

    // =========================
    // Section 6: Actor Icon (BMP next to maincode.cpp / exe)
    // =========================
    TextureAsset actorIcon;

    // =========================
    // Section 7: Audio (WAV via SDL_QueueAudio) - minimal
    // =========================
    bool audioReady = false;
    SDL_AudioDeviceID audioDev = 0;
    SDL_AudioSpec audioSpec{};
    uint32_t soundBusyUntilMs = 0;

    // =========================
    // Section 8: Lists (minimal)
    // =========================
    map<string, vector<Value>> lists;
    map<string, bool> listVisible;

    // =========================
    // Section 9: Clones (minimal)
    // =========================
    vector<CloneSprite> clones;
};

// =========================
// settings
// =========================
// =========================
// NEW: BGM helpers (looping WAV on separate device)
// =========================
static bool initBGMSystem(AppState& st) {
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 4096;
    want.callback = nullptr;

    SDL_AudioSpec have{};
    st.bgmDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!st.bgmDev) {
        st.bgmReady = false;
        st.log.warn(-1, "BGM", "OpenAudioDevice failed", SDL_GetError());
        return false;
    }

    st.bgmSpec = have;
    st.bgmReady = true;
    SDL_PauseAudioDevice(st.bgmDev, 0);

    st.log.info(-1, "BGM", "BGM device ready",
                "freq=" + to_string(have.freq) + " ch=" + to_string((int)have.channels));
    return true;
}

static bool loadBGM(AppState& st, const string& wavFile) {
    if (!st.bgmReady || !st.bgmDev) return false;

    // free old
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

    // Convert to bgmSpec if needed
    Uint8* outBuf = srcBuf;
    Uint32 outLen = srcLen;

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

            SDL_FreeWAV(srcBuf); // original freed; we keep converted
        } else {
            // no conversion needed actually
        }
    }

    // If we didn't convert, outBuf == srcBuf must be freed with SDL_FreeWAV later.
    // For simplicity, copy into SDL_malloc buffer so we always SDL_free().
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

static void bgmClearQueue(AppState& st) {
    if (st.bgmReady && st.bgmDev) SDL_ClearQueuedAudio(st.bgmDev);
}

// Feed BGM gradually (keeps latency stable)
static void bgmTick(AppState& st) {
    if (!st.bgmReady || !st.bgmDev) return;
    if (!st.bgmBuf || st.bgmLen == 0) return;

    int vol = st.musicMuted ? 0 : clampT(st.musicVolume, 0, 100);
    if (vol <= 0) {
        // keep silent
        bgmClearQueue(st);
        return;
    }

    // Keep queued audio around ~200ms..400ms
    Uint32 queuedBytes = SDL_GetQueuedAudioSize(st.bgmDev);

    int bytesPerSample = (SDL_AUDIO_BITSIZE(st.bgmSpec.format) / 8) * (int)st.bgmSpec.channels;
    if (bytesPerSample <= 0 || st.bgmSpec.freq <= 0) return;

    Uint32 targetMs = 300;
    Uint32 targetBytes = (Uint32)((st.bgmSpec.freq * bytesPerSample) * (targetMs / 1000.0));

    if (queuedBytes >= targetBytes) return;

    // Chunk size ~100ms
    Uint32 chunkMs = 100;
    Uint32 chunkBytes = (Uint32)((st.bgmSpec.freq * bytesPerSample) * (chunkMs / 1000.0));
    if (chunkBytes < 256) chunkBytes = 256;

    // Prepare a temp buffer (scaled by volume)
    Uint8* tmp = (Uint8*)SDL_malloc(chunkBytes);
    if (!tmp) return;
    SDL_memset(tmp, 0, chunkBytes);

    // Fill tmp from bgmBuf with looping
    Uint32 remaining = chunkBytes;
    Uint32 writePos = 0;

    while (remaining > 0) {
        Uint32 avail = st.bgmLen - st.bgmPos;
        Uint32 take = (avail < remaining) ? avail : remaining;

        // scale using SDL_MixAudioFormat into tmp
        int sdlVol = (int)llround((vol / 100.0) * SDL_MIX_MAXVOLUME); // 0..128
        SDL_MixAudioFormat(tmp + writePos, st.bgmBuf + st.bgmPos, st.bgmSpec.format, take, sdlVol);

        st.bgmPos += take;
        if (st.bgmPos >= st.bgmLen) st.bgmPos = 0;

        writePos += take;
        remaining -= take;
    }

    SDL_QueueAudio(st.bgmDev, tmp, chunkBytes);
    SDL_free(tmp);
}

static SDL_Rect settingsRect(int w, int h) {
    return SDL_Rect{w/2 - 260, h/2 - 170, 520, 340};
}

static void openSettings(AppState& st) {
    st.settingsOpen = true;
    st.helpMenuOpen = false;
    st.showLogsPanel = false;
    st.extensionLibraryOpen = false;
    st.penColorPickerOpen = false;
    st.funcIOMenuOpen = false;
    st.saveDialogOpen = false;
    st.loadDialogOpen = false;
    st.askDialogOpen = false;
    SDL_StopTextInput();
    st.log.info(-1, "SET", "Open settings", "");
}

static bool handleSettingsEvent(AppState& st, const SDL_Event& e, int w, int h) {
    if (!st.settingsOpen) return false;

    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        if (e.key.keysym.sym == SDLK_ESCAPE) {
            st.settingsOpen = false;
            st.log.info(-1, "SET", "Close settings (Esc)", "");
            return true;
        }
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;
        SDL_Rect box = settingsRect(w,h);

        if (!pointInRect(mx, my, box)) {
            st.settingsOpen = false;
            st.log.info(-1, "SET", "Close settings (outside)", "");
            return true;
        }

        SDL_Rect rowSpeedDec = {box.x + 30, box.y + 90, 60, 40};
        SDL_Rect rowSpeedInc = {box.x + box.w - 90, box.y + 90, 60, 40};
        SDL_Rect rowToggle   = {box.x + 30, box.y + 150, box.w - 60, 44};

        SDL_Rect okBtn  = {box.x + box.w - 180, box.y + box.h - 60, 140, 40};
        // NEW rows
        SDL_Rect rowCostPrev = {box.x + 30,           box.y + 210, 60, 38};
        SDL_Rect rowCostNext = {box.x + box.w - 90,   box.y + 210, 60, 38};
        SDL_Rect rowCostMid  = {box.x + 100,          box.y + 210, box.w - 200, 38};

        SDL_Rect rowBackPrev = {box.x + 30,           box.y + 255, 60, 38};
        SDL_Rect rowBackNext = {box.x + box.w - 90,   box.y + 255, 60, 38};
        SDL_Rect rowBackMid  = {box.x + 100,          box.y + 255, box.w - 200, 38};

        SDL_Rect rowMusicDec = {box.x + 30,           box.y + 300, 60, 32};
        SDL_Rect rowMusicInc = {box.x + box.w - 90,   box.y + 300, 60, 32};
        SDL_Rect rowMusicMid = {box.x + 100,          box.y + 300, box.w - 320, 32}; // smaller to fit mute btn
        SDL_Rect rowMusicMute= {box.x + box.w - 250,  box.y + 300, 150, 32};


        if (pointInRect(mx,my,rowSpeedDec)) {
            st.runSpeedMs = clampT(st.runSpeedMs - 10, 0, 300);
            st.log.info(-1, "SET", "Speed -10", "runSpeedMs=" + to_string(st.runSpeedMs));
            return true;
        }
        if (pointInRect(mx,my,rowSpeedInc)) {
            st.runSpeedMs = clampT(st.runSpeedMs + 10, 0, 300);
            st.log.info(-1, "SET", "Speed +10", "runSpeedMs=" + to_string(st.runSpeedMs));
            return true;
        }
        if (pointInRect(mx,my,rowToggle)) {
            st.drawActorWhenStopped = !st.drawActorWhenStopped;
            st.log.info(-1, "SET", "Toggle draw actor when stopped", st.drawActorWhenStopped ? "ON" : "OFF");
            return true;
        }
        if (pointInRect(mx,my,okBtn)) {
            st.settingsOpen = false;
            st.log.info(-1, "SET", "Close settings (OK)", "");
            return true;
        }
                // ===== NEW: Costume controls =====
        if (pointInRect(mx,my,rowCostPrev)) {
            if (!st.costumes.empty()) {
                st.costumeIndex--;
                if (st.costumeIndex < 0) st.costumeIndex = (int)st.costumes.size() - 1;
            }
            st.log.info(-1, "SET", "Costume prev", "idx=" + to_string(st.costumeIndex));
            return true;
        }
        if (pointInRect(mx,my,rowCostNext)) {
            if (!st.costumes.empty()) {
                st.costumeIndex = (st.costumeIndex + 1) % (int)st.costumes.size();
            }
            st.log.info(-1, "SET", "Costume next", "idx=" + to_string(st.costumeIndex));
            return true;
        }

        // ===== NEW: Backdrop controls =====
        if (pointInRect(mx,my,rowBackPrev)) {
            if (!st.backdrops.empty()) {
                st.backdropIndex--;
                if (st.backdropIndex < 0) st.backdropIndex = (int)st.backdrops.size() - 1;
            }
            st.log.info(-1, "SET", "Backdrop prev", "idx=" + to_string(st.backdropIndex));
            return true;
        }
        if (pointInRect(mx,my,rowBackNext)) {
            if (!st.backdrops.empty()) {
                st.backdropIndex = (st.backdropIndex + 1) % (int)st.backdrops.size();
            }
            st.log.info(-1, "SET", "Backdrop next", "idx=" + to_string(st.backdropIndex));
            return true;
        }

        // ===== NEW: Music volume +/-10 =====
        if (pointInRect(mx,my,rowMusicDec)) {
            st.musicVolume = clampT(st.musicVolume - 10, 0, 100);
            st.log.info(-1, "SET", "Music volume -10", "musicVolume=" + to_string(st.musicVolume));
            return true;
        }
        if (pointInRect(mx,my,rowMusicInc)) {
            st.musicVolume = clampT(st.musicVolume + 10, 0, 100);
            st.log.info(-1, "SET", "Music volume +10", "musicVolume=" + to_string(st.musicVolume));
            return true;
        }

        // click bar to set volume
        if (pointInRect(mx,my,rowMusicMid)) {
            int rel = mx - rowMusicMid.x;
            int v = (int)llround((rel / (double)max(1, rowMusicMid.w)) * 100.0);
            st.musicVolume = clampT(v, 0, 100);
            st.log.info(-1, "SET", "Music volume set", "musicVolume=" + to_string(st.musicVolume));
            return true;
        }

        // mute/unmute
        if (pointInRect(mx,my,rowMusicMute)) {
            st.musicMuted = !st.musicMuted;
            if (st.musicMuted) bgmClearQueue(st); // instantly silence
            st.log.info(-1, "SET", "Music mute toggle", st.musicMuted ? "MUTED" : "UNMUTED");
            return true;
        }


        return true;
    }

    return true;
}

static void renderSettings(const AppState& st, SDL_Renderer* r, int w, int h) {
    if (!st.settingsOpen) return;

    SDL_SetRenderDrawColor(r, 0,0,0,170);
    SDL_Rect full{0,0,w,h};
    SDL_RenderFillRect(r, &full);

    SDL_Rect box = settingsRect(w,h);
    SDL_SetRenderDrawColor(r, 40,40,46,255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 200,200,200,255);
    SDL_RenderDrawRect(r, &box);

    SDL_Color white{240,240,240,255};
    renderText(r, st.uiFont, "Settings (Esc to close)", box.x + 20, box.y + 18, white);

    // speed row
    SDL_Rect dec = {box.x + 30, box.y + 90, 60, 40};
    SDL_Rect inc = {box.x + box.w - 90, box.y + 90, 60, 40};
    SDL_Rect mid = {box.x + 100, box.y + 90, box.w - 200, 40};

    SDL_SetRenderDrawColor(r, 80,80,90,255);
    SDL_RenderFillRect(r, &dec);
    SDL_RenderFillRect(r, &inc);
    SDL_SetRenderDrawColor(r, 25,25,28,255);
    SDL_RenderFillRect(r, &mid);

    SDL_SetRenderDrawColor(r, 15,15,15,255);
    SDL_RenderDrawRect(r, &dec);
    SDL_RenderDrawRect(r, &inc);
    SDL_RenderDrawRect(r, &mid);

    renderTextCentered(r, st.uiFont, "-", dec, 0, white);
    renderTextCentered(r, st.uiFont, "+", inc, 0, white);

    string sp = "Run speed delay: " + to_string(st.runSpeedMs) + " ms per block";
    renderText(r, st.uiFont, sp, mid.x + 10, mid.y + 10, white);

    // toggle row
    SDL_Rect tog = {box.x + 30, box.y + 150, box.w - 60, 44};
    SDL_SetRenderDrawColor(r, 25,25,28,255);
    SDL_RenderFillRect(r, &tog);
    SDL_SetRenderDrawColor(r, 120,120,120,255);
    SDL_RenderDrawRect(r, &tog);

    string tv = string("Show actor when stopped: ") + (st.drawActorWhenStopped ? "ON" : "OFF");
    renderText(r, st.uiFont, tv, tog.x + 12, tog.y + 12, white);
        // ===== NEW: Costume row =====
    SDL_Rect costPrev = {box.x + 30,         box.y + 210, 60, 38};
    SDL_Rect costNext = {box.x + box.w - 90, box.y + 210, 60, 38};
    SDL_Rect costMid  = {box.x + 100,        box.y + 210, box.w - 200, 38};

    SDL_SetRenderDrawColor(r, 80,80,90,255);
    SDL_RenderFillRect(r, &costPrev);
    SDL_RenderFillRect(r, &costNext);
    SDL_SetRenderDrawColor(r, 25,25,28,255);
    SDL_RenderFillRect(r, &costMid);

    SDL_SetRenderDrawColor(r, 15,15,15,255);
    SDL_RenderDrawRect(r, &costPrev);
    SDL_RenderDrawRect(r, &costNext);
    SDL_RenderDrawRect(r, &costMid);

    renderTextCentered(r, st.uiFont, "<", costPrev, 0, white);
    renderTextCentered(r, st.uiFont, ">", costNext, 0, white);

    int cCount = (int)st.costumes.size();
    int cIdx = cCount > 0 ? ((st.costumeIndex % cCount) + cCount) % cCount : 0;
    string cLabel = "Costume: " + to_string(cIdx) + " / " + to_string(max(0, cCount - 1));
    renderText(r, st.uiFont, cLabel, costMid.x + 10, costMid.y + 10, white);

    // ===== NEW: Backdrop row =====
    SDL_Rect backPrev = {box.x + 30,         box.y + 255, 60, 38};
    SDL_Rect backNext = {box.x + box.w - 90, box.y + 255, 60, 38};
    SDL_Rect backMid  = {box.x + 100,        box.y + 255, box.w - 200, 38};

    SDL_SetRenderDrawColor(r, 80,80,90,255);
    SDL_RenderFillRect(r, &backPrev);
    SDL_RenderFillRect(r, &backNext);
    SDL_SetRenderDrawColor(r, 25,25,28,255);
    SDL_RenderFillRect(r, &backMid);

    SDL_SetRenderDrawColor(r, 15,15,15,255);
    SDL_RenderDrawRect(r, &backPrev);
    SDL_RenderDrawRect(r, &backNext);
    SDL_RenderDrawRect(r, &backMid);

    renderTextCentered(r, st.uiFont, "<", backPrev, 0, white);
    renderTextCentered(r, st.uiFont, ">", backNext, 0, white);

    int bCount = (int)st.backdrops.size();
    int bIdx = bCount > 0 ? ((st.backdropIndex % bCount) + bCount) % bCount : 0;
    string bLabel = "Backdrop: " + to_string(bIdx) + " / " + to_string(max(0, bCount - 1));
    renderText(r, st.uiFont, bLabel, backMid.x + 10, backMid.y + 10, white);

    // ===== NEW: Music row (volume + mute) =====
    SDL_Rect musDec  = {box.x + 30,         box.y + 300, 60, 32};
    SDL_Rect musInc  = {box.x + box.w - 90, box.y + 300, 60, 32};
    SDL_Rect musMid  = {box.x + 100,        box.y + 300, box.w - 320, 32};
    SDL_Rect musMute = {box.x + box.w - 250, box.y + 300, 150, 32};

    SDL_SetRenderDrawColor(r, 80,80,90,255);
    SDL_RenderFillRect(r, &musDec);
    SDL_RenderFillRect(r, &musInc);

    SDL_SetRenderDrawColor(r, 25,25,28,255);
    SDL_RenderFillRect(r, &musMid);

    SDL_SetRenderDrawColor(r, st.musicMuted ? 120 : 60, st.musicMuted ? 60 : 140, 70, 255);
    SDL_RenderFillRect(r, &musMute);

    SDL_SetRenderDrawColor(r, 15,15,15,255);
    SDL_RenderDrawRect(r, &musDec);
    SDL_RenderDrawRect(r, &musInc);
    SDL_RenderDrawRect(r, &musMid);
    SDL_RenderDrawRect(r, &musMute);

    renderTextCentered(r, st.uiFont, "-", musDec, 0, white);
    renderTextCentered(r, st.uiFont, "+", musInc, 0, white);

    string mv = "Music: " + to_string(clampT(st.musicVolume, 0, 100));
    renderText(r, st.uiFont, mv, musMid.x + 10, musMid.y + 7, white);

    // simple bar fill indicator
    int fillW = (int)llround((clampT(st.musicVolume,0,100) / 100.0) * (musMid.w - 20));
    SDL_Rect bar{musMid.x + 10, musMid.y + musMid.h - 8, max(0, fillW), 4};
    SDL_SetRenderDrawColor(r, 200,200,200,255);
    SDL_RenderFillRect(r, &bar);

    renderTextCentered(r, st.uiFont, st.musicMuted ? "Muted" : "Mute", musMute, 0, white);


    // OK
    SDL_Rect okBtn  = {box.x + box.w - 180, box.y + box.h - 60, 140, 40};
    SDL_SetRenderDrawColor(r, 60,140,70,255);
    SDL_RenderFillRect(r, &okBtn);
    SDL_SetRenderDrawColor(r, 20,20,20,255);
    SDL_RenderDrawRect(r, &okBtn);
    renderTextCentered(r, st.uiFont, "OK", okBtn, 0, white);
}



// =========================
// Dialog helpers (Save/Load + Ask)
// =========================
static void beginSaveDialog(AppState& st) {
    st.saveDialogOpen = true;
    st.loadDialogOpen = false;
    st.saveNameInput = "";
    SDL_StartTextInput();
}

static void beginLoadDialog(AppState& st) {
    st.loadDialogOpen = true;
    st.saveDialogOpen = false;
    st.saveList = listSaveStems();
    st.loadHoverIndex = -1;
    st.loadScroll = 0;
    SDL_StopTextInput();
}

static void beginAskDialog(AppState& st, const string& question, int resumePC) {
    st.askDialogOpen = true;
    st.askQuestion = question;
    st.askInput = "";
    st.askResumePC = resumePC;
    SDL_StartTextInput();
    st.log.info(st.scriptPC, "ASK", "Open ask dialog", "q=" + question);
}

static void modalRects(int winW, int winH, SDL_Rect& modal, SDL_Rect& listArea) {
    modal = SDL_Rect{winW/2 - 260, winH/2 - 180, 520, 360};
    listArea = SDL_Rect{modal.x + 30, modal.y + 60, modal.w - 60, modal.h - 90};
}

static void updateLoadHover(AppState& st, int winW, int winH) {
    if (!st.loadDialogOpen) return;

    SDL_Rect modal{}, listArea{};
    modalRects(winW, winH, modal, listArea);

    st.loadHoverIndex = -1;
    if (!pointInRect(st.in.mx, st.in.my, listArea)) return;

    const int rowH = 28;
    int local = (st.in.my - listArea.y) / rowH;
    int idx = st.loadScroll + local;
    if (idx >= 0 && idx < (int)st.saveList.size()) st.loadHoverIndex = idx;
}

static bool handleDialogsEvent(AppState& st, const SDL_Event& e, int winW, int winH) {
    // --- Ask Dialog (highest priority)
    if (st.askDialogOpen) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                st.askDialogOpen = false;
                SDL_StopTextInput();
                st.lastAnswer = "";
                st.scriptPC = st.askResumePC;
                st.askResumePC = -1;
                st.log.warn(st.scriptPC, "ASK", "Ask cancelled", "");
                return true;
            }
            if (e.key.keysym.sym == SDLK_BACKSPACE) {
                if (!st.askInput.empty()) st.askInput.pop_back();
                return true;
            }
            if (e.key.keysym.sym == SDLK_RETURN) {
                st.lastAnswer = st.askInput;
                st.askDialogOpen = false;
                SDL_StopTextInput();
                st.scriptPC = st.askResumePC;
                st.askResumePC = -1;
                st.log.info(st.scriptPC, "ASK", "Answer received", "ans=" + st.lastAnswer);
                return true;
            }
        }
        if (e.type == SDL_TEXTINPUT) {
            st.askInput += e.text.text;
            return true;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;

            SDL_Rect modal = {winW/2 - 260, winH/2 - 120, 520, 240};
            SDL_Rect okBtn  = {modal.x + modal.w - 180, modal.y + modal.h - 60, 140, 40};
            SDL_Rect canBtn = {modal.x +  40,         modal.y + modal.h - 60, 140, 40};

            if (pointInRect(mx, my, okBtn)) {
                st.lastAnswer = st.askInput;
                st.askDialogOpen = false;
                SDL_StopTextInput();
                st.scriptPC = st.askResumePC;
                st.askResumePC = -1;
                st.log.info(st.scriptPC, "ASK", "Answer received (click)", "ans=" + st.lastAnswer);
                return true;
            }
            if (pointInRect(mx, my, canBtn)) {
                st.askDialogOpen = false;
                SDL_StopTextInput();
                st.lastAnswer = "";
                st.scriptPC = st.askResumePC;
                st.askResumePC = -1;
                st.log.warn(st.scriptPC, "ASK", "Ask cancelled (click)", "");
                return true;
            }
            return true;
        }

        return true;
    }

    // --- Save Dialog
    if (st.saveDialogOpen) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                st.saveDialogOpen = false;
                SDL_StopTextInput();
                return true;
            }
            if (e.key.keysym.sym == SDLK_BACKSPACE) {
                if (!st.saveNameInput.empty()) st.saveNameInput.pop_back();
                return true;
            }
            if (e.key.keysym.sym == SDLK_RETURN) {
                string stem = sanitizeStem(st.saveNameInput);
                bool ok = saveProjectNamed(stem, st.ws, st.log);
                st.saveDialogOpen = false;
                SDL_StopTextInput();
                if (ok) infoBox("Saved", "Project saved successfully.");
                else    infoBox("Save failed", "Could not save the project.");
                return true;
            }
        }
        if (e.type == SDL_TEXTINPUT) {
            st.saveNameInput += e.text.text;
            return true;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;

            SDL_Rect modal = {winW/2 - 220, winH/2 - 90, 440, 180};
            SDL_Rect okBtn  = {modal.x + 260, modal.y + 120, 140, 40};
            SDL_Rect canBtn = {modal.x +  40, modal.y + 120, 140, 40};

            if (pointInRect(mx, my, okBtn)) {
                string stem = sanitizeStem(st.saveNameInput);
                bool ok = saveProjectNamed(stem, st.ws, st.log);
                st.saveDialogOpen = false;
                SDL_StopTextInput();
                if (ok) infoBox("Saved", "Project saved successfully.");
                else    infoBox("Save failed", "Could not save the project.");
                return true;
            }
            if (pointInRect(mx, my, canBtn)) {
                st.saveDialogOpen = false;
                SDL_StopTextInput();
                return true;
            }
            return true;
        }

        return true;
    }

    // --- Load Dialog
    if (st.loadDialogOpen) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                st.loadDialogOpen = false;
                return true;
            }
        }

        if (e.type == SDL_MOUSEWHEEL) {
            const int rowStep = (e.wheel.y > 0) ? -1 : (e.wheel.y < 0 ? +1 : 0);
            if (rowStep != 0) {
                SDL_Rect modal{}, listArea{};
                modalRects(winW, winH, modal, listArea);

                int rowH = 28;
                int visible = listArea.h / rowH;
                int maxScroll = max(0, (int)st.saveList.size() - visible);

                st.loadScroll = clampT(st.loadScroll + rowStep, 0, maxScroll);
            }
            return true;
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;

            SDL_Rect modal{}, listArea{};
            modalRects(winW, winH, modal, listArea);

            if (!pointInRect(mx, my, modal)) {
                st.loadDialogOpen = false;
                return true;
            }

            if (pointInRect(mx, my, listArea)) {
                const int rowH = 28;
                int local = (my - listArea.y) / rowH;
                int idx = st.loadScroll + local;

                if (idx >= 0 && idx < (int)st.saveList.size()) {
                    bool ok = loadProjectNamed(st.saveList[idx], st.ws, st.log);
                    st.loadDialogOpen = false;
                    if (ok) infoBox("Loaded", "Project loaded successfully.");
                    else    infoBox("Load failed", "Could not load the project.");
                    return true;
                }
            }
            return true;
        }

        return true;
    }

    return false;
}

static void renderDialogs(const AppState& st, SDL_Renderer* r, int winW, int winH) {
    if (!st.saveDialogOpen && !st.loadDialogOpen && !st.askDialogOpen) return;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, winW, winH};
    SDL_RenderFillRect(r, &full);

    SDL_Color white = {240, 240, 240, 255};

    if (st.askDialogOpen) {
        SDL_Rect modal = {winW/2 - 260, winH/2 - 120, 520, 240};
        SDL_SetRenderDrawColor(r, 40, 40, 46, 255);
        SDL_RenderFillRect(r, &modal);
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderDrawRect(r, &modal);

        renderText(r, st.uiFont, "Ask", modal.x + 20, modal.y + 14, white);
        renderText(r, st.uiFont, st.askQuestion, modal.x + 20, modal.y + 40, white);

        SDL_Rect field = {modal.x + 20, modal.y + 80, modal.w - 40, 40};
        SDL_SetRenderDrawColor(r, 25, 25, 28, 255);
        SDL_RenderFillRect(r, &field);
        SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
        SDL_RenderDrawRect(r, &field);

        string shown = st.askInput.empty() ? "type answer..." : st.askInput;
        renderText(r, st.uiFont, shown, field.x + 10, field.y + 8, white);

        SDL_Rect okBtn  = {modal.x + modal.w - 180, modal.y + modal.h - 60, 140, 40};
        SDL_Rect canBtn = {modal.x +  40,         modal.y + modal.h - 60, 140, 40};

        SDL_SetRenderDrawColor(r, 60, 140, 70, 255);
        SDL_RenderFillRect(r, &okBtn);
        SDL_SetRenderDrawColor(r, 140, 60, 60, 255);
        SDL_RenderFillRect(r, &canBtn);

        SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
        SDL_RenderDrawRect(r, &okBtn);
        SDL_RenderDrawRect(r, &canBtn);

        renderTextCentered(r, st.uiFont, "OK", okBtn, -6, white);
        renderTextCentered(r, st.uiFont, "Enter", okBtn, +10, white);

        renderTextCentered(r, st.uiFont, "Cancel", canBtn, -6, white);
        renderTextCentered(r, st.uiFont, "Esc", canBtn, +10, white);
    }

    if (st.saveDialogOpen) {
        SDL_Rect modal = {winW/2 - 220, winH/2 - 90, 440, 180};
        SDL_SetRenderDrawColor(r, 40, 40, 46, 255);
        SDL_RenderFillRect(r, &modal);
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderDrawRect(r, &modal);

        renderText(r, st.uiFont, "Save Project", modal.x + 20, modal.y + 14, white);

        SDL_Rect field = {modal.x + 20, modal.y + 55, modal.w - 40, 40};
        SDL_SetRenderDrawColor(r, 25, 25, 28, 255);
        SDL_RenderFillRect(r, &field);
        SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
        SDL_RenderDrawRect(r, &field);

        string shown = st.saveNameInput.empty() ? "type a name..." : st.saveNameInput;
        renderText(r, st.uiFont, shown, field.x + 10, field.y + 8, white);

        SDL_Rect okBtn  = {modal.x + 260, modal.y + 120, 140, 40};
        SDL_Rect canBtn = {modal.x +  40, modal.y + 120, 140, 40};

        SDL_SetRenderDrawColor(r, 60, 140, 70, 255);
        SDL_RenderFillRect(r, &okBtn);
        SDL_SetRenderDrawColor(r, 140, 60, 60, 255);
        SDL_RenderFillRect(r, &canBtn);

        SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
        SDL_RenderDrawRect(r, &okBtn);
        SDL_RenderDrawRect(r, &canBtn);

        renderTextCentered(r, st.uiFont, "OK", okBtn, -6, white);
        renderTextCentered(r, st.uiFont, "Enter", okBtn, +10, white);

        renderTextCentered(r, st.uiFont, "Cancel", canBtn, -6, white);
        renderTextCentered(r, st.uiFont, "Esc", canBtn, +10, white);
    }

    if (st.loadDialogOpen) {
        SDL_Rect modal{}, listArea{};
        modalRects(winW, winH, modal, listArea);

        SDL_SetRenderDrawColor(r, 40, 40, 46, 255);
        SDL_RenderFillRect(r, &modal);
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderDrawRect(r, &modal);

        renderText(r, st.uiFont, "Load Project", modal.x + 20, modal.y + 14, white);
        renderText(r, st.uiFont, "Click a save to load. (Esc to close)", modal.x + 20, modal.y + 34, white);

        SDL_SetRenderDrawColor(r, 25, 25, 28, 255);
        SDL_RenderFillRect(r, &listArea);
        SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
        SDL_RenderDrawRect(r, &listArea);

        const int rowH = 28;
        int visible = listArea.h / rowH;
        int start = st.loadScroll;
        int end = min((int)st.saveList.size(), start + visible);

        for (int i = start; i < end; i++) {
            int y = listArea.y + (i - start) * rowH;
            SDL_Rect row = {listArea.x, y, listArea.w, rowH};

            bool hover = (i == st.loadHoverIndex);

            if (hover) SDL_SetRenderDrawColor(r, 60, 60, 70, 255);
            else       SDL_SetRenderDrawColor(r, ((i % 2) ? 34 : 30), ((i % 2) ? 34 : 30), ((i % 2) ? 38 : 34), 255);

            SDL_RenderFillRect(r, &row);

            renderText(r, st.uiFont, st.saveList[i], row.x + 10, row.y + 4, white);
        }

        if (st.saveList.empty()) {
            renderText(r, st.uiFont, "No saves found in SMemory.", listArea.x + 10, listArea.y + 10, white);
        }
    }
}

// =========================
// Add Extension: Pen helpers (logic + rendering)
// =========================
static SDL_Color hsvToRgb(double h, double s, double v) {
    s = clampT(s, 0.0, 100.0) / 100.0;
    v = clampT(v, 0.0, 100.0) / 100.0;
    h = fmod(h, 360.0);
    if (h < 0) h += 360.0;

    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;

    double r=0,g=0,b=0;
    if      (h < 60)  { r=c; g=x; b=0; }
    else if (h < 120) { r=x; g=c; b=0; }
    else if (h < 180) { r=0; g=c; b=x; }
    else if (h < 240) { r=0; g=x; b=c; }
    else if (h < 300) { r=x; g=0; b=c; }
    else              { r=c; g=0; b=x; }

    Uint8 R = (Uint8)clampT((int)round((r + m) * 255.0), 0, 255);
    Uint8 G = (Uint8)clampT((int)round((g + m) * 255.0), 0, 255);
    Uint8 B = (Uint8)clampT((int)round((b + m) * 255.0), 0, 255);
    return SDL_Color{R,G,B,255};
}

static void rgbToHsv(const SDL_Color& c, double& outH, double& outS, double& outV) {
    double r = c.r / 255.0;
    double g = c.g / 255.0;
    double b = c.b / 255.0;

    double mx = max(r, max(g,b));
    double mn = min(r, min(g,b));
    double d = mx - mn;

    double h = 0.0;
    if (d == 0.0) h = 0.0;
    else if (mx == r) h = 60.0 * fmod(((g - b) / d), 6.0);
    else if (mx == g) h = 60.0 * (((b - r) / d) + 2.0);
    else              h = 60.0 * (((r - g) / d) + 4.0);

    if (h < 0) h += 360.0;

    double s = (mx == 0.0) ? 0.0 : (d / mx);
    double v = mx;

    outH = h;
    outS = s * 100.0;
    outV = v * 100.0;
}

static bool isPenCmd(const string& cmd) {
    return cmd.rfind("PEN_", 0) == 0;
}

static void penSyncRGB(AppState& st) {
    st.penRGB = hsvToRgb(st.penHue, st.penSat, st.penBri);
}

static string penNextAttr(const string& cur) {
    if (cur == "COLOR") return "SAT";
    if (cur == "SAT")   return "BRI";
    return "COLOR";
}

static void penClearAll(AppState& st) {
    st.penSegs.clear();
    st.penStamps.clear();
}

static void penAddSegment(AppState& st, double x1, double y1, double x2, double y2) {
    PenSegment seg;
    seg.x1 = x1; seg.y1 = y1; seg.x2 = x2; seg.y2 = y2;
    seg.c = st.penRGB;
    seg.size = st.penSize;
    st.penSegs.push_back(seg);
}

static void penAddStamp(AppState& st) {
    PenStamp s;
    s.x = st.actorX;
    s.y = st.actorY;
    s.size = 12;
    st.penStamps.push_back(s);
}

static void drawThickLine(SDL_Renderer* r, double x1, double y1, double x2, double y2, SDL_Color c, int size) {
    size = clampT(size, 1, 30);
    double dx = x2 - x1;
    double dy = y2 - y1;
    double steps = max(fabs(dx), fabs(dy));
    if (steps < 1.0) steps = 1.0;

    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);

    for (int i = 0; i <= (int)steps; i++) {
        double t = (double)i / steps;
        int px = (int)round(x1 + dx * t);
        int py = (int)round(y1 + dy * t);
        SDL_Rect dot{px - size/2, py - size/2, size, size};
        SDL_RenderFillRect(r, &dot);
    }
}

static void renderPenLayer(const AppState& st, SDL_Renderer* r) {
    SDL_RenderSetClipRect(r, &st.ws.bounds);

    for (const auto& seg : st.penSegs) {
        drawThickLine(r, seg.x1, seg.y1, seg.x2, seg.y2, seg.c, seg.size);
    }

    for (const auto& sp : st.penStamps) {
        int s = sp.size;
        SDL_Rect rc{(int)sp.x - s/2, (int)sp.y - s/2, s, s};
        SDL_SetRenderDrawColor(r, sp.fill.r, sp.fill.g, sp.fill.b, sp.fill.a);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, sp.outline.r, sp.outline.g, sp.outline.b, sp.outline.a);
        SDL_RenderDrawRect(r, &rc);
    }

    SDL_RenderSetClipRect(r, nullptr);
}

// =========================
// Add Extension: Library UI + Palette + Color Picker
// =========================
static void openExtensionLibrary(AppState& st) {
    st.extensionLibraryOpen = true;
    st.helpMenuOpen = false;
    st.showLogsPanel = false;
    st.penColorPickerOpen = false;
    st.funcIOMenuOpen = false;
    st.funcIOMenuBlockIndex = -1;
    st.log.info(-1, "EXT", "Open library", "");
}

static SDL_Rect extensionLibraryRect(int w, int h) {
    return SDL_Rect{w/2 - 320, h/2 - 220, 640, 440};
}

static SDL_Rect extensionItemRect(const SDL_Rect& box, int i) {
    return SDL_Rect{box.x + 30, box.y + 90 + i*70, box.w - 60, 56};
}

static bool handleExtensionLibraryEvent(AppState& st, const SDL_Event& e, int w, int h) {
    if (!st.extensionLibraryOpen) return false;

    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        if (e.key.keysym.sym == SDLK_ESCAPE) {
            st.extensionLibraryOpen = false;
            st.log.info(-1, "EXT", "Close library (Esc)", "");
            return true;
        }
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;
        SDL_Rect box = extensionLibraryRect(w,h);

        if (!pointInRect(mx, my, box)) {
            st.extensionLibraryOpen = false;
            st.log.info(-1, "EXT", "Close library (outside click)", "");
            return true;
        }

        SDL_Rect penItem = extensionItemRect(box, 0);
        if (pointInRect(mx, my, penItem)) {
            st.penExtensionEnabled = true;
            st.extensionLibraryOpen = false;
            st.paletteDirty = true;
            st.log.info(-1, "EXT", "Enable Pen", "installed=1");
            return true;
        }

        return true;
    }

    return true;
}

static void renderExtensionLibrary(const AppState& st, SDL_Renderer* r, int w, int h) {
    if (!st.extensionLibraryOpen) return;

    SDL_SetRenderDrawColor(r, 0,0,0,170);
    SDL_Rect full{0,0,w,h};
    SDL_RenderFillRect(r, &full);

    SDL_Rect box = extensionLibraryRect(w,h);
    SDL_SetRenderDrawColor(r, 40,40,46,255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 200,200,200,255);
    SDL_RenderDrawRect(r, &box);

    SDL_Color white{240,240,240,255};
    renderText(r, st.uiFont, "Extension Library (Esc to close)", box.x + 20, box.y + 18, white);
    renderText(r, st.uiFont, "Click an extension to enable it:", box.x + 20, box.y + 44, white);

    SDL_Rect penItem = extensionItemRect(box, 0);
    SDL_SetRenderDrawColor(r, 30,30,34,255);
    SDL_RenderFillRect(r, &penItem);
    SDL_SetRenderDrawColor(r, 15,15,15,255);
    SDL_RenderDrawRect(r, &penItem);

    string penLabel = "Pen";
    string penState = st.penExtensionEnabled ? "Installed" : "Click to enable";
    renderText(r, st.uiFont, penLabel, penItem.x + 16, penItem.y + 10, white);
    renderText(r, st.uiFont, penState, penItem.x + 16, penItem.y + 30, white);

    SDL_Rect sw{penItem.x + penItem.w - 44, penItem.y + 12, 28, 28};
    SDL_SetRenderDrawColor(r, 40,180,90,255);
    SDL_RenderFillRect(r, &sw);
    SDL_SetRenderDrawColor(r, 10,10,10,255);
    SDL_RenderDrawRect(r, &sw);
}

// ---- Pen color picker (minimal preset palette)
static SDL_Rect colorPickerRect(int w, int h) {
    return SDL_Rect{w/2 - 250, h/2 - 180, 500, 360};
}

static bool handlePenColorPickerEvent(AppState& st, const SDL_Event& e, int w, int h) {
    if (!st.penColorPickerOpen) return false;

    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        if (e.key.keysym.sym == SDLK_ESCAPE) {
            st.penColorPickerOpen = false;
            st.penColorPickerBlockIndex = -1;
            st.log.info(-1, "PEN", "Close color picker (Esc)", "");
            return true;
        }
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;
        SDL_Rect box = colorPickerRect(w,h);

        if (!pointInRect(mx, my, box)) {
            st.penColorPickerOpen = false;
            st.penColorPickerBlockIndex = -1;
            st.log.info(-1, "PEN", "Close color picker (outside click)", "");
            return true;
        }

        SDL_Color presets[12] = {
            {255,0,0,255},{255,128,0,255},{255,255,0,255},{128,255,0,255},
            {0,255,0,255},{0,255,128,255},{0,255,255,255},{0,128,255,255},
            {0,0,255,255},{128,0,255,255},{255,0,255,255},{255,255,255,255}
        };

        int gridX = box.x + 40;
        int gridY = box.y + 90;
        int cell = 60;
        int cols = 4;

        for (int i = 0; i < 12; i++) {
            int cx = gridX + (i % cols) * cell;
            int cy = gridY + (i / cols) * cell;
            SDL_Rect rc{cx, cy, 44, 44};
            if (pointInRect(mx,my,rc)) {
                SDL_Color chosen = presets[i];

                rgbToHsv(chosen, st.penHue, st.penSat, st.penBri);
                penSyncRGB(st);

                if (st.penColorPickerBlockIndex >= 0 &&
                    st.penColorPickerBlockIndex < (int)st.ws.blocks.size()) {
                    Block& b = st.ws.blocks[st.penColorPickerBlockIndex];
                    b.pickColor = chosen;
                }

                st.penColorPickerOpen = false;
                st.penColorPickerBlockIndex = -1;
                st.log.info(-1, "PEN", "Pick color",
                            "rgb=(" + to_string((int)chosen.r) + "," +
                                    to_string((int)chosen.g) + "," +
                                    to_string((int)chosen.b) + ")");
                return true;
            }
        }

        return true;
    }

    return true;
}

static void renderPenColorPicker(const AppState& st, SDL_Renderer* r, int w, int h) {
    if (!st.penColorPickerOpen) return;

    SDL_SetRenderDrawColor(r, 0,0,0,170);
    SDL_Rect full{0,0,w,h};
    SDL_RenderFillRect(r, &full);

    SDL_Rect box = colorPickerRect(w,h);
    SDL_SetRenderDrawColor(r, 40,40,46,255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 200,200,200,255);
    SDL_RenderDrawRect(r, &box);

    SDL_Color white{240,240,240,255};
    renderText(r, st.uiFont, "Pick Pen Color (Esc to close)", box.x + 20, box.y + 18, white);
    renderText(r, st.uiFont, "Click a color:", box.x + 20, box.y + 44, white);

    SDL_Color presets[12] = {
        {255,0,0,255},{255,128,0,255},{255,255,0,255},{128,255,0,255},
        {0,255,0,255},{0,255,128,255},{0,255,255,255},{0,128,255,255},
        {0,0,255,255},{128,0,255,255},{255,0,255,255},{255,255,255,255}
    };

    int gridX = box.x + 40;
    int gridY = box.y + 90;
    int cell = 60;
    int cols = 4;

    for (int i = 0; i < 12; i++) {
        int cx = gridX + (i % cols) * cell;
        int cy = gridY + (i / cols) * cell;
        SDL_Rect rc{cx, cy, 44, 44};

        SDL_SetRenderDrawColor(r, presets[i].r, presets[i].g, presets[i].b, 255);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, 10,10,10,255);
        SDL_RenderDrawRect(r, &rc);
    }

    SDL_Rect prev{box.x + box.w - 86, box.y + 18, 56, 56};
    SDL_SetRenderDrawColor(r, st.penRGB.r, st.penRGB.g, st.penRGB.b, 255);
    SDL_RenderFillRect(r, &prev);
    SDL_SetRenderDrawColor(r, 10,10,10,255);
    SDL_RenderDrawRect(r, &prev);
}

// =========================
// Section 5: Function I/O Menu (Modal)
// =========================
static SDL_Rect funcIOMenuRect(int w, int h) {
    return SDL_Rect{w/2 - 260, h/2 - 150, 520, 300};
}

static const vector<string>& funcList() {
    static vector<string> v = {"sqrt","abs","sin","cos","tan","round","floor","ceil"};
    return v;
}
static const vector<string>& funcInputList() {
    // minimal sources
    static vector<string> v = {"last","v","score","msg","mouseX","mouseY","timer","answer","actorX","actorY","dir"};
    return v;
}
static const vector<string>& funcOutputList() {
    static vector<string> v = {"last","v","score","msg"};
    return v;
}

static int indexOfStr(const vector<string>& v, const string& s) {
    for (int i = 0; i < (int)v.size(); i++) if (v[i] == s) return i;
    return -1;
}

static string cycleStrInList(const vector<string>& v, const string& cur) {
    if (v.empty()) return cur;
    int idx = indexOfStr(v, cur);
    if (idx < 0) return v[0];
    return v[(idx + 1) % (int)v.size()];
}

static string prettyIO(const string& token) {
    if (token == "last")   return "lastValue";
    if (token == "mouseX") return "mouseX";
    if (token == "mouseY") return "mouseY";
    if (token == "timer")  return "timer(sec)";
    if (token == "answer") return "answer";
    if (token == "actorX") return "actorX";
    if (token == "actorY") return "actorY";
    if (token == "dir")    return "direction";
    return string("var: ") + token;
}

static void openFuncIOMenu(AppState& st, int blockIndex) {
    if (blockIndex < 0 || blockIndex >= (int)st.ws.blocks.size()) return;
    if (st.ws.blocks[blockIndex].cmd != "FUNC_APPLY") return;

    st.funcIOMenuOpen = true;
    st.funcIOMenuBlockIndex = blockIndex;

    // close other overlays
    st.helpMenuOpen = false;
    st.showLogsPanel = false;
    st.extensionLibraryOpen = false;
    st.penColorPickerOpen = false;

    st.log.info(blockIndex, "FUNC", "Open I/O menu", "");
}

static void closeFuncIOMenu(AppState& st, const string& why) {
    if (!st.funcIOMenuOpen) return;
    st.funcIOMenuOpen = false;
    st.funcIOMenuBlockIndex = -1;
    st.log.info(-1, "FUNC", "Close I/O menu", why);
}

static bool handleFuncIOMenuEvent(AppState& st, const SDL_Event& e, int w, int h) {
    if (!st.funcIOMenuOpen) return false;

    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        if (e.key.keysym.sym == SDLK_ESCAPE) {
            closeFuncIOMenu(st, "Esc");
            return true;
        }
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;
        SDL_Rect box = funcIOMenuRect(w,h);

        if (!pointInRect(mx, my, box)) {
            closeFuncIOMenu(st, "outside click");
            return true;
        }

        int bi = st.funcIOMenuBlockIndex;
        if (bi < 0 || bi >= (int)st.ws.blocks.size()) {
            closeFuncIOMenu(st, "invalid index");
            return true;
        }
        Block& b = st.ws.blocks[bi];

        SDL_Rect rowFn  = {box.x + 30, box.y + 80,  box.w - 60, 38};
        SDL_Rect rowIn  = {box.x + 30, box.y + 126, box.w - 60, 38};
        SDL_Rect rowOut = {box.x + 30, box.y + 172, box.w - 60, 38};

        SDL_Rect okBtn  = {box.x + box.w - 180, box.y + box.h - 60, 140, 40};
        SDL_Rect canBtn = {box.x +  40,         box.y + box.h - 60, 140, 40};

        if (pointInRect(mx,my,rowFn)) {
            b.s1 = cycleStrInList(funcList(), b.s1.empty() ? "sqrt" : b.s1);
            st.log.info(bi, "FUNC_APPLY", "Cycle function", "fn=" + b.s1);
            return true;
        }
        if (pointInRect(mx,my,rowIn)) {
            b.inSel = cycleStrInList(funcInputList(), b.inSel.empty() ? "last" : b.inSel);
            st.log.info(bi, "FUNC_APPLY", "Cycle input", "in=" + b.inSel);
            return true;
        }
        if (pointInRect(mx,my,rowOut)) {
            b.outSel = cycleStrInList(funcOutputList(), b.outSel.empty() ? "last" : b.outSel);
            st.log.info(bi, "FUNC_APPLY", "Cycle output", "out=" + b.outSel);
            return true;
        }

        if (pointInRect(mx,my,okBtn)) {
            closeFuncIOMenu(st, "OK");
            return true;
        }
        if (pointInRect(mx,my,canBtn)) {
            closeFuncIOMenu(st, "Cancel");
            return true;
        }

        return true;
    }

    return true;
}

static void renderFuncIOMenu(const AppState& st, SDL_Renderer* r, int w, int h) {
    if (!st.funcIOMenuOpen) return;

    SDL_SetRenderDrawColor(r, 0,0,0,170);
    SDL_Rect full{0,0,w,h};
    SDL_RenderFillRect(r, &full);

    SDL_Rect box = funcIOMenuRect(w,h);
    SDL_SetRenderDrawColor(r, 40,40,46,255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 200,200,200,255);
    SDL_RenderDrawRect(r, &box);

    SDL_Color white{240,240,240,255};
    renderText(r, st.uiFont, "Function I/O Menu (Esc to close)", box.x + 20, box.y + 18, white);
    renderText(r, st.uiFont, "Click each row to cycle options:", box.x + 20, box.y + 44, SDL_Color{200,200,200,255});

    int bi = st.funcIOMenuBlockIndex;
    string fn = "sqrt", inSel = "last", outSel = "last";
    if (bi >= 0 && bi < (int)st.ws.blocks.size()) {
        const Block& b = st.ws.blocks[bi];
        fn = b.s1.empty() ? "sqrt" : b.s1;
        inSel = b.inSel.empty() ? "last" : b.inSel;
        outSel = b.outSel.empty() ? "last" : b.outSel;
    }

    SDL_Rect rowFn  = {box.x + 30, box.y + 80,  box.w - 60, 38};
    SDL_Rect rowIn  = {box.x + 30, box.y + 126, box.w - 60, 38};
    SDL_Rect rowOut = {box.x + 30, box.y + 172, box.w - 60, 38};

    auto drawRow = [&](const SDL_Rect& rc, const string& title, const string& val) {
        SDL_SetRenderDrawColor(r, 25,25,28,255);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, 120,120,120,255);
        SDL_RenderDrawRect(r, &rc);
        renderText(r, st.uiFont, title + ": " + val, rc.x + 12, rc.y + 9, white);
    };

    drawRow(rowFn,  "Function", fn);
    drawRow(rowIn,  "Input",    prettyIO(inSel));
    drawRow(rowOut, "Output",   prettyIO(outSel));

    SDL_Rect okBtn  = {box.x + box.w - 180, box.y + box.h - 60, 140, 40};
    SDL_Rect canBtn = {box.x +  40,         box.y + box.h - 60, 140, 40};

    SDL_SetRenderDrawColor(r, 60,140,70,255);
    SDL_RenderFillRect(r, &okBtn);
    SDL_SetRenderDrawColor(r, 140,60,60,255);
    SDL_RenderFillRect(r, &canBtn);

    SDL_SetRenderDrawColor(r, 20,20,20,255);
    SDL_RenderDrawRect(r, &okBtn);
    SDL_RenderDrawRect(r, &canBtn);

    renderTextCentered(r, st.uiFont, "OK", okBtn, -6, white);
    renderTextCentered(r, st.uiFont, "Enter", okBtn, +10, white);

    renderTextCentered(r, st.uiFont, "Cancel", canBtn, -6, white);
    renderTextCentered(r, st.uiFont, "Esc", canBtn, +10, white);
}

// ---- Left palette: minimal "Code list"
static Button makePaletteBtn(int x, int y, int w, const string& label, const string& sub, function<void()> cb) {
    Button b;
    b.rect = SDL_Rect{x, y, w, 34};
    b.text = label;
    b.sub = sub;
    b.onClick = cb;
    return b;
}

// =========================
// Section 4: Code Menu helpers (visuals + palette + interpreter pieces)
// =========================
static bool isControlCmd(const string& c) {
    return (c == "WAIT" || c == "REPEAT" || c == "END_REPEAT" ||
            c == "FOREVER" || c == "END_FOREVER" ||
            c == "IF" || c == "IFELSE" || c == "ELSE" || c == "END_IF" ||
            c == "WAIT_UNTIL" || c == "REPEAT_UNTIL" || c == "STOP_ALL");
}

static bool isEventCmd(const string& c) {
    return (c == "EVENT_FLAG" || c == "EVENT_KEY" || c == "EVENT_CLICK" ||
            c == "BROADCAST" || c == "WHEN_RECEIVE");
}

static bool isLooksCmd(const string& c) {
    return (c == "SAY" || c == "SAY_T" || c == "THINK" || c == "THINK_T" ||
            c == "SHOW" || c == "HIDE" ||
            c == "SIZE_SET" || c == "SIZE_CHANGE" ||
            c == "FX_COLOR_SET" || c == "FX_COLOR_CHANGE" || c == "FX_CLEAR" ||
            c == "COSTUME_SET" || c == "COSTUME_NEXT" ||
            c == "BACKDROP_SET" || c == "BACKDROP_NEXT");
}

static bool isMotionCmd(const string& c) {
    return (c == "MOVE" || c == "MOVE_STEPS" || c == "TURN_R" || c == "TURN_L" ||
            c == "GOTO_XY" || c == "CHANGE_X" || c == "CHANGE_Y" ||
            c == "SET_DIR" || c == "GOTO_RANDOM" || c == "GOTO_MOUSE" ||
            c == "BOUNCE_EDGE");
}

static bool isSoundCmd(const string& c) {
    return (c == "SOUND_PLAY" || c == "SOUND_PLAY_UNTIL" || c == "SOUND_STOP_ALL" ||
            c == "SOUND_SET_VOL" || c == "SOUND_CHANGE_VOL");
}

static bool isSensingCmd(const string& c) {
    return (c == "TOUCH_EDGE" || c == "TOUCH_MOUSE" || c == "DIST_MOUSE" ||
            c == "KEY_PRESSED" || c == "MOUSE_DOWN" || c == "MOUSE_X" || c == "MOUSE_Y" ||
            c == "ASK" || c == "ANSWER" ||
            c == "TIMER" || c == "RESET_TIMER");
}

static bool isOperatorCmd(const string& c) {
    return (c == "OP_ADD" || c == "OP_SUB" || c == "OP_MUL" || c == "DIV" ||
            c == "OP_EQ" || c == "OP_LT" || c == "OP_GT" ||
            c == "OP_AND" || c == "OP_OR" || c == "OP_NOT" ||
            c == "OP_STRLEN" || c == "OP_LETTER" || c == "OP_JOIN");
}

static bool isVarCmd(const string& c) {
    return (c == "VAR_SET_NUM" || c == "VAR_SET_STR" || c == "VAR_CHANGE" ||
            c == "VAR_SHOW" || c == "VAR_HIDE" || c == "VAR_GET");
}

static bool isFuncCmd(const string& c) {
    return (c == "FUNC_APPLY");
}

static bool isListCmd(const string& c) {
    return (c == "LIST_ADD" || c == "LIST_DELETE" || c == "LIST_CLEAR" ||
            c == "LIST_LENGTH" || c == "LIST_ITEM" || c == "LIST_CONTAINS" ||
            c == "LIST_SHOW" || c == "LIST_HIDE");
}

static bool isCloneCmd(const string& c) {
    return (c == "CLONE_CREATE" || c == "CLONE_DELETE_LAST" || c == "CLONE_CLEAR" || c == "CLONE_COUNT");
}

static void setBlockVisual(Block& b) {
    if (isMotionCmd(b.cmd)) b.color = SDL_Color{60, 150, 220, 255};
    else if (isLooksCmd(b.cmd)) b.color = SDL_Color{120, 90, 200, 255};
    else if (isSoundCmd(b.cmd)) b.color = SDL_Color{190, 70, 170, 255};
    else if (isEventCmd(b.cmd)) b.color = SDL_Color{220, 190, 60, 255};
    else if (isControlCmd(b.cmd)) b.color = SDL_Color{220, 160, 60, 255};
    else if (isSensingCmd(b.cmd)) b.color = SDL_Color{60, 200, 200, 255};
    else if (isOperatorCmd(b.cmd)) b.color = SDL_Color{70, 160, 90, 255};
    else if (isVarCmd(b.cmd)) b.color = SDL_Color{200, 140, 70, 255};
    else if (isFuncCmd(b.cmd)) b.color = SDL_Color{80, 140, 160, 255}; // Section 5
    else if (isListCmd(b.cmd)) b.color = SDL_Color{210, 110, 80, 255};   // Lists
    else if (isCloneCmd(b.cmd)) b.color = SDL_Color{140, 140, 220, 255}; // Clones
    else if (isPenCmd(b.cmd)) b.color = SDL_Color{40, 180, 90, 255};
    else if (b.cmd == "SQRT") b.color = SDL_Color{150, 90, 200, 255};
    else if (b.cmd == "LOOP") b.color = SDL_Color{220, 160, 60, 255};
    else b.color = SDL_Color{80, 80, 90, 255};
}

static void addTypedBlock(AppState& st, const string& cmd, double a, double bb, const string& s1 = "", const string& s2 = "", int i1 = 0) {
    st.ws.addBlock(st.ws.bounds.x + 60, st.ws.bounds.y + 60);
    if (!st.ws.blocks.empty()) {
        Block& b = st.ws.blocks.back();
        b.cmd = cmd;
        b.a = a;
        b.b = bb;
        b.s1 = s1;
        b.s2 = s2;
        b.i1 = i1;
        setBlockVisual(b);
        st.log.info((int)st.ws.blocks.size() - 1, cmd, "Add block",
                    "a=" + to_string(a) + " b=" + to_string(bb) + " s1=" + s1);
    }
}

// =========================
// Palette build (Section 4: Code Menu) + scrolling
// =========================
static void paletteAddCat(AppState& st, int y, const string& label) {
    st.paletteCats.push_back({y, label});
}

static void rebuildPalette(AppState& st, int winW, int winH) {
    (void)winW; (void)winH;
    st.palette.clear();
    st.paletteCats.clear();

    int x = 10;
    int w = LEFT_PANEL_W - 20;

    int contentY = TOP_BAR_H + 64;
    auto placeBtn = [&](const string& label, const string& sub, function<void()> cb) {
        int drawY = contentY - st.paletteScroll;
        st.palette.push_back(makePaletteBtn(x, drawY, w, label, sub, cb));
        contentY += 42;
    };

    auto cat = [&](const string& label) {
        paletteAddCat(st, contentY, label);
        contentY += 28;
    };

    cat("Events");
    placeBtn("when flag clicked", "", [&]{ addTypedBlock(st, "EVENT_FLAG", 0, 0); });
    placeBtn("when key pressed (Space)", "", [&]{ addTypedBlock(st, "EVENT_KEY", 0, 0, "", "", (int)SDL_SCANCODE_SPACE); });
    placeBtn("broadcast (msg1)", "", [&]{ addTypedBlock(st, "BROADCAST", 0, 0, "msg1"); });
    placeBtn("when I receive (msg1)", "", [&]{ addTypedBlock(st, "WHEN_RECEIVE", 0, 0, "msg1"); });

    cat("Motion");
    placeBtn("move 10 steps", "", [&]{ addTypedBlock(st, "MOVE_STEPS", 10.0, 0.0); });
    placeBtn("turn right 15", "", [&]{ addTypedBlock(st, "TURN_R", 15.0, 0.0); });
    placeBtn("turn left 15", "", [&]{ addTypedBlock(st, "TURN_L", 15.0, 0.0); });
    placeBtn("go to x:100 y:100", "", [&]{ addTypedBlock(st, "GOTO_XY", 100.0, 100.0); });
    placeBtn("change x by 10", "", [&]{ addTypedBlock(st, "CHANGE_X", 10.0, 0.0); });
    placeBtn("change y by 10", "", [&]{ addTypedBlock(st, "CHANGE_Y", 10.0, 0.0); });
    placeBtn("point dir 90", "", [&]{ addTypedBlock(st, "SET_DIR", 90.0, 0.0); });
    placeBtn("go to random", "", [&]{ addTypedBlock(st, "GOTO_RANDOM", 0.0, 0.0); });
    placeBtn("go to mouse", "", [&]{ addTypedBlock(st, "GOTO_MOUSE", 0.0, 0.0); });
    placeBtn("if on edge, bounce", "", [&]{ addTypedBlock(st, "BOUNCE_EDGE", 0.0, 0.0); });

    cat("Looks");
    placeBtn("say \"Hello\"", "", [&]{ addTypedBlock(st, "SAY", 0.0, 0.0, "Hello"); });
    placeBtn("say \"Hi\" for 2s", "", [&]{ addTypedBlock(st, "SAY_T", 2.0, 0.0, "Hi"); });
    placeBtn("think \"...\"", "", [&]{ addTypedBlock(st, "THINK", 0.0, 0.0, "..."); });
    placeBtn("show", "", [&]{ addTypedBlock(st, "SHOW", 0.0, 0.0); });
    placeBtn("hide", "", [&]{ addTypedBlock(st, "HIDE", 0.0, 0.0); });
    placeBtn("set size to 100%", "", [&]{ addTypedBlock(st, "SIZE_SET", 100.0, 0.0); });
    placeBtn("change size by 10", "", [&]{ addTypedBlock(st, "SIZE_CHANGE", 10.0, 0.0); });
    placeBtn("set color effect 30", "", [&]{ addTypedBlock(st, "FX_COLOR_SET", 30.0, 0.0); });
    placeBtn("change color effect 10", "", [&]{ addTypedBlock(st, "FX_COLOR_CHANGE", 10.0, 0.0); });
    placeBtn("clear effects", "", [&]{ addTypedBlock(st, "FX_CLEAR", 0.0, 0.0); });

    cat("Sound (minimal)");
    placeBtn("play sound (pop)", "", [&]{ addTypedBlock(st, "SOUND_PLAY", 0.0, 0.0, "pop"); });
    placeBtn("play sound until done", "", [&]{ addTypedBlock(st, "SOUND_PLAY_UNTIL", 0.0, 0.0, "pop"); });
    placeBtn("stop all sounds", "", [&]{ addTypedBlock(st, "SOUND_STOP_ALL", 0.0, 0.0); });
    placeBtn("set volume 80", "", [&]{ addTypedBlock(st, "SOUND_SET_VOL", 80.0, 0.0); });
    placeBtn("change volume -10", "", [&]{ addTypedBlock(st, "SOUND_CHANGE_VOL", -10.0, 0.0); });

    cat("Control");
    placeBtn("wait 0.5s", "", [&]{ addTypedBlock(st, "WAIT", 0.5, 0.0); });
    placeBtn("repeat 5", "", [&]{ addTypedBlock(st, "REPEAT", 5.0, 0.0); });
    placeBtn("end repeat", "", [&]{ addTypedBlock(st, "END_REPEAT", 0.0, 0.0); });
    placeBtn("forever", "", [&]{ addTypedBlock(st, "FOREVER", 0.0, 0.0); });
    placeBtn("end forever", "", [&]{ addTypedBlock(st, "END_FOREVER", 0.0, 0.0); });
    placeBtn("if (uses last op)", "", [&]{ addTypedBlock(st, "IF", 0.0, 0.0); });
    placeBtn("if/else (uses last op)", "", [&]{ addTypedBlock(st, "IFELSE", 0.0, 0.0); });
    placeBtn("else", "", [&]{ addTypedBlock(st, "ELSE", 0.0, 0.0); });
    placeBtn("end if", "", [&]{ addTypedBlock(st, "END_IF", 0.0, 0.0); });
    placeBtn("wait until (last op)", "", [&]{ addTypedBlock(st, "WAIT_UNTIL", 0.0, 0.0); });
    placeBtn("repeat until (last op)", "", [&]{ addTypedBlock(st, "REPEAT_UNTIL", 0.0, 0.0); });
    placeBtn("stop all (script)", "", [&]{ addTypedBlock(st, "STOP_ALL", 0.0, 0.0); });

    cat("Sensing");
    placeBtn("touching edge?", "", [&]{ addTypedBlock(st, "TOUCH_EDGE", 0.0, 0.0); });
    placeBtn("touching mouse?", "", [&]{ addTypedBlock(st, "TOUCH_MOUSE", 0.0, 0.0); });
    placeBtn("distance to mouse", "", [&]{ addTypedBlock(st, "DIST_MOUSE", 0.0, 0.0); });
    placeBtn("key (Space) pressed?", "", [&]{ addTypedBlock(st, "KEY_PRESSED", 0.0, 0.0, "", "", (int)SDL_SCANCODE_SPACE); });
    placeBtn("mouse down?", "", [&]{ addTypedBlock(st, "MOUSE_DOWN", 0.0, 0.0); });
    placeBtn("mouse x", "", [&]{ addTypedBlock(st, "MOUSE_X", 0.0, 0.0); });
    placeBtn("mouse y", "", [&]{ addTypedBlock(st, "MOUSE_Y", 0.0, 0.0); });
    placeBtn("ask \"Name?\"", "", [&]{ addTypedBlock(st, "ASK", 0.0, 0.0, "Name?"); });
    placeBtn("answer", "", [&]{ addTypedBlock(st, "ANSWER", 0.0, 0.0); });
    placeBtn("timer", "", [&]{ addTypedBlock(st, "TIMER", 0.0, 0.0); });
    placeBtn("reset timer", "", [&]{ addTypedBlock(st, "RESET_TIMER", 0.0, 0.0); });

    cat("Operators");
    placeBtn("add (3+4)", "", [&]{ addTypedBlock(st, "OP_ADD", 3.0, 4.0); });
    placeBtn("sub (10-2)", "", [&]{ addTypedBlock(st, "OP_SUB", 10.0, 2.0); });
    placeBtn("mul (6*7)", "", [&]{ addTypedBlock(st, "OP_MUL", 6.0, 7.0); });
    placeBtn("div (10/2)", "", [&]{ addTypedBlock(st, "DIV", 10.0, 2.0); });
    placeBtn("eq (5==5)", "", [&]{ addTypedBlock(st, "OP_EQ", 5.0, 5.0); });
    placeBtn("lt (3<9)", "", [&]{ addTypedBlock(st, "OP_LT", 3.0, 9.0); });
    placeBtn("gt (9>3)", "", [&]{ addTypedBlock(st, "OP_GT", 9.0, 3.0); });
    placeBtn("and (1 and 0)", "", [&]{ addTypedBlock(st, "OP_AND", 1.0, 0.0); });
    placeBtn("or (0 or 1)", "", [&]{ addTypedBlock(st, "OP_OR", 0.0, 1.0); });
    placeBtn("not (0)", "", [&]{ addTypedBlock(st, "OP_NOT", 0.0, 0.0); });
    placeBtn("len(\"abc\")", "", [&]{ addTypedBlock(st, "OP_STRLEN", 0.0, 0.0, "abc"); });
    placeBtn("letter 2 of \"abc\"", "", [&]{ addTypedBlock(st, "OP_LETTER", 2.0, 0.0, "abc"); });
    placeBtn("join \"a\" \"b\"", "", [&]{ addTypedBlock(st, "OP_JOIN", 0.0, 0.0, "a", "b"); });

    // ===== Section 5: Functions =====
    cat("Functions");
    placeBtn("apply function (sqrt)", "Shift+Click edit I/O", [&]{
        addTypedBlock(st, "FUNC_APPLY", 0.0, 0.0, "sqrt");
        if (!st.ws.blocks.empty()) {
            Block& b = st.ws.blocks.back();
            b.inSel = "last";
            b.outSel = "last";
            setBlockVisual(b);
        }
    });

    cat("Variables");
    placeBtn("set v = 10", "Shift+Click cycle name", [&]{ addTypedBlock(st, "VAR_SET_NUM", 10.0, 0.0, "v"); });
    placeBtn("set msg = \"hi\"", "Shift+Click cycle name", [&]{ addTypedBlock(st, "VAR_SET_STR", 0.0, 0.0, "msg", "hi"); });
    placeBtn("change v by 1", "Shift+Click cycle name", [&]{ addTypedBlock(st, "VAR_CHANGE", 1.0, 0.0, "v"); });
    placeBtn("show v", "", [&]{ addTypedBlock(st, "VAR_SHOW", 0.0, 0.0, "v"); });
    placeBtn("hide v", "", [&]{ addTypedBlock(st, "VAR_HIDE", 0.0, 0.0, "v"); });
    placeBtn("get v (to last)", "Shift+Click cycle name", [&]{ addTypedBlock(st, "VAR_GET", 0.0, 0.0, "v"); });

    // ===== Section 8: Lists =====
    cat("Lists");
    placeBtn("add \"hi\" to list", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_ADD", 0.0, 0.0, "list", "hi"); });
    placeBtn("add 1 to list", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_ADD", 1.0, 0.0, "list", ""); });
    placeBtn("delete 1 of list", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_DELETE", 1.0, 0.0, "list"); });
    placeBtn("clear list", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_CLEAR", 0.0, 0.0, "list"); });
    placeBtn("length of list (to last)", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_LENGTH", 0.0, 0.0, "list"); });
    placeBtn("item 1 of list (to last)", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_ITEM", 1.0, 0.0, "list"); });
    placeBtn("list contains \"hi\"? (to last)", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_CONTAINS", 0.0, 0.0, "list", "hi"); });
    placeBtn("show list", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_SHOW", 0.0, 0.0, "list"); });
    placeBtn("hide list", "Shift+Click cycle name", [&]{ addTypedBlock(st, "LIST_HIDE", 0.0, 0.0, "list"); });

    // ===== Section 9: Clones =====
    cat("Clones");
    placeBtn("create clone", "", [&]{ addTypedBlock(st, "CLONE_CREATE", 0.0, 0.0); });
    placeBtn("delete last clone", "", [&]{ addTypedBlock(st, "CLONE_DELETE_LAST", 0.0, 0.0); });
    placeBtn("clear clones", "", [&]{ addTypedBlock(st, "CLONE_CLEAR", 0.0, 0.0); });
    placeBtn("clone count (to last)", "", [&]{ addTypedBlock(st, "CLONE_COUNT", 0.0, 0.0); });

    // Pen at end
    if (st.penExtensionEnabled) {
        cat("Pen (Extension)");
        placeBtn("PEN Down", "", [&]{ addTypedBlock(st, "PEN_DOWN", 0.0, 0.0); });
        placeBtn("PEN Up", "", [&]{ addTypedBlock(st, "PEN_UP", 0.0, 0.0); });
        placeBtn("Stamp", "", [&]{ addTypedBlock(st, "PEN_STAMP", 0.0, 0.0); });
        placeBtn("All Erase", "", [&]{ addTypedBlock(st, "PEN_ERASE_ALL", 0.0, 0.0); });
        placeBtn("Set Attr (Shift=cycle)", "", [&]{
            addTypedBlock(st, "PEN_SET_ATTR", 120.0, 0.0);
            if (!st.ws.blocks.empty()) st.ws.blocks.back().opt = "COLOR";
            setBlockVisual(st.ws.blocks.back());
        });
        placeBtn("Change Attr (+10)", "", [&]{
            addTypedBlock(st, "PEN_CHANGE_ATTR", 10.0, 0.0);
            if (!st.ws.blocks.empty()) st.ws.blocks.back().opt = "BRI";
            setBlockVisual(st.ws.blocks.back());
        });
        placeBtn("Set Size (3)", "", [&]{ addTypedBlock(st, "PEN_SET_SIZE", 3.0, 0.0); });
        placeBtn("Change Size (+1)", "", [&]{ addTypedBlock(st, "PEN_CHANGE_SIZE", 1.0, 0.0); });
        placeBtn("Set Color (picker)", "Shift+Click edit", [&]{
            addTypedBlock(st, "PEN_SET_COLOR", 0.0, 0.0);
            if (!st.ws.blocks.empty()) {
                st.ws.blocks.back().pickColor = st.penRGB;
                setBlockVisual(st.ws.blocks.back());
                st.penColorPickerOpen = true;
                st.penColorPickerBlockIndex = (int)st.ws.blocks.size() - 1;
            }
        });
    }

    // scroll limits
    int visibleH = (winH - TOP_BAR_H) - 10;
    int contentEnd = contentY + 10;
    st.paletteMaxScroll = max(0, contentEnd - (TOP_BAR_H + visibleH));
    st.paletteScroll = clampT(st.paletteScroll, 0, st.paletteMaxScroll);

    st.paletteDirty = false;
}

static void renderPaletteHeader(const AppState& st, SDL_Renderer* r) {
    SDL_Color white{240,240,240,255};
    renderText(r, st.uiFont, "Code", 12, TOP_BAR_H + 6, white);
    if (st.penExtensionEnabled) renderText(r, st.uiFont, "Pen: enabled", 12, TOP_BAR_H + 28, SDL_Color{40,180,90,255});
    else                       renderText(r, st.uiFont, "Pen: Extensions (E)", 12, TOP_BAR_H + 28, SDL_Color{160,160,160,255});

    renderText(r, st.uiFont, "Scroll wheel", 12, TOP_BAR_H + 46, SDL_Color{120,120,120,255});
}

static void renderPaletteCats(const AppState& st, SDL_Renderer* r) {
    SDL_Color c{210,210,210,255};
    for (auto& it : st.paletteCats) {
        int y = it.first - st.paletteScroll;
        if (y < TOP_BAR_H + 55 || y > st.ws.bounds.y + st.ws.bounds.h) continue;
        renderText(r, st.uiFont, it.second, 12, y, c);
    }
}

// =========================
// Help Menu + Logs Panel
// =========================
static SDL_Rect helpMenuRect(const AppState& st) {
    SDL_Rect r;
    r.x = st.helpButtonRect.x;
    r.y = TOP_BAR_H - 2;
    r.w = 260;
    r.h = 32 * 3 + 10;
    return r;
}

static SDL_Rect helpMenuItemRect(const SDL_Rect& menu, int i) {
    SDL_Rect r = {menu.x + 5, menu.y + 5 + i * 32, menu.w - 10, 28};
    return r;
}

static bool updateHelpMenu(AppState& st) {
    if (!st.helpMenuOpen) return false;

    SDL_Rect menu = helpMenuRect(st);

    if (st.in.mousePressed) {
        if (!pointInRect(st.in.mx, st.in.my, menu)) {
            st.helpMenuOpen = false;
            st.log.info(-1, "HELP", "Close menu", "");
            return true;
        }

        SDL_Rect i0 = helpMenuItemRect(menu, 0);
        SDL_Rect i1 = helpMenuItemRect(menu, 1);
        SDL_Rect i2 = helpMenuItemRect(menu, 2);

        if (pointInRect(st.in.mx, st.in.my, i0)) {
            st.showLogsPanel = !st.showLogsPanel;
            st.logsScroll = 0;
            st.helpMenuOpen = false;
            st.log.info(-1, "HELP", "Show Logs", st.showLogsPanel ? "ON" : "OFF");
            return true;
        }
        if (pointInRect(st.in.mx, st.in.my, i1)) {
            st.log.clear();
            st.helpMenuOpen = false;
            st.log.info(-1, "HELP", "Clear Logs", "done");
            return true;
        }
        if (pointInRect(st.in.mx, st.in.my, i2)) {
            st.debugStepMode = !st.debugStepMode;
            st.helpMenuOpen = false;
            st.log.info(-1, "HELP", "Toggle Step-by-Step", st.debugStepMode ? "ON" : "OFF");
            return true;
        }
    }

    return false;
}

static void renderHelpMenu(const AppState& st, SDL_Renderer* r) {
    if (!st.helpMenuOpen) return;

    SDL_Rect menu = helpMenuRect(st);

    SDL_SetRenderDrawColor(r, 44, 44, 50, 255);
    SDL_RenderFillRect(r, &menu);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &menu);

    SDL_Color white = {240, 240, 240, 255};

    SDL_Rect i0 = helpMenuItemRect(menu, 0);
    SDL_Rect i1 = helpMenuItemRect(menu, 1);
    SDL_Rect i2 = helpMenuItemRect(menu, 2);

    SDL_SetRenderDrawColor(r, 35, 35, 40, 255);
    SDL_RenderFillRect(r, &i0);
    SDL_RenderFillRect(r, &i1);
    SDL_RenderFillRect(r, &i2);

    SDL_SetRenderDrawColor(r, 15, 15, 15, 255);
    SDL_RenderDrawRect(r, &i0);
    SDL_RenderDrawRect(r, &i1);
    SDL_RenderDrawRect(r, &i2);

    string s0 = string("Show Logs  ") + (st.showLogsPanel ? "[ON]" : "[OFF]");
    string s1 = "Clear Logs";
    string s2 = string("Toggle Step-by-Step  ") + (st.debugStepMode ? "[ON]" : "[OFF]");

    renderText(r, st.uiFont, s0, i0.x + 10, i0.y + 5, white);
    renderText(r, st.uiFont, s1, i1.x + 10, i1.y + 5, white);
    renderText(r, st.uiFont, s2, i2.x + 10, i2.y + 5, white);
}

static void updateLogsPanel(AppState& st) {
    if (!st.showLogsPanel) return;
}

static void renderLogsPanel(const AppState& st, SDL_Renderer* r, int winW, int winH) {
    if (!st.showLogsPanel) return;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, winW, winH};
    SDL_RenderFillRect(r, &full);

    SDL_Rect box = {winW/2 - 420, winH/2 - 240, 840, 480};
    SDL_SetRenderDrawColor(r, 40, 40, 46, 255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &box);

    SDL_Color white = {240, 240, 240, 255};
    renderText(r, st.uiFont, "Logs (Esc to close)", box.x + 16, box.y + 12, white);

    SDL_Rect area = {box.x + 16, box.y + 44, box.w - 32, box.h - 60};
    SDL_SetRenderDrawColor(r, 25, 25, 28, 255);
    SDL_RenderFillRect(r, &area);
    SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
    SDL_RenderDrawRect(r, &area);

    const vector<string>& lines = st.log.lines();
    const int lineH = 18;
    int visible = area.h / lineH;

    int end = (int)lines.size();
    int start = max(0, end - visible);

    int y = area.y + 6;
    for (int i = start; i < end; i++) {
        renderText(r, st.uiFont, lines[i], area.x + 8, y, white);
        y += lineH;
        if (y > area.y + area.h - lineH) break;
    }
}

// =========================
// Minimal Script Runner (extended for Code Menu)
// =========================
static void stopScript(AppState& st, const string& reason, const string& level = "WARNING") {
    if (!st.scriptRunning) return;

    st.scriptRunning = false;

    // reset yield-wait state
    st.waiting = false;
    st.waitUntilMs = 0;

    // reset sound wait state
    st.soundBusyUntilMs = 0;

    if (level == "ERROR") st.log.error(st.scriptPC, "RUN", "Stop script", reason);
    else                  st.log.warn(st.scriptPC, "RUN", "Stop script", reason);
}

static bool safeDiv(AppState& st, int blockIndex, double a, double b, double& out) {
    if (b == 0.0) {
        st.log.error(blockIndex, "DIV", "Divide by zero prevented",
                     "a=" + to_string(a) + " b=" + to_string(b));
        fatalBox("Math Error", "Division by zero prevented.");
        return false;
    }
    out = a / b;
    return true;
}

static bool safeSqrt(AppState& st, int blockIndex, double v, double& out) {
    if (v < 0.0) {
        st.log.error(blockIndex, "SQRT", "sqrt(negative) prevented",
                     "v=" + to_string(v));
        fatalBox("Math Error", "sqrt of negative prevented.");
        return false;
    }
    out = std::sqrt(v);
    return true;
}

static void clampActorPos(AppState& st, int blockIndex, const string& cmd, double beforeX, double beforeY) {
    double minX = st.ws.bounds.x;
    double maxX = st.ws.bounds.x + st.ws.bounds.w;
    double minY = st.ws.bounds.y;
    double maxY = st.ws.bounds.y + st.ws.bounds.h;

    double ox = st.actorX, oy = st.actorY;

    if (st.actorX < minX) st.actorX = minX;
    if (st.actorX > maxX) st.actorX = maxX;
    if (st.actorY < minY) st.actorY = minY;
    if (st.actorY > maxY) st.actorY = maxY;

    bool clamped = (st.actorX != ox) || (st.actorY != oy);
    if (clamped) {
        st.log.warn(blockIndex, cmd, "Boundary clamp",
                    "pos(" + to_string(beforeX) + "," + to_string(beforeY) + ")->(" +
                    to_string(st.actorX) + "," + to_string(st.actorY) + ")");
    }
}

static double degToRad(double d) { return d * 3.14159265358979323846 / 180.0; }

static void runnerPreScan(AppState& st) {
    int n = (int)st.ws.blocks.size();
    st.jumpTo.assign(n, -1);
    st.jumpElse.assign(n, -1);
    st.jumpEnd.assign(n, -1);
    st.loopEnd.assign(n, -1);
    st.loopStart.assign(n, -1);
    st.repeatCounter.assign(n, 0);

    vector<int> ifStack;
    vector<int> ifElseStack;
    vector<int> repeatStack;
    vector<int> foreverStack;
    vector<int> repeatUntilStack;

    for (int i = 0; i < n; i++) {
        const string& c = st.ws.blocks[i].cmd;

        if (c == "IF" || c == "IFELSE") {
            ifStack.push_back(i);
            if (c == "IFELSE") ifElseStack.push_back(i);
            continue;
        }

        if (c == "ELSE") {
            if (!ifStack.empty()) {
                int start = ifStack.back();
                st.jumpElse[start] = i;
            }
            continue;
        }

        if (c == "END_IF") {
            if (!ifStack.empty()) {
                int start = ifStack.back();
                ifStack.pop_back();
                st.jumpEnd[start] = i;
                if (st.jumpElse[start] != -1) {
                    int elseIdx = st.jumpElse[start];
                    st.jumpTo[elseIdx] = i;
                }
            }
            continue;
        }

        if (c == "REPEAT") {
            repeatStack.push_back(i);
            continue;
        }
        if (c == "REPEAT_UNTIL") {
            repeatUntilStack.push_back(i);
            continue;
        }
        if (c == "END_REPEAT") {
            if (!repeatUntilStack.empty() && (repeatStack.empty() || repeatUntilStack.back() > repeatStack.back())) {
                int start = repeatUntilStack.back();
                repeatUntilStack.pop_back();
                st.loopEnd[start] = i;
                st.loopStart[i] = start;
            } else if (!repeatStack.empty()) {
                int start = repeatStack.back();
                repeatStack.pop_back();
                st.loopEnd[start] = i;
                st.loopStart[i] = start;
            }
            continue;
        }

        if (c == "FOREVER") {
            foreverStack.push_back(i);
            continue;
        }
        if (c == "END_FOREVER") {
            if (!foreverStack.empty()) {
                int start = foreverStack.back();
                foreverStack.pop_back();
                st.loopEnd[start] = i;
                st.loopStart[i] = start;
            }
            continue;
        }
    }

    for (int idx : ifStack) st.log.warn(idx, "IF", "Unmatched IF (missing END_IF)", "");
    for (int idx : repeatStack) st.log.warn(idx, "REPEAT", "Unmatched REPEAT (missing END_REPEAT)", "");
    for (int idx : repeatUntilStack) st.log.warn(idx, "REPEAT_UNTIL", "Unmatched REPEAT_UNTIL (missing END_REPEAT)", "");
    for (int idx : foreverStack) st.log.warn(idx, "FOREVER", "Unmatched FOREVER (missing END_FOREVER)", "");
}

static void startScript(AppState& st) {
    st.scriptRunning = true;
    st.scriptPC = 0;
    st.stepRequested = false;

    st.nextStepAtMs = SDL_GetTicks();
    st.waiting = false;
    st.waitUntilMs = 0;

    // reset yield-wait state
    st.waiting = false;
    st.waitUntilMs = 0;

    st.actorX = st.ws.bounds.x + st.ws.bounds.w * 0.5;
    st.actorY = st.ws.bounds.y + st.ws.bounds.h * 0.5;
    st.actorDirDeg = 90.0;
    st.actorVisible = true;
    st.actorSizePct = 100.0;
    st.lookColorEffect = 0.0;

    st.lastValue = Value::Num(0.0);
    st.bubbleText.clear();
    st.bubbleUntilMs = 0;

    st.timerStartMs = SDL_GetTicks();

    st.clones.clear();

    // reset sound wait state
    st.soundBusyUntilMs = 0;

    runnerPreScan(st);

    st.log.info(0, "RUN", "Start script", "blocks=" + to_string((int)st.ws.blocks.size()));
    if (st.debugStepMode) {
        st.log.info(0, "DEBUG", "Step-by-step ON", "Press Space to run next block");
    }
}

static SDL_Color applyLookEffect(SDL_Color base, double hueShiftDeg) {
    double h,s,v;
    rgbToHsv(base, h,s,v);
    h = fmod(h + hueShiftDeg, 360.0);
    if (h < 0) h += 360.0;
    return hsvToRgb(h,s,v);
}

// =========================
// Section 6: Asset helpers (BMP icon)
// =========================
static void destroyTextureAsset(TextureAsset& a) {
    if (a.tex) SDL_DestroyTexture(a.tex);
    a.tex = nullptr; a.w = 0; a.h = 0;
}

static TextureAsset loadBMPTexture(SDL_Renderer* r, const string& path, Logger& log) {
    TextureAsset out;
    SDL_Surface* s = SDL_LoadBMP(path.c_str());
    if (!s) {
        log.warn(-1, "ASSET", "SDL_LoadBMP failed", path + " err=" + SDL_GetError());
        return out;
    }

    // optional: make magenta (255,0,255) transparent if you want
    Uint32 key = SDL_MapRGB(s->format, 255, 0, 255);
    SDL_SetColorKey(s, SDL_TRUE, key);

    out.w = s->w; out.h = s->h;
    out.tex = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);

    if (!out.tex) {
        log.warn(-1, "ASSET", "CreateTextureFromSurface failed", path + " err=" + SDL_GetError());
        out.w = out.h = 0;
        return out;
    }

    SDL_SetTextureBlendMode(out.tex, SDL_BLENDMODE_BLEND);
    log.info(-1, "ASSET", "Loaded BMP texture", path);
    return out;
}

static void initAssets(AppState& st, SDL_Renderer* r) {
    // main actor icon (fallback)
    destroyTextureAsset(st.actorIcon);
    st.actorIcon = loadBMPTexture(r, st.actorIconFile, st.log);

    // load costumes موجود در پروژه
    st.costumes.clear();
    st.costumes.push_back(loadBMPTexture(r, "costume0.bmp", st.log));
    st.costumes.push_back(loadBMPTexture(r, "costume1.bmp", st.log));

    // load backdrops موجود در پروژه
    st.backdrops.clear();
    st.backdrops.push_back(loadBMPTexture(r, "backdrop0.bmp", st.log));
    st.backdrops.push_back(loadBMPTexture(r, "backdrop1.bmp", st.log));
}

static void shutdownAssets(AppState& st) {
    destroyTextureAsset(st.actorIcon);

    for (auto& t : st.costumes) destroyTextureAsset(t);
    st.costumes.clear();

    for (auto& t : st.backdrops) destroyTextureAsset(t);
    st.backdrops.clear();
}

// =========================
// Section 7: Audio helpers (WAV via SDL_QueueAudio) - minimal
// =========================
static uint32_t computeAudioMs(const SDL_AudioSpec& spec, uint32_t lenBytes) {
    int bytesPerSample = (SDL_AUDIO_BITSIZE(spec.format) / 8) * (int)spec.channels;
    if (bytesPerSample <= 0 || spec.freq <= 0) return 0;
    double samples = (double)lenBytes / (double)bytesPerSample;
    double sec = samples / (double)spec.freq;
    if (sec < 0) sec = 0;
    return (uint32_t)llround(sec * 1000.0);
}

static bool initAudioSystem(AppState& st) {
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 4096;
    want.callback = nullptr;

    SDL_AudioSpec have{};
    st.audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!st.audioDev) {
        st.audioReady = false;
        st.log.warn(-1, "AUDIO", "OpenAudioDevice failed", SDL_GetError());
        return false;
    }

    st.audioSpec = have;
    st.audioReady = true;
    SDL_PauseAudioDevice(st.audioDev, 0);

    st.log.info(-1, "AUDIO", "Audio ready",
                "freq=" + to_string(have.freq) + " ch=" + to_string((int)have.channels));
    return true;
}

static void shutdownAudioSystem(AppState& st) {
    if (st.audioDev) {
        SDL_ClearQueuedAudio(st.audioDev);
        SDL_CloseAudioDevice(st.audioDev);
    }
    st.audioDev = 0;
    st.audioReady = false;
    st.soundBusyUntilMs = 0;
}



static uint32_t playWavOneShot(AppState& st, const string& wavFile) {
    if (!st.audioReady || !st.audioDev) {
        st.log.warn(-1, "AUDIO", "Audio not ready", "skip " + wavFile);
        return 0;
    }

    SDL_AudioSpec srcSpec{};
    Uint8* srcBuf = nullptr;
    Uint32 srcLen = 0;

    if (!SDL_LoadWAV(wavFile.c_str(), &srcSpec, &srcBuf, &srcLen)) {
        st.log.warn(-1, "AUDIO", "LoadWAV failed", wavFile + " err=" + SDL_GetError());
        return 0;
    }

    Uint8* buf = srcBuf;
    Uint32 len = srcLen;
    Uint8* cvtBuf = nullptr;

    // convert if needed
    if (srcSpec.format != st.audioSpec.format ||
        srcSpec.channels != st.audioSpec.channels ||
        srcSpec.freq != st.audioSpec.freq) {

        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt,
                              srcSpec.format, srcSpec.channels, srcSpec.freq,
                              st.audioSpec.format, st.audioSpec.channels, st.audioSpec.freq) >= 0 && cvt.needed) {

            cvt.len = (int)srcLen;
            cvtBuf = (Uint8*)SDL_malloc((size_t)cvt.len * (size_t)cvt.len_mult);
            if (cvtBuf) {
                SDL_memcpy(cvtBuf, srcBuf, srcLen);
                cvt.buf = cvtBuf;

                if (SDL_ConvertAudio(&cvt) == 0) {
                    buf = cvt.buf;
                    len = (Uint32)cvt.len_cvt;
                } else {
                    st.log.warn(-1, "AUDIO", "ConvertAudio failed", wavFile);
                }
            }
        }
    }

    int vol = st.soundMuted ? 0 : clampT(st.soundVolume, 0, 100);
    if (vol <= 0) {
        SDL_FreeWAV(srcBuf);
        if (cvtBuf) SDL_free(cvtBuf);
        st.log.info(-1, "AUDIO", "Muted/zero volume", wavFile);
        return 0;
    }

    // simple: single sound at a time
    SDL_ClearQueuedAudio(st.audioDev);

    Uint8* mixBuf = (Uint8*)SDL_malloc(len);
    if (!mixBuf) {
        SDL_FreeWAV(srcBuf);
        if (cvtBuf) SDL_free(cvtBuf);
        st.log.warn(-1, "AUDIO", "malloc failed", "len=" + to_string(len));
        return 0;
    }
    SDL_memset(mixBuf, 0, len);

    int sdlVol = (int)llround((vol / 100.0) * SDL_MIX_MAXVOLUME); // 0..128
    SDL_MixAudioFormat(mixBuf, buf, st.audioSpec.format, len, sdlVol);
    SDL_QueueAudio(st.audioDev, mixBuf, len);

    uint32_t ms = computeAudioMs(st.audioSpec, len);
    st.log.info(-1, "AUDIO", "Play WAV", wavFile + " ms=" + to_string(ms));

    SDL_free(mixBuf);
    SDL_FreeWAV(srcBuf);
    if (cvtBuf) SDL_free(cvtBuf);

    return ms;
}

// =========================
// Section 8: Lists helpers (minimal)
// =========================
static vector<Value>& getList(AppState& st, const string& name) {
    return st.lists[name]; // auto-create if missing
}

static Value listItemFromBlock(const Block& b) {
    // if s2 is set => string item, else numeric item from a
    if (!b.s2.empty()) return Value::Str(b.s2);
    return Value::Num(b.a);
}

static bool listIndexOk1(int idx1, int n) {
    return (idx1 >= 1 && idx1 <= n);
}

// =========================
// Section 9: Clone helpers (minimal)
// =========================
static void cloneCreateFromActor(AppState& st) {
    CloneSprite c;
    c.x = st.actorX;
    c.y = st.actorY;
    c.dirDeg = st.actorDirDeg;
    c.visible = st.actorVisible;
    c.sizePct = st.actorSizePct;
    st.clones.push_back(c);
}

static void cloneDeleteLast(AppState& st) {
    if (!st.clones.empty()) st.clones.pop_back();
}

static void cloneClearAll(AppState& st) {
    st.clones.clear();
}

// =========================
// Shared: draw actor/clones using icon if available
// =========================
static void drawSprite(SDL_Renderer* r, const AppState& st,
                       double x, double y, double dirDeg, double sizePct, bool visible) {
    if (!visible) return;

    int sizePx = (int)clampT((int)round(80.0 * (sizePct / 100.0)), 10, 300);
    SDL_Rect dst{(int)round(x) - sizePx/2, (int)round(y) - sizePx/2, sizePx, sizePx};

    SDL_Texture* useTex = nullptr;

    // اولویت با costume
    if (!st.costumes.empty()) {
        int ci = st.costumeIndex;
        if (ci < 0) ci = 0;
        ci %= (int)st.costumes.size();
        if (st.costumes[ci].tex) useTex = st.costumes[ci].tex;
    }

    // fallback: actorIconFile (costume0.bmp) یا مربع
    if (!useTex && st.actorIcon.tex) useTex = st.actorIcon.tex;

    if (useTex) {
        SDL_RenderCopy(r, useTex, nullptr, &dst);
        SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
        SDL_RenderDrawRect(r, &dst);
    } else {
        SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
        SDL_RenderFillRect(r, &dst);
        SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
        SDL_RenderDrawRect(r, &dst);
    }

    // جهت (فلش)
    double rad = degToRad(dirDeg);
    int x2 = (int)round(x + cos(rad) * (sizePx/2 + 10));
    int y2 = (int)round(y - sin(rad) * (sizePx/2 + 10));
    SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
    SDL_RenderDrawLine(r, (int)round(x), (int)round(y), x2, y2);
}

// =========================
// Section 5: Function I/O (runtime helpers)
// =========================
static double funcReadInput(AppState& st, const string& inSel) {
    if (inSel == "last") return asNum(st.lastValue);
    if (inSel == "mouseX") return (double)st.in.mx;
    if (inSel == "mouseY") return (double)st.in.my;
    if (inSel == "timer")  return (SDL_GetTicks() - st.timerStartMs) / 1000.0;
    if (inSel == "answer") {
        char* endp = nullptr;
        double x = strtod(st.lastAnswer.c_str(), &endp);
        if (endp && endp != st.lastAnswer.c_str()) return x;
        return 0.0;
    }
    if (inSel == "actorX") return st.actorX;
    if (inSel == "actorY") return st.actorY;
    if (inSel == "dir")    return st.actorDirDeg;

    // otherwise treat as variable name
    if (st.vars.count(inSel)) return asNum(st.vars[inSel]);
    return 0.0;
}

static void funcWriteOutput(AppState& st, const string& outSel, double v) {
    if (outSel == "last") {
        st.lastValue = Value::Num(v);
        return;
    }
    if (outSel == "msg") {
        ostringstream ss; ss << v;
        st.vars["msg"] = Value::Str(ss.str());
        return;
    }
    // v / score / any name: numeric
    st.vars[outSel] = Value::Num(v);
}

static bool funcApplyBuiltin(AppState& st, int blockIndex, const string& fn, double x, double& out) {
    if (fn == "sqrt") {
        return safeSqrt(st, blockIndex, x, out);
    }
    if (fn == "abs") {
        out = fabs(x);
        return true;
    }
    if (fn == "sin") {
        out = sin(degToRad(x)); // degrees (Scratch-like)
        return true;
    }
    if (fn == "cos") {
        out = cos(degToRad(x));
        return true;
    }
    if (fn == "tan") {
        out = tan(degToRad(x));
        return true;
    }
    if (fn == "round") {
        out = (double)llround(x);
        return true;
    }
    if (fn == "floor") {
        out = floor(x);
        return true;
    }
    if (fn == "ceil") {
        out = ceil(x);
        return true;
    }

    // unknown => passthrough
    out = x;
    return true;
}

static string cycleName3(const string& cur) {
    if (cur == "v") return "score";
    if (cur == "score") return "msg";
    return "v";
}

static string cycleListName3(const string& cur) {
    if (cur == "list") return "list2";
    if (cur == "list2") return "list3";
    return "list";
}

enum class StepResult { Advanced, Yielded, Stopped };

static StepResult executeOneBlock(AppState& st) {
    if (!st.scriptRunning) return StepResult::Stopped;
    if (st.askDialogOpen)  return StepResult::Yielded;

    // --- Non-blocking WAIT support
    if (st.waiting) {
        if (SDL_GetTicks() < st.waitUntilMs) return StepResult::Yielded;
        st.waiting = false;
        st.waitUntilMs = 0;
    }

    if (st.scriptPC < 0 || st.scriptPC >= (int)st.ws.blocks.size()) {
        stopScript(st, "Reached end", "WARNING");
        return StepResult::Stopped;
    }

    Block& b = st.ws.blocks[st.scriptPC];
    int idx = st.scriptPC;
    string cmd = b.cmd;

    // ---- Events (minimal behavior)
    if (cmd == "EVENT_FLAG" || cmd == "EVENT_KEY" || cmd == "EVENT_CLICK") {
        st.log.info(idx, cmd, "Event block", "pass");
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "BROADCAST") {
        st.lastBroadcast = b.s1.empty() ? "msg" : b.s1;
        st.log.info(idx, cmd, "Broadcast", "msg=" + st.lastBroadcast);
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "WHEN_RECEIVE") {
        st.log.info(idx, cmd, "When receive", "msg=" + b.s1);
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Motion
    if (cmd == "MOVE") {
        double bx = st.actorX, by = st.actorY;
        st.actorX += b.a;
        clampActorPos(st, idx, cmd, bx, by);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, st.actorX, st.actorY);

        st.log.info(idx, cmd, "Change X", "x:" + to_string(bx) + "->" + to_string(st.actorX));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "MOVE_STEPS") {
        double bx = st.actorX, by = st.actorY;
        double rad = degToRad(st.actorDirDeg);
        st.actorX += cos(rad) * b.a;
        st.actorY -= sin(rad) * b.a;
        clampActorPos(st, idx, cmd, bx, by);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, st.actorX, st.actorY);

        st.log.info(idx, cmd, "Move steps",
                    "pos(" + to_string(bx) + "," + to_string(by) + ")->(" +
                    to_string(st.actorX) + "," + to_string(st.actorY) + ")");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "TURN_R" || cmd == "TURN_L") {
        double before = st.actorDirDeg;
        double delta = (cmd == "TURN_R") ? b.a : -b.a;
        st.actorDirDeg = fmod(st.actorDirDeg + delta, 360.0);
        if (st.actorDirDeg < 0) st.actorDirDeg += 360.0;
        st.log.info(idx, cmd, "Turn", "dir:" + to_string(before) + "->" + to_string(st.actorDirDeg));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SET_DIR") {
        double before = st.actorDirDeg;
        st.actorDirDeg = fmod(b.a, 360.0);
        if (st.actorDirDeg < 0) st.actorDirDeg += 360.0;
        st.log.info(idx, cmd, "Set direction", "dir:" + to_string(before) + "->" + to_string(st.actorDirDeg));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "GOTO_XY") {
        double bx = st.actorX, by = st.actorY;
        st.actorX = b.a;
        st.actorY = b.b;
        clampActorPos(st, idx, cmd, bx, by);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, st.actorX, st.actorY);

        st.log.info(idx, cmd, "Go to",
                    "pos(" + to_string(bx) + "," + to_string(by) + ")->(" +
                    to_string(st.actorX) + "," + to_string(st.actorY) + ")");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "CHANGE_X") {
        double bx = st.actorX, by = st.actorY;
        st.actorX += b.a;
        clampActorPos(st, idx, cmd, bx, by);
        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, st.actorX, st.actorY);

        st.log.info(idx, cmd, "Change X", "x:" + to_string(bx) + "->" + to_string(st.actorX));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "CHANGE_Y") {
        double bx = st.actorX, by = st.actorY;
        st.actorY += b.a;
        clampActorPos(st, idx, cmd, bx, by);
        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, st.actorX, st.actorY);

        st.log.info(idx, cmd, "Change Y", "y:" + to_string(by) + "->" + to_string(st.actorY));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "GOTO_RANDOM") {
        double bx = st.actorX, by = st.actorY;
        double minX = st.ws.bounds.x;
        double maxX = st.ws.bounds.x + st.ws.bounds.w;
        double minY = st.ws.bounds.y;
        double maxY = st.ws.bounds.y + st.ws.bounds.h;

        st.actorX = minX + (rand() / (double)RAND_MAX) * (maxX - minX);
        st.actorY = minY + (rand() / (double)RAND_MAX) * (maxY - minY);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, st.actorX, st.actorY);

        st.log.info(idx, cmd, "Go random", "pos(" + to_string(st.actorX) + "," + to_string(st.actorY) + ")");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "GOTO_MOUSE") {
        double bx = st.actorX, by = st.actorY;
        st.actorX = st.in.mx;
        st.actorY = st.in.my;
        clampActorPos(st, idx, cmd, bx, by);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, st.actorX, st.actorY);

        st.log.info(idx, cmd, "Go mouse", "pos(" + to_string(st.actorX) + "," + to_string(st.actorY) + ")");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "BOUNCE_EDGE") {
        bool onEdge = (st.actorX <= st.ws.bounds.x + 0.5) ||
                      (st.actorX >= st.ws.bounds.x + st.ws.bounds.w - 0.5) ||
                      (st.actorY <= st.ws.bounds.y + 0.5) ||
                      (st.actorY >= st.ws.bounds.y + st.ws.bounds.h - 0.5);

        if (onEdge) {
            double before = st.actorDirDeg;
            st.actorDirDeg = fmod(180.0 - st.actorDirDeg, 360.0);
            if (st.actorDirDeg < 0) st.actorDirDeg += 360.0;
            st.log.warn(idx, cmd, "Bounce", "dir:" + to_string(before) + "->" + to_string(st.actorDirDeg));
        } else {
            st.log.info(idx, cmd, "Bounce", "not on edge");
        }
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Looks
    auto setBubble = [&](bool think, const string& text, double seconds) {
        st.bubbleThink = think;
        st.bubbleText = text;
        if (seconds <= 0.0) st.bubbleUntilMs = 0;
        else st.bubbleUntilMs = SDL_GetTicks() + (uint32_t)max(0.0, seconds * 1000.0);
    };

    if (cmd == "SAY")      { setBubble(false, b.s1, 0.0); st.log.info(idx, cmd, "Say", "text=" + b.s1); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "SAY_T")    { setBubble(false, b.s1, b.a); st.log.info(idx, cmd, "Say for", "t=" + to_string(b.a) + " text=" + b.s1); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "THINK")    { setBubble(true,  b.s1, 0.0); st.log.info(idx, cmd, "Think", "text=" + b.s1); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "THINK_T")  { setBubble(true,  b.s1, b.a); st.log.info(idx, cmd, "Think for", "t=" + to_string(b.a) + " text=" + b.s1); st.scriptPC++; return StepResult::Advanced; }

    if (cmd == "SHOW") { st.actorVisible = true;  st.log.info(idx, cmd, "Show", ""); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "HIDE") { st.actorVisible = false; st.log.info(idx, cmd, "Hide", ""); st.scriptPC++; return StepResult::Advanced; }

    if (cmd == "SIZE_SET") {
        double before = st.actorSizePct;
        st.actorSizePct = clampT(b.a, 0.0, 300.0);
        st.log.info(idx, cmd, "Set size", "size:" + to_string(before) + "->" + to_string(st.actorSizePct));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "SIZE_CHANGE") {
        double before = st.actorSizePct;
        st.actorSizePct = clampT(st.actorSizePct + b.a, 0.0, 300.0);
        st.log.info(idx, cmd, "Change size", "size:" + to_string(before) + "->" + to_string(st.actorSizePct));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "FX_COLOR_SET") {
        double before = st.lookColorEffect;
        st.lookColorEffect = fmod(b.a, 360.0);
        if (st.lookColorEffect < 0) st.lookColorEffect += 360.0;
        st.log.info(idx, cmd, "Set color effect", "h:" + to_string(before) + "->" + to_string(st.lookColorEffect));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "FX_COLOR_CHANGE") {
        double before = st.lookColorEffect;
        st.lookColorEffect = fmod(st.lookColorEffect + b.a, 360.0);
        if (st.lookColorEffect < 0) st.lookColorEffect += 360.0;
        st.log.info(idx, cmd, "Change color effect", "h:" + to_string(before) + "->" + to_string(st.lookColorEffect));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "FX_CLEAR") {
        st.lookColorEffect = 0.0;
        st.log.info(idx, cmd, "Clear effects", "");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "COSTUME_SET")  { st.costumeIndex = (int)round(b.a); st.log.info(idx, cmd, "Set costume", "idx=" + to_string(st.costumeIndex)); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "COSTUME_NEXT") { st.costumeIndex++;                 st.log.info(idx, cmd, "Next costume", "idx=" + to_string(st.costumeIndex)); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "BACKDROP_SET") { st.backdropIndex = (int)round(b.a); st.log.info(idx, cmd, "Set backdrop", "idx=" + to_string(st.backdropIndex)); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "BACKDROP_NEXT"){ st.backdropIndex++;                 st.log.info(idx, cmd, "Next backdrop", "idx=" + to_string(st.backdropIndex)); st.scriptPC++; return StepResult::Advanced; }

    // ---- Sound
    if (cmd == "SOUND_PLAY") {
        string name = b.s1.empty() ? "pop" : b.s1;
        string wav = name + ".wav";
        uint32_t ms = playWavOneShot(st, wav);
        st.soundBusyUntilMs = (ms > 0) ? (SDL_GetTicks() + ms) : 0;
        st.log.info(idx, cmd, "Play sound", "file=" + wav);
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_PLAY_UNTIL") {
        string name = b.s1.empty() ? "pop" : b.s1;
        string wav = name + ".wav";
        uint32_t now = SDL_GetTicks();

        // If already busy, yield until finished
        if (st.soundBusyUntilMs != 0 && now < st.soundBusyUntilMs) {
            return StepResult::Yielded;
        }

        // Not busy now: start playing
        st.soundBusyUntilMs = 0;
        uint32_t ms = playWavOneShot(st, wav);
        st.soundBusyUntilMs = (ms > 0) ? (SDL_GetTicks() + ms) : 0;

        st.log.info(idx, cmd, "Play until done", "file=" + wav);

        // If it actually has duration, yield on this block until it finishes
        if (st.soundBusyUntilMs != 0 && SDL_GetTicks() < st.soundBusyUntilMs) {
            return StepResult::Yielded;
        }

        // Finished immediately or failed => advance
        st.soundBusyUntilMs = 0;
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_STOP_ALL") {
        if (st.audioReady && st.audioDev) SDL_ClearQueuedAudio(st.audioDev);
        st.soundBusyUntilMs = 0;
        st.log.info(idx, cmd, "Stop all sounds", "");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_SET_VOL") {
        int before = st.soundVolume;
        st.soundVolume = clampT((int)round(b.a), 0, 100);
        st.log.info(idx, cmd, "Set volume", "vol:" + to_string(before) + "->" + to_string(st.soundVolume));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_CHANGE_VOL") {
        int before = st.soundVolume;
        st.soundVolume = clampT((int)round(st.soundVolume + b.a), 0, 100);
        st.log.info(idx, cmd, "Change volume", "vol:" + to_string(before) + "->" + to_string(st.soundVolume));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Sensing
    if (cmd == "TOUCH_EDGE") {
        bool onEdge = (st.actorX <= st.ws.bounds.x + 0.5) ||
                      (st.actorX >= st.ws.bounds.x + st.ws.bounds.w - 0.5) ||
                      (st.actorY <= st.ws.bounds.y + 0.5) ||
                      (st.actorY >= st.ws.bounds.y + st.ws.bounds.h - 0.5);
        st.lastValue = Value::Num(onEdge ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Touch edge?", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "TOUCH_MOUSE") {
        double dx = st.actorX - st.in.mx;
        double dy = st.actorY - st.in.my;
        double dist = sqrt(dx*dx + dy*dy);
        bool touching = dist <= 10.0;
        st.lastValue = Value::Num(touching ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Touch mouse?", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "DIST_MOUSE") {
        double dx = st.actorX - st.in.mx;
        double dy = st.actorY - st.in.my;
        double dist = sqrt(dx*dx + dy*dy);
        st.lastValue = Value::Num(dist);
        st.log.info(idx, cmd, "Distance mouse", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "KEY_PRESSED") {
        int sc = b.i1;
        bool down = (sc >= 0 && sc < SDL_NUM_SCANCODES) ? st.in.keyDown[sc] : false;
        st.lastValue = Value::Num(down ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Key pressed?", "sc=" + to_string(sc) + " -> " + st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "MOUSE_DOWN") {
        st.lastValue = Value::Num(st.in.mouseDown ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Mouse down?", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "MOUSE_X") {
        st.lastValue = Value::Num((double)st.in.mx);
        st.log.info(idx, cmd, "Mouse x", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "MOUSE_Y") {
        st.lastValue = Value::Num((double)st.in.my);
        st.log.info(idx, cmd, "Mouse y", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "ASK") {
        beginAskDialog(st, b.s1.empty() ? "?" : b.s1, idx + 1);
        return StepResult::Yielded;
    }
    if (cmd == "ANSWER") {
        st.lastValue = Value::Str(st.lastAnswer);
        st.log.info(idx, cmd, "Answer", "val=" + st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "TIMER") {
        double secs = (SDL_GetTicks() - st.timerStartMs) / 1000.0;
        st.lastValue = Value::Num(secs);
        st.log.info(idx, cmd, "Timer", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "RESET_TIMER") {
        st.timerStartMs = SDL_GetTicks();
        st.log.info(idx, cmd, "Reset timer", "0");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Operators
    if (cmd == "OP_ADD") { st.lastValue = Value::Num(b.a + b.b); st.log.info(idx, cmd, "Add", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_SUB") { st.lastValue = Value::Num(b.a - b.b); st.log.info(idx, cmd, "Sub", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_MUL") { st.lastValue = Value::Num(b.a * b.b); st.log.info(idx, cmd, "Mul", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }

    if (cmd == "DIV") {
        double out = 0.0;
        if (safeDiv(st, idx, b.a, b.b, out)) st.lastValue = Value::Num(out);
        st.log.info(idx, cmd, "Div", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "OP_EQ") { st.lastValue = Value::Num((b.a == b.b) ? 1.0 : 0.0); st.log.info(idx, cmd, "Eq", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_LT") { st.lastValue = Value::Num((b.a <  b.b) ? 1.0 : 0.0); st.log.info(idx, cmd, "Lt", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_GT") { st.lastValue = Value::Num((b.a >  b.b) ? 1.0 : 0.0); st.log.info(idx, cmd, "Gt", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }

    if (cmd == "OP_AND") { st.lastValue = Value::Num(((b.a != 0.0) && (b.b != 0.0)) ? 1.0 : 0.0); st.log.info(idx, cmd, "AND", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_OR")  { st.lastValue = Value::Num(((b.a != 0.0) || (b.b != 0.0)) ? 1.0 : 0.0); st.log.info(idx, cmd, "OR", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_NOT") { st.lastValue = Value::Num((b.a == 0.0) ? 1.0 : 0.0); st.log.info(idx, cmd, "NOT", st.lastValue.toString()); st.scriptPC++; return StepResult::Advanced; }

    if (cmd == "OP_STRLEN") {
        st.lastValue = Value::Num((double)b.s1.size());
        st.log.info(idx, cmd, "strlen", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "OP_LETTER") {
        int n = (int)round(b.a);
        if (n <= 0 || n > (int)b.s1.size()) st.lastValue = Value::Str("");
        else st.lastValue = Value::Str(string(1, b.s1[n-1]));
        st.log.info(idx, cmd, "letter", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "OP_JOIN") {
        st.lastValue = Value::Str(b.s1 + b.s2);
        st.log.info(idx, cmd, "join", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Function Apply
    if (cmd == "FUNC_APPLY") {
        string fn = b.s1.empty() ? "sqrt" : b.s1;
        string inSel = b.inSel.empty() ? "last" : b.inSel;
        string outSel = b.outSel.empty() ? "last" : b.outSel;

        double x = funcReadInput(st, inSel);
        double y = 0.0;

        if (!funcApplyBuiltin(st, idx, fn, x, y)) {
            st.scriptPC++;
            return StepResult::Advanced;
        }

        funcWriteOutput(st, outSel, y);

        st.log.info(idx, cmd, "Apply " + fn,
                    "in=" + inSel + " x=" + to_string(x) +
                    " -> out=" + outSel + " y=" + to_string(y));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Variables
    if (cmd == "VAR_SET_NUM") {
        string name = b.s1.empty() ? "v" : b.s1;
        Value before = st.vars.count(name) ? st.vars[name] : Value::Num(0);
        st.vars[name] = Value::Num(b.a);
        st.log.info(idx, cmd, "Set var", name + ":" + before.toString() + "->" + st.vars[name].toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_SET_STR") {
        string name = b.s1.empty() ? "msg" : b.s1;
        Value before = st.vars.count(name) ? st.vars[name] : Value::Str("");
        st.vars[name] = Value::Str(b.s2);
        st.log.info(idx, cmd, "Set var", name + ":" + before.toString() + "->" + st.vars[name].toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_CHANGE") {
        string name = b.s1.empty() ? "v" : b.s1;
        double before = st.vars.count(name) ? asNum(st.vars[name]) : 0.0;
        double after = before + b.a;
        st.vars[name] = Value::Num(after);
        st.log.info(idx, cmd, "Change var", name + ":" + to_string(before) + "->" + to_string(after));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_SHOW") {
        string name = b.s1.empty() ? "v" : b.s1;
        st.varVisible[name] = true;
        st.log.info(idx, cmd, "Show var", name);
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_HIDE") {
        string name = b.s1.empty() ? "v" : b.s1;
        st.varVisible[name] = false;
        st.log.info(idx, cmd, "Hide var", name);
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_GET") {
        string name = b.s1.empty() ? "v" : b.s1;
        if (st.vars.count(name)) st.lastValue = st.vars[name];
        else st.lastValue = Value::Num(0.0);
        st.log.info(idx, cmd, "Get var", name + " -> " + st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Lists
    if (cmd == "LIST_ADD") {
        string name = b.s1.empty() ? "list" : b.s1;
        Value item = listItemFromBlock(b);
        getList(st, name).push_back(item);
        st.log.info(idx, cmd, "Add to list", name + " <- " + item.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_DELETE") {
        string name = b.s1.empty() ? "list" : b.s1;
        auto& L = getList(st, name);
        int i1 = (int)round(b.a);
        if (listIndexOk1(i1, (int)L.size())) {
            Value removed = L[i1 - 1];
            L.erase(L.begin() + (i1 - 1));
            st.log.info(idx, cmd, "Delete from list", name + " idx=" + to_string(i1) + " val=" + removed.toString());
        } else {
            st.log.warn(idx, cmd, "Delete out of range", name + " idx=" + to_string(i1));
        }
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_CLEAR") {
        string name = b.s1.empty() ? "list" : b.s1;
        getList(st, name).clear();
        st.log.info(idx, cmd, "Clear list", name);
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_LENGTH") {
        string name = b.s1.empty() ? "list" : b.s1;
        st.lastValue = Value::Num((double)getList(st, name).size());
        st.log.info(idx, cmd, "List length", name + " -> " + st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_ITEM") {
        string name = b.s1.empty() ? "list" : b.s1;
        auto& L = getList(st, name);
        int i1 = (int)round(b.a);
        if (listIndexOk1(i1, (int)L.size())) st.lastValue = L[i1 - 1];
        else st.lastValue = Value::Str("");
        st.log.info(idx, cmd, "List item", name + "[" + to_string(i1) + "] -> " + st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_CONTAINS") {
        string name = b.s1.empty() ? "list" : b.s1;
        auto& L = getList(st, name);
        string needle = b.s2;
        bool ok = false;
        for (auto& it : L) {
            if (it.toString() == needle) { ok = true; break; }
        }
        st.lastValue = Value::Num(ok ? 1.0 : 0.0);
        st.log.info(idx, cmd, "List contains?", name + " \"" + needle + "\" -> " + st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_SHOW") {
        string name = b.s1.empty() ? "list" : b.s1;
        st.listVisible[name] = true;
        st.log.info(idx, cmd, "Show list", name);
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_HIDE") {
        string name = b.s1.empty() ? "list" : b.s1;
        st.listVisible[name] = false;
        st.log.info(idx, cmd, "Hide list", name);
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Clones
    if (cmd == "CLONE_CREATE") {
        cloneCreateFromActor(st);
        st.log.info(idx, cmd, "Create clone", "count=" + to_string((int)st.clones.size()));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "CLONE_DELETE_LAST") {
        int before = (int)st.clones.size();
        cloneDeleteLast(st);
        st.log.info(idx, cmd, "Delete last clone", "count:" + to_string(before) + "->" + to_string((int)st.clones.size()));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "CLONE_CLEAR") {
        cloneClearAll(st);
        st.log.info(idx, cmd, "Clear clones", "");
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "CLONE_COUNT") {
        st.lastValue = Value::Num((double)st.clones.size());
        st.log.info(idx, cmd, "Clone count", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Control flow (Yield-based!)
    if (cmd == "WAIT") {
        int ms = (int)max(0.0, b.a * 1000.0);
        st.waiting = true;
        st.waitUntilMs = SDL_GetTicks() + (uint32_t)ms;
        st.log.info(idx, cmd, "Wait", "ms=" + to_string(ms));
        st.scriptPC++;                 // advance PC now
        return StepResult::Yielded;     // yield until time passes
    }

    if (cmd == "STOP_ALL") {
        st.log.warn(idx, cmd, "Stop all scripts", "stop");
        stopScript(st, "STOP_ALL");
        return StepResult::Stopped;
    }

    if (cmd == "WAIT_UNTIL") {
        if (!st.lastValue.truthy()) {
            st.log.info(idx, cmd, "Wait until", "blocked (last=false)");
            return StepResult::Yielded;
        }
        st.log.info(idx, cmd, "Wait until", "pass");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "IF" || cmd == "IFELSE") {
        bool cond = st.lastValue.truthy();
        int endIdx  = (idx >= 0 && idx < (int)st.jumpEnd.size())  ? st.jumpEnd[idx]  : -1;
        int elseIdx = (idx >= 0 && idx < (int)st.jumpElse.size()) ? st.jumpElse[idx] : -1;

        if (!cond) {
            if (cmd == "IFELSE" && elseIdx != -1) {
                st.log.info(idx, cmd, "IF false", "jump to ELSE");
                st.scriptPC = elseIdx + 1;
                return StepResult::Advanced;
            }
            if (endIdx != -1) {
                st.log.info(idx, cmd, "IF false", "jump to END_IF");
                st.scriptPC = endIdx + 1;
                return StepResult::Advanced;
            }
        }

        st.log.info(idx, cmd, "IF true", "enter");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "ELSE") {
        int endIdx = (idx >= 0 && idx < (int)st.jumpTo.size()) ? st.jumpTo[idx] : -1;
        if (endIdx != -1) {
            st.log.info(idx, cmd, "ELSE", "jump END_IF");
            st.scriptPC = endIdx + 1;
            return StepResult::Advanced;
        }
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "END_IF") {
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "REPEAT") {
        int endIdx = (idx >= 0 && idx < (int)st.loopEnd.size()) ? st.loopEnd[idx] : -1;
        int count = clampT((int)round(b.a), 0, 1000000);

        if (st.repeatCounter[idx] == 0) st.repeatCounter[idx] = count;

        if (count == 0 || st.repeatCounter[idx] <= 0) {
            st.repeatCounter[idx] = 0;
            if (endIdx != -1) {
                st.log.info(idx, cmd, "Repeat skip", "count=0");
                st.scriptPC = endIdx + 1;
                return StepResult::Advanced;
            }
        }

        st.log.info(idx, cmd, "Repeat enter", "left=" + to_string(st.repeatCounter[idx]));
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "REPEAT_UNTIL") {
        int endIdx = (idx >= 0 && idx < (int)st.loopEnd.size()) ? st.loopEnd[idx] : -1;
        if (st.lastValue.truthy()) {
            st.log.info(idx, cmd, "RepeatUntil", "cond true -> exit");
            st.scriptPC = (endIdx != -1) ? (endIdx + 1) : (idx + 1);
            return StepResult::Advanced;
        }
        st.log.info(idx, cmd, "RepeatUntil", "cond false -> enter");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "END_REPEAT") {
        int startIdx = (idx >= 0 && idx < (int)st.loopStart.size()) ? st.loopStart[idx] : -1;
        if (startIdx != -1) {
            string startCmd = st.ws.blocks[startIdx].cmd;
            if (startCmd == "REPEAT") {
                st.repeatCounter[startIdx] = max(0, st.repeatCounter[startIdx] - 1);
                if (st.repeatCounter[startIdx] > 0) {
                    st.log.info(idx, cmd, "Repeat loop", "back to start");
                    st.scriptPC = startIdx + 1;
                    return StepResult::Advanced;
                }
                st.repeatCounter[startIdx] = 0;
                st.log.info(idx, cmd, "Repeat end", "done");
                st.scriptPC++;
                return StepResult::Advanced;
            }
            if (startCmd == "REPEAT_UNTIL") {
                st.log.info(idx, cmd, "RepeatUntil loop", "back to start");
                st.scriptPC = startIdx;
                return StepResult::Advanced;
            }
        }
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "FOREVER") {
        st.log.info(idx, cmd, "Forever enter", "");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "END_FOREVER") {
        int startIdx = (idx >= 0 && idx < (int)st.loopStart.size()) ? st.loopStart[idx] : -1;
        if (startIdx != -1) {
            st.log.warn(idx, cmd, "Forever loop", "back to start");
            st.scriptPC = startIdx + 1;
            return StepResult::Advanced;
        }
        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Legacy extra
    if (cmd == "SQRT") {
        double out = 0.0;
        if (safeSqrt(st, idx, b.a, out)) st.lastValue = Value::Num(out);
        st.log.info(idx, cmd, "Sqrt", st.lastValue.toString());
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "LOOP") {
        st.log.warn(idx, cmd, "Infinite loop block", "pc stays same");
        return StepResult::Yielded; // keep yielding so UI doesn't watchdog
    }

    // ---- Pen extension blocks
    if (isPenCmd(cmd) && !st.penExtensionEnabled) {
        st.log.warn(idx, cmd, "Pen extension not enabled", "skipped");
        st.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "PEN_DOWN") { st.penDown = true;  st.log.info(idx, cmd, "Pen down", ""); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "PEN_UP")   { st.penDown = false; st.log.info(idx, cmd, "Pen up", "");   st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "PEN_ERASE_ALL") { penClearAll(st); st.log.info(idx, cmd, "All erase", "cleared"); st.scriptPC++; return StepResult::Advanced; }
    if (cmd == "PEN_STAMP") { penAddStamp(st); st.log.info(idx, cmd, "Stamp", "count=" + to_string((int)st.penStamps.size())); st.scriptPC++; return StepResult::Advanced; }

    if (cmd == "PEN_SET_SIZE") {
        int before = st.penSize;
        st.penSize = clampT((int)round(b.a), 1, 30);
        st.log.info(idx, cmd, "Set size", "size:" + to_string(before) + "->" + to_string(st.penSize));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "PEN_CHANGE_SIZE") {
        int before = st.penSize;
        st.penSize = clampT((int)round(st.penSize + b.a), 1, 30);
        st.log.info(idx, cmd, "Change size", "size:" + to_string(before) + "->" + to_string(st.penSize));
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "PEN_SET_COLOR") {
        SDL_Color c = b.pickColor;
        rgbToHsv(c, st.penHue, st.penSat, st.penBri);
        penSyncRGB(st);
        st.log.info(idx, cmd, "Set color (direct)",
                    "rgb=(" + to_string((int)c.r) + "," + to_string((int)c.g) + "," + to_string((int)c.b) + ")");
        st.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "PEN_SET_ATTR" || cmd == "PEN_CHANGE_ATTR") {
        string opt = b.opt.empty() ? "COLOR" : b.opt;
        bool isChange = (cmd == "PEN_CHANGE_ATTR");

        if (opt == "COLOR") {
            double before = st.penHue;
            st.penHue = isChange ? (st.penHue + b.a) : b.a;
            st.penHue = fmod(st.penHue, 360.0);
            if (st.penHue < 0) st.penHue += 360.0;
            penSyncRGB(st);
            st.log.info(idx, cmd, isChange ? "Change hue" : "Set hue",
                        "h:" + to_string(before) + "->" + to_string(st.penHue));
        } else if (opt == "SAT") {
            double before = st.penSat;
            st.penSat = isChange ? (st.penSat + b.a) : b.a;
            st.penSat = clampT(st.penSat, 0.0, 100.0);
            penSyncRGB(st);
            st.log.info(idx, cmd, isChange ? "Change sat" : "Set sat",
                        "s:" + to_string(before) + "->" + to_string(st.penSat));
        } else {
            double before = st.penBri;
            st.penBri = isChange ? (st.penBri + b.a) : b.a;
            st.penBri = clampT(st.penBri, 0.0, 100.0);
            penSyncRGB(st);
            st.log.info(idx, cmd, isChange ? "Change bri" : "Set bri",
                        "v:" + to_string(before) + "->" + to_string(st.penBri));
        }

        st.scriptPC++;
        return StepResult::Advanced;
    }

    // ---- Unknown
    st.log.warn(idx, cmd, "Unknown cmd skipped", "");
    st.scriptPC++;
    return StepResult::Advanced;
}

static void runScriptTick(AppState& st) {
    if (!st.scriptRunning) return;
    if (st.askDialogOpen)  return;

    uint32_t now = SDL_GetTicks();

    // --- Step-by-step
    if (st.debugStepMode) {
        if (!st.stepRequested) return;
        st.stepRequested = false;

        StepResult r = executeOneBlock(st);
        if (r == StepResult::Stopped) return;

        if (st.scriptPC >= (int)st.ws.blocks.size()) {
            stopScript(st, "Reached end");
        }
        return;
    }

    // --- سرعت اجرا: هر runSpeedMs یک بلاک
    if (st.runSpeedMs > 0) {
        if (now < st.nextStepAtMs) return;
    }

    // هر فریم فقط 1 بلاک (برای دیدن حرکت)
    StepResult r = executeOneBlock(st);

    if (!st.scriptRunning) return;
    if (st.askDialogOpen)  return;

    if (r == StepResult::Yielded) {
        // منتظر زمان/صدا/شرط
        return;
    }

    if (st.scriptPC >= (int)st.ws.blocks.size()) {
        stopScript(st, "Reached end");
        return;
    }

    // زمان بلاک بعدی
    if (st.runSpeedMs > 0) st.nextStepAtMs = SDL_GetTicks() + (uint32_t)st.runSpeedMs;
}

// =========================
// UI setup
// =========================
static void setupUI(AppState& st) {
    st.buttons.clear();

    auto mkBtn = [&](int x, int w, const string& label, const string& sub, function<void()> cb) {
        Button b;
        b.rect = SDL_Rect{x, 8, w, TOP_BAR_H - 16};
        b.text = label;
        b.sub = sub;
        b.onClick = cb;
        return b;
    };

    int x = 10;

    st.buttons.push_back(mkBtn(x, 90, "New", "Ctrl+N", [&] {
        st.ws.reset();
        st.penDown = false;
        penClearAll(st);

        st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 40);
        if (!st.ws.blocks.empty()) {
            Block& b = st.ws.blocks.back();
            b.cmd = "EVENT_FLAG";
            setBlockVisual(b);
        }
        st.log.log("NEW", "Reset workspace");
    }));
    x += 100;

    st.buttons.push_back(mkBtn(x, 90, "Save", "Ctrl+S", [&] {
        beginSaveDialog(st);
        st.log.log("UI", "Open Save dialog");
    }));
    x += 100;

    st.buttons.push_back(mkBtn(x, 90, "Load", "Ctrl+O", [&] {
        beginLoadDialog(st);
        st.log.log("UI", "Open Load dialog");
    }));
    x += 100;

    st.buttons.push_back(mkBtn(x, 110, "Add Block", "B", [&] {
        st.ws.addBlock(st.ws.bounds.x + 60, st.ws.bounds.y + 60);
        if (!st.ws.blocks.empty()) {
            Block& b = st.ws.blocks.back();
            b.cmd = "MOVE_STEPS"; b.a = 10.0;
            setBlockVisual(b);
        }
        st.log.log("ADD", "Added block");
    }));
    x += 120;

    st.buttons.push_back(mkBtn(x, 120, "Extensions", "E", [&] {
        openExtensionLibrary(st);
    }));
    x += 130;

    st.buttons.push_back(mkBtn(x, 90, "Help", "H", [&] {
        st.helpMenuOpen = !st.helpMenuOpen;
        st.log.info(-1, "HELP", st.helpMenuOpen ? "Open menu" : "Close menu", "");
    }));
    st.helpButtonRect = st.buttons.back().rect;
    x += 100;

    st.buttons.push_back(mkBtn(x, 110, "Settings", "P", [&] {
    openSettings(st);
    }));
    x += 120;

    st.buttons.push_back(mkBtn(x, 90, "Run", "F5", [&] {
        startScript(st);
    }));
    x += 100;

    st.buttons.push_back(mkBtn(x, 90, "Stop", "F6", [&] {
        stopScript(st, "User stop (F6)");
    }));
    x += 100;

    st.buttons.push_back(mkBtn(x, 90, "Quit", "Esc", [&] {
        st.quit = true;
    }));
}

// =========================
// Event processing
// =========================
static void processEvents(AppState& st, SDL_Window* window) {
    SDL_Event e;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window, &winW, &winH);

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_MOUSEMOTION) {
            st.in.mx = e.motion.x;
            st.in.my = e.motion.y;
        }
        // Settings modal first
        if (st.settingsOpen) {
            if (handleSettingsEvent(st, e, winW, winH)) continue;
        }


        // Extension modals first
        if (st.extensionLibraryOpen) {
            if (handleExtensionLibraryEvent(st, e, winW, winH)) continue;
        }
        if (st.penColorPickerOpen) {
            if (handlePenColorPickerEvent(st, e, winW, winH)) continue;
        }
        // Section 5 modal
        if (st.funcIOMenuOpen) {
            if (handleFuncIOMenuEvent(st, e, winW, winH)) continue;
        }

        // Ask/Save/Load dialogs
        if (st.askDialogOpen || st.saveDialogOpen || st.loadDialogOpen) {
            if (handleDialogsEvent(st, e, winW, winH)) continue;
        }

        // palette scrolling (wheel over left panel)
        if (e.type == SDL_MOUSEWHEEL) {
            SDL_Rect left = {0, TOP_BAR_H, LEFT_PANEL_W, winH - TOP_BAR_H};
            if (pointInRect(st.in.mx, st.in.my, left)) {
                int step = (e.wheel.y > 0) ? -42 : (e.wheel.y < 0 ? 42 : 0);
                if (step != 0) {
                    st.paletteScroll = clampT(st.paletteScroll + step, 0, st.paletteMaxScroll);
                    st.paletteDirty = true;
                    continue;
                }
            }
        }

        switch (e.type) {
            case SDL_QUIT:
                st.quit = true;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    st.in.mouseDown = true;
                    st.in.mousePressed = true;
                    st.in.mx = e.button.x;
                    st.in.my = e.button.y;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    st.in.mouseDown = false;
                    st.in.mouseReleased = true;
                    st.in.mx = e.button.x;
                    st.in.my = e.button.y;
                }
                break;

            case SDL_KEYDOWN:
                if (!e.key.repeat) {
                    st.in.keyDown[e.key.keysym.scancode] = true;
                    st.in.keyPressed[e.key.keysym.scancode] = true;
                }
                break;

            case SDL_KEYUP:
                st.in.keyDown[e.key.keysym.scancode] = false;
                break;

            default:
                break;
        }
    }
}

// =========================
// Shortcuts
// =========================
static void handleShortcuts(AppState& st) {
    if (st.in.keyPressed[SDL_SCANCODE_P]) {
        openSettings(st);
    }

    if (st.extensionLibraryOpen) {
        if (st.in.keyPressed[SDL_SCANCODE_ESCAPE]) {
            st.extensionLibraryOpen = false;
            st.log.info(-1, "EXT", "Close library (Esc)", "");
        }
        return;
    }

    if (st.penColorPickerOpen) {
        if (st.in.keyPressed[SDL_SCANCODE_ESCAPE]) {
            st.penColorPickerOpen = false;
            st.penColorPickerBlockIndex = -1;
            st.log.info(-1, "PEN", "Close color picker (Esc)", "");
        }
        return;
    }

    if (st.funcIOMenuOpen) {
        if (st.in.keyPressed[SDL_SCANCODE_ESCAPE]) {
            closeFuncIOMenu(st, "Esc shortcut");
        }
        return;
    }

    if (st.askDialogOpen) {
        return;
    }

    if (st.showLogsPanel) {
        if (st.in.keyPressed[SDL_SCANCODE_ESCAPE]) {
            st.showLogsPanel = false;
            st.logsScroll = 0;
            st.log.info(-1, "HELP", "Close Logs panel", "");
        }
        return;
    }

    if (st.helpMenuOpen && st.in.keyPressed[SDL_SCANCODE_ESCAPE]) {
        st.helpMenuOpen = false;
        st.log.info(-1, "HELP", "Close menu (Esc)", "");
        return;
    }

    bool ctrl = st.in.keyDown[SDL_SCANCODE_LCTRL] || st.in.keyDown[SDL_SCANCODE_RCTRL];

    if (st.in.keyPressed[SDL_SCANCODE_H]) {
        st.helpMenuOpen = !st.helpMenuOpen;
        st.log.info(-1, "HELP", st.helpMenuOpen ? "Open menu (H)" : "Close menu (H)", "");
    }

    if (st.in.keyPressed[SDL_SCANCODE_E]) {
        openExtensionLibrary(st);
    }

    if (st.in.keyPressed[SDL_SCANCODE_F5]) {
        startScript(st);
    }
    if (st.in.keyPressed[SDL_SCANCODE_F6]) {
        stopScript(st, "User stop (F6)");
    }

    if (st.debugStepMode && st.scriptRunning && st.in.keyPressed[SDL_SCANCODE_SPACE]) {
        st.stepRequested = true;
        st.log.info(st.scriptPC, "DEBUG", "Step", "Space pressed");
    }

    if (ctrl && st.in.keyPressed[SDL_SCANCODE_N]) {
        st.ws.reset();
        st.penDown = false;
        penClearAll(st);

        st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 40);
        if (!st.ws.blocks.empty()) {
            Block& b = st.ws.blocks.back();
            b.cmd = "EVENT_FLAG";
            setBlockVisual(b);
        }
        st.log.log("NEW", "Reset (shortcut)");
    }

    if (ctrl && st.in.keyPressed[SDL_SCANCODE_S]) {
        beginSaveDialog(st);
        st.log.log("UI", "Open Save dialog (shortcut)");
    }

    if (ctrl && st.in.keyPressed[SDL_SCANCODE_O]) {
        beginLoadDialog(st);
        st.log.log("UI", "Open Load dialog (shortcut)");
    }

    if (st.in.keyPressed[SDL_SCANCODE_B]) {
        st.ws.addBlock(st.ws.bounds.x + 60, st.ws.bounds.y + 60);
        if (!st.ws.blocks.empty()) {
            Block& b = st.ws.blocks.back();
            b.cmd = "MOVE_STEPS"; b.a = 10.0;
            setBlockVisual(b);
        }
        st.log.log("ADD", "Added block (shortcut)");
    }

    if (st.in.keyPressed[SDL_SCANCODE_ESCAPE]) {
        st.quit = true;
    }
}

// =========================
// Update + Render
// =========================
static void update(AppState& st, SDL_Window* window) {
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    bgmTick(st);

    st.ws.bounds = SDL_Rect{LEFT_PANEL_W, TOP_BAR_H, w - LEFT_PANEL_W, h - TOP_BAR_H};

    if (w != st.paletteLastW || h != st.paletteLastH || st.penExtensionEnabled != st.paletteLastPenEnabled) {
        st.paletteDirty = true;
        st.paletteLastW = w; st.paletteLastH = h;
        st.paletteLastPenEnabled = st.penExtensionEnabled;
    }
    if (st.paletteDirty) rebuildPalette(st, w, h);

    if (st.loadDialogOpen) updateLoadHover(st, w, h);

    if (st.askDialogOpen || st.saveDialogOpen || st.loadDialogOpen) return;
    if (st.extensionLibraryOpen || st.penColorPickerOpen || st.funcIOMenuOpen) return;
    if (st.settingsOpen) return;


    if (updateHelpMenu(st)) return;

    if (st.showLogsPanel) {
        updateLogsPanel(st);
        handleShortcuts(st);
        return;
    }

    for (size_t i = 0; i < st.buttons.size(); i++) st.buttons[i].update(st.in);
    for (size_t i = 0; i < st.palette.size(); i++) st.palette[i].update(st.in);

    handleShortcuts(st);

    // Shift+Click: Function IO menu + Pen blocks editing + Variable blocks name cycling (minimal)
    bool shift = st.in.keyDown[SDL_SCANCODE_LSHIFT] || st.in.keyDown[SDL_SCANCODE_RSHIFT];
    if (shift && st.in.mousePressed) {
        int hit = st.ws.hitTest(st.in.mx, st.in.my);
        if (hit != -1) {
            Block& b = st.ws.blocks[hit];

            // Section 5 first: open IO menu for function block
            if (b.cmd == "FUNC_APPLY") {
                openFuncIOMenu(st, hit);
                return;
            }

            if (b.cmd == "PEN_SET_COLOR") {
                st.penColorPickerOpen = true;
                st.penColorPickerBlockIndex = hit;
                st.log.info(hit, "PEN_SET_COLOR", "Open color picker", "");
                return;
            }
            if (b.cmd == "PEN_SET_ATTR" || b.cmd == "PEN_CHANGE_ATTR") {
                if (b.opt.empty()) b.opt = "COLOR";
                b.opt = penNextAttr(b.opt);
                st.log.info(hit, b.cmd, "Cycle attr", "opt=" + b.opt);
                return;
            }

            if (b.cmd.rfind("VAR_", 0) == 0) {
                b.s1 = cycleName3(b.s1.empty() ? "v" : b.s1);
                st.log.info(hit, b.cmd, "Cycle var name", "name=" + b.s1);
                return;
            }

            if (b.cmd.rfind("LIST_", 0) == 0) {
                b.s1 = cycleListName3(b.s1.empty() ? "list" : b.s1);
                st.log.info(hit, b.cmd, "Cycle list name", "name=" + b.s1);
                return;
            }
        }
    }

    // bubble timeout
    if (st.bubbleUntilMs != 0 && SDL_GetTicks() >= st.bubbleUntilMs) {
        st.bubbleText.clear();
        st.bubbleUntilMs = 0;
        st.log.info(-1, "LOOKS", "Bubble timeout", "");
    }

    st.log.cycle++;
    st.ws.update(st.in, st.log);

    runScriptTick(st);
}

static void renderBlockLabels(const AppState& st, SDL_Renderer* r) {
    SDL_Color t{10,10,10,255};
    SDL_Color w{240,240,240,255};

    for (size_t i = 0; i < st.ws.blocks.size(); i++) {
        const Block& b = st.ws.blocks[i];
        string label = b.cmd;

        if (b.cmd == "MOVE_STEPS") label = "move " + to_string((int)round(b.a)) + " steps";
        else if (b.cmd == "TURN_R") label = "turn right " + to_string((int)round(b.a));
        else if (b.cmd == "TURN_L") label = "turn left " + to_string((int)round(b.a));
        else if (b.cmd == "GOTO_XY") label = "go to (" + to_string((int)round(b.a)) + "," + to_string((int)round(b.b)) + ")";
        else if (b.cmd == "SAY") label = "say " + b.s1;
        else if (b.cmd == "THINK") label = "think " + b.s1;
        else if (b.cmd == "ASK") label = "ask " + b.s1;
        else if (b.cmd == "BROADCAST") label = "broadcast " + b.s1;
        else if (b.cmd == "WHEN_RECEIVE") label = "when receive " + b.s1;
        else if (b.cmd == "VAR_SET_NUM") label = "set " + b.s1 + " = " + to_string((int)round(b.a));
        else if (b.cmd == "VAR_SET_STR") label = "set " + b.s1 + " = \"" + b.s2 + "\"";
        else if (b.cmd == "VAR_CHANGE") label = "change " + b.s1 + " by " + to_string((int)round(b.a));
        else if (b.cmd == "FUNC_APPLY") {
            string fn = b.s1.empty() ? "sqrt" : b.s1;
            string inSel = b.inSel.empty() ? "last" : b.inSel;
            string outSel = b.outSel.empty() ? "last" : b.outSel;
            label = fn + "(" + inSel + ") -> " + outSel;
        }
        else if (b.cmd == "LIST_ADD") label = "add " + (b.s2.empty() ? to_string((int)round(b.a)) : "\"" + b.s2 + "\"") + " to " + b.s1;
        else if (b.cmd == "LIST_DELETE") label = "delete " + to_string((int)round(b.a)) + " of " + b.s1;
        else if (b.cmd == "LIST_CLEAR") label = "clear " + b.s1;
        else if (b.cmd == "LIST_LENGTH") label = "length of " + b.s1;
        else if (b.cmd == "LIST_ITEM") label = "item " + to_string((int)round(b.a)) + " of " + b.s1;
        else if (b.cmd == "LIST_CONTAINS") label = b.s1 + " contains \"" + b.s2 + "\"?";
        else if (b.cmd == "LIST_SHOW") label = "show " + b.s1;
        else if (b.cmd == "LIST_HIDE") label = "hide " + b.s1;
        else if (b.cmd == "CLONE_CREATE") label = "create clone";
        else if (b.cmd == "CLONE_DELETE_LAST") label = "delete last clone";
        else if (b.cmd == "CLONE_CLEAR") label = "clear clones";
        else if (b.cmd == "CLONE_COUNT") label = "clone count";

        renderText(r, st.uiFont, label, b.rect.x + 10 + 1, b.rect.y + 16 + 1, t);
        renderText(r, st.uiFont, label, b.rect.x + 10, b.rect.y + 16, w);
    }
}

static void render(const AppState& st, SDL_Renderer* r, SDL_Window* window) {
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);

    SDL_SetRenderDrawColor(r, 25, 25, 28, 255);
    SDL_RenderClear(r);

    // اگر backdrop texture داریم، توی workspace بکش
    if (!st.backdrops.empty()) {
        int bi = st.backdropIndex;
        if (bi < 0) bi = 0;
        bi %= (int)st.backdrops.size();
        if (st.backdrops[bi].tex) {
            SDL_Rect dst = st.ws.bounds; // فقط داخل صحنه
            SDL_RenderCopy(r, st.backdrops[bi].tex, nullptr, &dst);
        }
    }


    SDL_Rect top = {0, 0, w, TOP_BAR_H};
    SDL_SetRenderDrawColor(r, 20, 20, 22, 255);
    SDL_RenderFillRect(r, &top);

    SDL_Rect left = {0, TOP_BAR_H, LEFT_PANEL_W, h - TOP_BAR_H};
    SDL_SetRenderDrawColor(r, 24, 24, 26, 255);
    SDL_RenderFillRect(r, &left);
    SDL_SetRenderDrawColor(r, 12, 12, 12, 255);
    SDL_RenderDrawRect(r, &left);

    for (size_t i = 0; i < st.buttons.size(); i++) st.buttons[i].draw(r, st.uiFont);

    renderPaletteHeader(st, r);
    renderPaletteCats(st, r);
    for (size_t i = 0; i < st.palette.size(); i++) st.palette[i].draw(r, st.uiFont);

    SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
    SDL_RenderDrawRect(r, &st.ws.bounds);

    st.ws.draw(r);
    renderBlockLabels(st, r);

    renderPenLayer(st, r);

    if (st.scriptRunning && st.scriptPC >= 0 && st.scriptPC < (int)st.ws.blocks.size()) {
        SDL_Rect hi = st.ws.blocks[st.scriptPC].rect;
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderDrawRect(r, &hi);
    }

    bool shouldDrawActor = st.scriptRunning || st.drawActorWhenStopped;

    if (st.scriptRunning) {
        for (const auto& c : st.clones) {
            drawSprite(r, st, c.x, c.y, c.dirDeg, c.sizePct, c.visible);
        }
    }

    if (shouldDrawActor) {
        drawSprite(r, st, st.actorX, st.actorY, st.actorDirDeg, st.actorSizePct, st.actorVisible);
    }

    if (!st.bubbleText.empty() && st.scriptRunning && st.actorVisible) {
        SDL_Rect box{(int)st.actorX + 16, (int)st.actorY - 40, 240, 46};
        SDL_SetRenderDrawColor(r, 245,245,245,255);
        SDL_RenderFillRect(r, &box);
        SDL_SetRenderDrawColor(r, 10,10,10,255);
        SDL_RenderDrawRect(r, &box);
        renderText(r, st.uiFont, st.bubbleThink ? ("(think) " + st.bubbleText) : st.bubbleText, box.x + 8, box.y + 12, SDL_Color{10,10,10,255});
    }

    int vy = TOP_BAR_H + 8;
    for (auto& kv : st.varVisible) {
        if (!kv.second) continue;
        string name = kv.first;
        string val = st.vars.count(name) ? st.vars.at(name).toString() : "0";
        renderText(r, st.uiFont, name + " = " + val, LEFT_PANEL_W + 12, vy, SDL_Color{220,220,220,255});
        vy += 18;
        if (vy > TOP_BAR_H + 140) break;
    }

    int ly = vy + 10;
    int shownLists = 0;
    for (auto& kv : st.listVisible) {
        if (!kv.second) continue;
        const string& name = kv.first;

        renderText(r, st.uiFont, name + ":", LEFT_PANEL_W + 12, ly, SDL_Color{220,220,220,255});
        ly += 18;

        auto it = st.lists.find(name);
        if (it != st.lists.end()) {
            int showItems = 0;
            for (auto& v : it->second) {
                renderText(r, st.uiFont, " - " + v.toString(), LEFT_PANEL_W + 22, ly, SDL_Color{180,180,180,255});
                ly += 18;
                if (++showItems >= 6) break;
                if (ly > TOP_BAR_H + 260) break;
            }
        }

        ly += 8;
        if (++shownLists >= 2) break; // prevent clutter
        if (ly > TOP_BAR_H + 280) break;
    }

    renderHelpMenu(st, r);
    renderLogsPanel(st, r, w, h);

    renderExtensionLibrary(st, r, w, h);
    renderPenColorPicker(st, r, w, h);

    // Section 5 overlay
    renderFuncIOMenu(st, r, w, h);

    renderDialogs(st, r, w, h);
    renderSettings(st, r, w, h);

    SDL_RenderPresent(r);
}

// =========================
// Run
// =========================
static int RunApp() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        fatalBox("SDL_Init failed", SDL_GetError());
        return 1;
    }

    if (TTF_Init() != 0) {
        fatalBox("TTF_Init failed", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "YKP Base (SDL2)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        fatalBox("SDL_CreateWindow failed", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer) {
        fatalBox("SDL_CreateRenderer failed", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    AppState st;
    st.uiFont = loadUIFont(16);
    if (!st.uiFont) {
        fatalBox("Font load failed", "Could not load a system font. Try putting 'font.ttf' next to the executable.");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    penSyncRGB(st);

    initAudioSystem(st);          // Section 7 (SFX) - unchanged
    initBGMSystem(st);            // NEW: BGM device
    loadBGM(st, st.bgmFile);      // NEW: tries bgm.wav (if missing -> logs warning, no crash)

    initAssets(st, renderer);     // Section 6

    st.ws.bounds = SDL_Rect{LEFT_PANEL_W, TOP_BAR_H, WINDOW_W - LEFT_PANEL_W, WINDOW_H - TOP_BAR_H};

    st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 40);
    if (!st.ws.blocks.empty()) {
        Block& b = st.ws.blocks.back();
        b.cmd = "EVENT_FLAG";
        setBlockVisual(b);
    }
    st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 110);
    if (st.ws.blocks.size() >= 2) {
        Block& b2 = st.ws.blocks.back();
        b2.cmd = "MOVE_STEPS";
        b2.a = 10.0;
        setBlockVisual(b2);
    }

    setupUI(st);

    while (!st.quit) {
        st.in.beginFrame();
        processEvents(st, window);
        update(st, window);
        render(st, renderer, window);
        SDL_Delay(1);
    }

    SDL_StopTextInput();

    shutdownAssets(st);
    shutdownBGMSystem(st);   // NEW
    shutdownAudioSystem(st);


    if (st.uiFont) TTF_CloseFont(st.uiFont);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();
    return 0;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    return RunApp();
}