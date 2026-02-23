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

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <commdlg.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

using namespace std;

static const int WINDOW_W = 1200;
static const int WINDOW_H = 720;
static const int TOP_BAR_H = 52;
static const int LEFT_PANEL_W = 320;

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

    void log(const string& tag, const string& msg) { logLine("INFO", -1, tag, msg, ""); }
    void info(int idx, const string& cmd, const string& op, const string& data) { logLine("INFO", idx, cmd, op, data); }
    void warn(int idx, const string& cmd, const string& op, const string& data) { logLine("WARNING", idx, cmd, op, data); }
    void error(int idx, const string& cmd, const string& op, const string& data) { logLine("ERROR", idx, cmd, op, data); }
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

struct Button {
    SDL_Rect rect{};
    string text;
    string sub;
    function<void()> onClick;
    function<void()> onPress;
    bool hovered = false;
    bool down = false;

    void update(const InputState& in) {
        hovered = pointInRect(in.mx, in.my, rect);
        if (hovered && in.mousePressed) {
            down = true;
            if (onPress) onPress();
        }
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
    string inSel = "last";
    string outSel = "last";
};

struct Workspace {
    SDL_Rect bounds{};
    vector<Block> blocks;
    int nextId = 1;

    void reset() { blocks.clear(); nextId = 1; }

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

struct Sprite {
    string name = "Sprite";
    TextureAsset icon;
    bool isDragging = false;
    double dragOffX = 0.0;
    double dragOffY = 0.0;
    double backupX = 0.0;
    double backupY = 0.0;
    double backupDirDeg = 90.0;
    bool backupVisible = true;
    double backupSizePct = 100.0;
    double backupColorEffect = 0.0;
    int backupCostumeIndex = 0;
    int backupZOrder = 0;
    double x = 0.0;
    double y = 0.0;
    double dirDeg = 90.0;
    bool visible = true;
    double sizePct = 100.0;
    double colorEffect = 0.0;
    int costumeIndex = 0;
    int zOrder = 0;
    Workspace ws;
    bool scriptRunning = false;
    int scriptPC = 0;
    bool stepRequested = false;
    uint32_t waitUntilMs = 0;
    bool waiting = false;
    vector<int> jumpTo;
    vector<int> jumpElse;
    vector<int> jumpEnd;
    vector<int> loopEnd;
    vector<int> loopStart;
    vector<int> repeatCounter;
    map<string, int> funcDefs;
    vector<int> callStack;
    double currentParam = 0.0;
};

struct AppState {
    vector<Sprite> sprites;
    int activeSprite = 0;

    Sprite& getActive() { return sprites[activeSprite]; }
    const Sprite& getActive() const { return sprites[activeSprite]; }

    bool quit = false;
    bool isPaused = false;
    bool settingsOpen = false;
    int  runSpeedMs = 30;
    bool drawActorWhenStopped = true;
    uint32_t nextStepAtMs = 0;
    vector<TextureAsset> costumes;
    vector<TextureAsset> backdrops;
    string actorIconFile = "costume0.bmp";
    InputState in;
    Logger log = Logger("log.txt");
    bool isFullscreen = false;
    SDL_Rect stageBounds {};
    vector<Button> buttons;
    string baseTitle = "YKP Base (SDL2)";
    TTF_Font* uiFont = nullptr;
    bool saveDialogOpen = false;
    bool loadDialogOpen = false;
    bool renameDialogOpen = false;
    string renameInput = "";
    string saveNameInput = "";
    vector<string> saveList;
    int loadHoverIndex = -1;
    int loadScroll = 0;
    bool helpMenuOpen = false;
    SDL_Rect helpButtonRect{0,0,0,0};
    bool showLogsPanel = false;
    int logsScroll = 0;
    bool debugStepMode = false;
    int backdropIndex = 0;
    string bubbleText = "";
    bool bubbleThink = false;
    uint32_t bubbleUntilMs = 0;
    bool askDialogOpen = false;
    string askQuestion = "";
    string askInput = "";
    string lastAnswer = "";
    int askResumePC = -1;
    uint32_t timerStartMs = 0;
    int soundVolume = 100;
    bool soundMuted = false;
    bool bgmReady = false;
    SDL_AudioDeviceID bgmDev = 0;
    SDL_AudioSpec bgmSpec{};
    Uint8* bgmBuf = nullptr;
    Uint32 bgmLen = 0;
    Uint32 bgmPos = 0;
    int  musicVolume = 100;
    bool musicMuted  = false;
    string bgmFile = "bgm.wav";
    map<string, Value> vars;
    map<string, bool> varVisible;
    Value lastValue = Value::Num(0.0);
    string lastBroadcast = "";
    bool extensionLibraryOpen = false;
    bool penExtensionEnabled = false;
    vector<Button> palette;
    bool paletteDirty = true;
    int paletteLastW = 0, paletteLastH = 0;
    bool paletteLastPenEnabled = false;
    int paletteScroll = 0;
    int paletteMaxScroll = 0;
    vector<pair<int,string>> paletteCats;
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
    bool funcIOMenuOpen = false;
    int  funcIOMenuBlockIndex = -1;
    TextureAsset actorIcon;
    bool audioReady = false;
    SDL_AudioDeviceID audioDev = 0;
    SDL_AudioSpec audioSpec{};
    uint32_t soundBusyUntilMs = 0;
    map<string, vector<Value>> lists;
    map<string, bool> listVisible;
    vector<CloneSprite> clones;
};

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

static void bgmTick(AppState& st) {
    if (!st.bgmReady || !st.bgmDev) return;
    if (!st.bgmBuf || st.bgmLen == 0) return;
    int vol = st.musicMuted ? 0 : clampT(st.musicVolume, 0, 100);
    if (vol <= 0) { bgmClearQueue(st); return; }
    Uint32 queuedBytes = SDL_GetQueuedAudioSize(st.bgmDev);
    int bytesPerSample = (SDL_AUDIO_BITSIZE(st.bgmSpec.format) / 8) * (int)st.bgmSpec.channels;
    if (bytesPerSample <= 0 || st.bgmSpec.freq <= 0) return;
    Uint32 targetMs = 300;
    Uint32 targetBytes = (Uint32)((st.bgmSpec.freq * bytesPerSample) * (targetMs / 1000.0));
    if (queuedBytes >= targetBytes) return;
    Uint32 chunkMs = 100;
    Uint32 chunkBytes = (Uint32)((st.bgmSpec.freq * bytesPerSample) * (chunkMs / 1000.0));
    if (chunkBytes < 256) chunkBytes = 256;
    Uint8* tmp = (Uint8*)SDL_malloc(chunkBytes);
    if (!tmp) return;
    SDL_memset(tmp, 0, chunkBytes);
    Uint32 remaining = chunkBytes;
    Uint32 writePos = 0;
    while (remaining > 0) {
        Uint32 avail = st.bgmLen - st.bgmPos;
        Uint32 take = (avail < remaining) ? avail : remaining;
        int sdlVol = (int)llround((vol / 100.0) * SDL_MIX_MAXVOLUME);
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
        SDL_Rect rowCostPrev = {box.x + 30,           box.y + 210, 60, 38};
        SDL_Rect rowCostNext = {box.x + box.w - 90,   box.y + 210, 60, 38};
        SDL_Rect rowCostMid  = {box.x + 100,          box.y + 210, box.w - 200, 38};
        SDL_Rect rowBackPrev = {box.x + 30,           box.y + 255, 60, 38};
        SDL_Rect rowBackNext = {box.x + box.w - 90,   box.y + 255, 60, 38};
        SDL_Rect rowBackMid  = {box.x + 100,          box.y + 255, box.w - 200, 38};
        SDL_Rect rowMusicDec = {box.x + 30,           box.y + 300, 60, 32};
        SDL_Rect rowMusicInc = {box.x + box.w - 90,   box.y + 300, 60, 32};
        SDL_Rect rowMusicMid = {box.x + 100,          box.y + 300, box.w - 320, 32};
        SDL_Rect rowMusicMute= {box.x + box.w - 250,  box.y + 300, 150, 32};

        if (pointInRect(mx,my,rowSpeedDec)) { st.runSpeedMs = clampT(st.runSpeedMs - 10, 0, 300); st.log.info(-1, "SET", "Speed -10", "runSpeedMs=" + to_string(st.runSpeedMs)); return true; }
        if (pointInRect(mx,my,rowSpeedInc)) { st.runSpeedMs = clampT(st.runSpeedMs + 10, 0, 300); st.log.info(-1, "SET", "Speed +10", "runSpeedMs=" + to_string(st.runSpeedMs)); return true; }
        if (pointInRect(mx,my,rowToggle)) { st.drawActorWhenStopped = !st.drawActorWhenStopped; st.log.info(-1, "SET", "Toggle draw actor when stopped", st.drawActorWhenStopped ? "ON" : "OFF"); return true; }
        if (pointInRect(mx,my,okBtn)) { st.settingsOpen = false; st.log.info(-1, "SET", "Close settings (OK)", ""); return true; }
        if (pointInRect(mx,my,rowCostPrev)) { if (!st.costumes.empty()) { st.getActive().costumeIndex--; if (st.getActive().costumeIndex < 0) st.getActive().costumeIndex = (int)st.costumes.size() - 1; } st.log.info(-1, "SET", "Costume prev", "idx=" + to_string(st.getActive().costumeIndex)); return true; }
        if (pointInRect(mx,my,rowCostNext)) { if (!st.costumes.empty()) { st.getActive().costumeIndex = (st.getActive().costumeIndex + 1) % (int)st.costumes.size(); } st.log.info(-1, "SET", "Costume next", "idx=" + to_string(st.getActive().costumeIndex)); return true; }
        if (pointInRect(mx,my,rowBackPrev)) { if (!st.backdrops.empty()) { st.backdropIndex--; if (st.backdropIndex < 0) st.backdropIndex = (int)st.backdrops.size() - 1; } st.log.info(-1, "SET", "Backdrop prev", "idx=" + to_string(st.backdropIndex)); return true; }
        if (pointInRect(mx,my,rowBackNext)) { if (!st.backdrops.empty()) { st.backdropIndex = (st.backdropIndex + 1) % (int)st.backdrops.size(); } st.log.info(-1, "SET", "Backdrop next", "idx=" + to_string(st.backdropIndex)); return true; }
        if (pointInRect(mx,my,rowMusicDec)) { st.musicVolume = clampT(st.musicVolume - 10, 0, 100); st.log.info(-1, "SET", "Music volume -10", "musicVolume=" + to_string(st.musicVolume)); return true; }
        if (pointInRect(mx,my,rowMusicInc)) { st.musicVolume = clampT(st.musicVolume + 10, 0, 100); st.log.info(-1, "SET", "Music volume +10", "musicVolume=" + to_string(st.musicVolume)); return true; }
        if (pointInRect(mx,my,rowMusicMid)) { int rel = mx - rowMusicMid.x; int v = (int)llround((rel / (double)max(1, rowMusicMid.w)) * 100.0); st.musicVolume = clampT(v, 0, 100); st.log.info(-1, "SET", "Music volume set", "musicVolume=" + to_string(st.musicVolume)); return true; }
        if (pointInRect(mx,my,rowMusicMute)) { st.musicMuted = !st.musicMuted; if (st.musicMuted) bgmClearQueue(st); st.log.info(-1, "SET", "Music mute toggle", st.musicMuted ? "MUTED" : "UNMUTED"); return true; }
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
    SDL_Rect tog = {box.x + 30, box.y + 150, box.w - 60, 44};
    SDL_SetRenderDrawColor(r, 25,25,28,255);
    SDL_RenderFillRect(r, &tog);
    SDL_SetRenderDrawColor(r, 120,120,120,255);
    SDL_RenderDrawRect(r, &tog);
    string tv = string("Show actor when stopped: ") + (st.drawActorWhenStopped ? "ON" : "OFF");
    renderText(r, st.uiFont, tv, tog.x + 12, tog.y + 12, white);
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
    int cIdx = cCount > 0 ? ((st.getActive().costumeIndex % cCount) + cCount) % cCount : 0;
    string cLabel = "Costume: " + to_string(cIdx) + " / " + to_string(max(0, cCount - 1));
    renderText(r, st.uiFont, cLabel, costMid.x + 10, costMid.y + 10, white);
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
    int fillW = (int)llround((clampT(st.musicVolume,0,100) / 100.0) * (musMid.w - 20));
    SDL_Rect bar{musMid.x + 10, musMid.y + musMid.h - 8, max(0, fillW), 4};
    SDL_SetRenderDrawColor(r, 200,200,200,255);
    SDL_RenderFillRect(r, &bar);
    renderTextCentered(r, st.uiFont, st.musicMuted ? "Muted" : "Mute", musMute, 0, white);
    SDL_Rect okBtn  = {box.x + box.w - 180, box.y + box.h - 60, 140, 40};
    SDL_SetRenderDrawColor(r, 60,140,70,255);
    SDL_RenderFillRect(r, &okBtn);
    SDL_SetRenderDrawColor(r, 20,20,20,255);
    SDL_RenderDrawRect(r, &okBtn);
    renderTextCentered(r, st.uiFont, "OK", okBtn, 0, white);
}

struct AppState;
struct Block;

static void setBlockVisual(Block& b);
static void penSyncRGB(AppState& st);

static bool saveProjectNamed(const string& saveStem, AppState& st) {
    const string path = buildSavePath(saveStem);
    ofstream f(path.c_str());
    if (!f) { st.log.log("SAVE", "Cannot open: " + path); return false; }
    f << "SAVE_V1\n";
    f << "BLOCKS " << st.getActive().ws.blocks.size() << "\n";
    for (size_t i = 0; i < st.getActive().ws.blocks.size(); i++) {
        const Block& b = st.getActive().ws.blocks[i];
        f << "BLOCK "
          << b.id << " "
          << b.rect.x << " " << b.rect.y << " "
          << b.rect.w << " " << b.rect.h << " "
          << (int)b.color.r << " " << (int)b.color.g << " " << (int)b.color.b << " "
          << b.cmd << " "
          << b.a << " " << b.b << " "
          << b.i1 << " "
          << (int)b.pickColor.r << " " << (int)b.pickColor.g << " " << (int)b.pickColor.b << " "
          << b.inSel << " " << b.outSel << " "
          << std::quoted(b.s1) << " " << std::quoted(b.s2)
          << "\n";
    }
    f << "SETTINGS " << st.runSpeedMs << " " << (st.drawActorWhenStopped ? 1 : 0) << "\n";
    f << "LOOKS " << st.getActive().costumeIndex << " " << st.backdropIndex << " " << st.getActive().colorEffect << "\n";
    f << "EXT_PEN " << (st.penExtensionEnabled ? 1 : 0) << "\n";
    f << "PEN_STATE " << (st.penDown ? 1 : 0) << " " << st.penHue << " " << st.penSat << " " << st.penBri << " " << st.penSize << "\n";
    f << "PEN_SEGS " << st.penSegs.size() << "\n";
    for (const auto& seg : st.penSegs) {
        f << "SEG " << seg.x1 << " " << seg.y1 << " " << seg.x2 << " " << seg.y2 << " "
          << (int)seg.c.r << " " << (int)seg.c.g << " " << (int)seg.c.b << " " << seg.size << "\n";
    }
    f << "PEN_STAMPS " << st.penStamps.size() << "\n";
    for (const auto& sp : st.penStamps) {
        f << "STAMP " << sp.x << " " << sp.y << " " << sp.costumeIndex << " " << sp.dirDeg << " " << sp.sizePct << "\n";
    }
    f << "VARS " << st.vars.size() << "\n";
    for (const auto& kv : st.vars) {
        const Value& v = kv.second;
        f << "VAR " << kv.first << " " << (v.isNum ? 1 : 0) << " ";
        if (v.isNum) f << v.num << " " << std::quoted("") << "\n";
        else         f << 0.0  << " " << std::quoted(v.str) << "\n";
    }
    f << "VARVIS " << st.varVisible.size() << "\n";
    for (const auto& kv : st.varVisible) {
        f << "VARV " << kv.first << " " << (kv.second ? 1 : 0) << "\n";
    }
    f << "LISTS " << st.lists.size() << "\n";
    for (const auto& kv : st.lists) {
        f << "LIST " << kv.first << " " << kv.second.size() << "\n";
        for (const auto& it : kv.second) {
            f << "ITEM " << (it.isNum ? 1 : 0) << " ";
            if (it.isNum) f << it.num << " " << std::quoted("") << "\n";
            else          f << 0.0   << " " << std::quoted(it.str) << "\n";
        }
    }
    f << "LISTVIS " << st.listVisible.size() << "\n";
    for (const auto& kv : st.listVisible) {
        f << "LISTV " << kv.first << " " << (kv.second ? 1 : 0) << "\n";
    }
    st.log.log("SAVE", "Saved: " + path);
    return true;
}

static bool loadProjectNamed(const string& saveStem, AppState& st) {
    const string path = buildSavePath(saveStem);
    ifstream f(path.c_str());
    if (!f) { st.log.log("LOAD", "Cannot open: " + path); return false; }
    st.getActive().ws.reset();
    st.penSegs.clear();
    st.penStamps.clear();
    st.vars.clear();
    st.varVisible.clear();
    st.lists.clear();
    st.listVisible.clear();
    string header;
    if (!getline(f, header)) return false;
    if (header != "SAVE_V1") { st.log.log("LOAD", "Invalid save header: " + header); return false; }
    string line;
    int maxId = 0;
    size_t expectBlocks = 0, expectPenSegs = 0, expectPenStamps = 0;
    size_t expectVars = 0, expectVarVis = 0, expectLists = 0, expectListVis = 0;
    while (getline(f, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string tag;
        ss >> tag;
        if (tag == "BLOCKS") { ss >> expectBlocks; continue; }
        if (tag == "BLOCK") {
            Block b;
            int r=80,g=80,bl=90,pr=0,pg=255,pb=0;
            ss >> b.id >> b.rect.x >> b.rect.y >> b.rect.w >> b.rect.h
               >> r >> g >> bl >> b.cmd >> b.a >> b.b >> b.i1
               >> pr >> pg >> pb >> b.inSel >> b.outSel
               >> std::quoted(b.s1) >> std::quoted(b.s2);
            b.color = SDL_Color{(Uint8)r,(Uint8)g,(Uint8)bl,255};
            b.pickColor = SDL_Color{(Uint8)pr,(Uint8)pg,(Uint8)pb,255};
            setBlockVisual(b);
            maxId = max(maxId, b.id);
            st.getActive().ws.blocks.push_back(b);
            continue;
        }
        if (tag == "SETTINGS") { int drawFlag=1; ss >> st.runSpeedMs >> drawFlag; st.drawActorWhenStopped = (drawFlag != 0); continue; }
        if (tag == "LOOKS") { ss >> st.getActive().costumeIndex >> st.backdropIndex >> st.getActive().colorEffect; continue; }
        if (tag == "EXT_PEN") { int v=0; ss >> v; st.penExtensionEnabled = (v != 0); continue; }
        if (tag == "PEN_STATE") { int down=0; ss >> down >> st.penHue >> st.penSat >> st.penBri >> st.penSize; st.penDown = (down != 0); penSyncRGB(st); continue; }
        if (tag == "PEN_SEGS") { ss >> expectPenSegs; continue; }
        if (tag == "SEG") { PenSegment seg; int cr=0,cg=255,cb=0; ss >> seg.x1 >> seg.y1 >> seg.x2 >> seg.y2 >> cr >> cg >> cb >> seg.size; seg.c = SDL_Color{(Uint8)cr,(Uint8)cg,(Uint8)cb,255}; st.penSegs.push_back(seg); continue; }
        if (tag == "PEN_STAMPS") { ss >> expectPenStamps; continue; }
        if (tag == "STAMP") { PenStamp sp; ss >> sp.x >> sp.y >> sp.costumeIndex >> sp.dirDeg >> sp.sizePct; st.penStamps.push_back(sp); continue; }
        if (tag == "VARS") { ss >> expectVars; continue; }
        if (tag == "VAR") { string name; int isNum=1; double num=0.0; string str; ss >> name >> isNum >> num >> std::quoted(str); if (isNum) st.vars[name] = Value::Num(num); else st.vars[name] = Value::Str(str); continue; }
        if (tag == "VARVIS") { ss >> expectVarVis; continue; }
        if (tag == "VARV") { string name; int v=0; ss >> name >> v; st.varVisible[name] = (v!=0); continue; }
        if (tag == "LISTS") { ss >> expectLists; continue; }
        if (tag == "LIST") {
            string name; size_t n=0; ss >> name >> n;
            auto& L = st.lists[name]; L.clear();
            for (size_t i = 0; i < n; i++) {
                string itemLine; if (!getline(f, itemLine)) break;
                stringstream is(itemLine); string itag; is >> itag;
                if (itag != "ITEM") break;
                int isNum=1; double num=0.0; string s;
                is >> isNum >> num >> std::quoted(s);
                if (isNum) L.push_back(Value::Num(num));
                else       L.push_back(Value::Str(s));
            }
            continue;
        }
        if (tag == "LISTVIS") { ss >> expectListVis; continue; }
        if (tag == "LISTV") { string name; int v=0; ss >> name >> v; st.listVisible[name] = (v!=0); continue; }
    }
    st.getActive().ws.nextId = maxId + 1;
    st.paletteDirty = true;
    st.log.log("LOAD", "Loaded: " + path);
    return true;
}

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
    st.log.info(st.getActive().scriptPC, "ASK", "Open ask dialog", "q=" + question);
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
    if (st.renameDialogOpen) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) { st.renameDialogOpen = false; SDL_StopTextInput(); return true; }
            if (e.key.keysym.sym == SDLK_BACKSPACE) { if (!st.renameInput.empty()) st.renameInput.pop_back(); return true; }
            if (e.key.keysym.sym == SDLK_RETURN) { if (!st.renameInput.empty()) st.getActive().name = st.renameInput; st.renameDialogOpen = false; SDL_StopTextInput(); return true; }
        }
        if (e.type == SDL_TEXTINPUT) { st.renameInput += e.text.text; return true; }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            SDL_Rect modal = {winW/2 - 220, winH/2 - 90, 440, 180};
            SDL_Rect okBtn  = {modal.x + 260, modal.y + 120, 140, 40};
            SDL_Rect canBtn = {modal.x +  40, modal.y + 120, 140, 40};
            if (pointInRect(e.button.x, e.button.y, okBtn)) { if (!st.renameInput.empty()) st.getActive().name = st.renameInput; st.renameDialogOpen = false; SDL_StopTextInput(); return true; }
            if (pointInRect(e.button.x, e.button.y, canBtn)) { st.renameDialogOpen = false; SDL_StopTextInput(); return true; }
            return true;
        }
        return true;
    }

    if (st.askDialogOpen) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) { st.askDialogOpen = false; SDL_StopTextInput(); st.lastAnswer = ""; st.getActive().scriptPC = st.askResumePC; st.askResumePC = -1; st.log.warn(st.getActive().scriptPC, "ASK", "Ask cancelled", ""); return true; }
            if (e.key.keysym.sym == SDLK_BACKSPACE) { if (!st.askInput.empty()) st.askInput.pop_back(); return true; }
            if (e.key.keysym.sym == SDLK_RETURN) { st.lastAnswer = st.askInput; st.askDialogOpen = false; SDL_StopTextInput(); st.getActive().scriptPC = st.askResumePC; st.askResumePC = -1; st.log.info(st.getActive().scriptPC, "ASK", "Answer received", "ans=" + st.lastAnswer); return true; }
        }
        if (e.type == SDL_TEXTINPUT) { st.askInput += e.text.text; return true; }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;
            SDL_Rect modal = {winW/2 - 260, winH/2 - 120, 520, 240};
            SDL_Rect okBtn  = {modal.x + modal.w - 180, modal.y + modal.h - 60, 140, 40};
            SDL_Rect canBtn = {modal.x + 40, modal.y + modal.h - 60, 140, 40};
            if (pointInRect(mx, my, okBtn)) { st.lastAnswer = st.askInput; st.askDialogOpen = false; SDL_StopTextInput(); st.getActive().scriptPC = st.askResumePC; st.askResumePC = -1; st.log.info(st.getActive().scriptPC, "ASK", "Answer received (click)", "ans=" + st.lastAnswer); return true; }
            if (pointInRect(mx, my, canBtn)) { st.askDialogOpen = false; SDL_StopTextInput(); st.lastAnswer = ""; st.getActive().scriptPC = st.askResumePC; st.askResumePC = -1; st.log.warn(st.getActive().scriptPC, "ASK", "Ask cancelled (click)", ""); return true; }
            return true;
        }
        return true;
    }

    if (st.saveDialogOpen) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) { st.saveDialogOpen = false; SDL_StopTextInput(); return true; }
            if (e.key.keysym.sym == SDLK_BACKSPACE) { if (!st.saveNameInput.empty()) st.saveNameInput.pop_back(); return true; }
            if (e.key.keysym.sym == SDLK_RETURN) { string stem = sanitizeStem(st.saveNameInput); bool ok = saveProjectNamed(stem, st); st.saveDialogOpen = false; SDL_StopTextInput(); if (ok) infoBox("Saved", "Project saved successfully."); else infoBox("Save failed", "Could not save the project."); return true; }
        }
        if (e.type == SDL_TEXTINPUT) { st.saveNameInput += e.text.text; return true; }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;
            SDL_Rect modal = {winW/2 - 220, winH/2 - 90, 440, 180};
            SDL_Rect okBtn  = {modal.x + 260, modal.y + 120, 140, 40};
            SDL_Rect canBtn = {modal.x +  40, modal.y + 120, 140, 40};
            if (pointInRect(mx, my, okBtn)) { string stem = sanitizeStem(st.saveNameInput); bool ok = saveProjectNamed(stem, st); st.saveDialogOpen = false; SDL_StopTextInput(); if (ok) infoBox("Saved", "Project saved successfully."); else infoBox("Save failed", "Could not save the project."); return true; }
            if (pointInRect(mx, my, canBtn)) { st.saveDialogOpen = false; SDL_StopTextInput(); return true; }
            return true;
        }
        return true;
    }

    if (st.loadDialogOpen) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) { st.loadDialogOpen = false; return true; }
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
            if (!pointInRect(mx, my, modal)) { st.loadDialogOpen = false; return true; }
            if (pointInRect(mx, my, listArea)) {
                const int rowH = 28;
                int local = (my - listArea.y) / rowH;
                int idx = st.loadScroll + local;
                if (idx >= 0 && idx < (int)st.saveList.size()) {
                    bool ok = loadProjectNamed(st.saveList[idx], st);
                    st.loadDialogOpen = false;
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
    if (!st.saveDialogOpen && !st.loadDialogOpen && !st.askDialogOpen && !st.renameDialogOpen) return;

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

    if (st.renameDialogOpen) {
        SDL_Rect modal = {winW/2 - 220, winH/2 - 90, 440, 180};
        SDL_SetRenderDrawColor(r, 40, 40, 46, 255);
        SDL_RenderFillRect(r, &modal);
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderDrawRect(r, &modal);

        renderText(r, st.uiFont, "Rename Sprite", modal.x + 20, modal.y + 14, white);

        SDL_Rect field = {modal.x + 20, modal.y + 55, modal.w - 40, 40};
        SDL_SetRenderDrawColor(r, 25, 25, 28, 255);
        SDL_RenderFillRect(r, &field);
        SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
        SDL_RenderDrawRect(r, &field);

        string shown = st.renameInput.empty() ? "type new name..." : st.renameInput;
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
    s.x = st.getActive().x;
    s.y = st.getActive().y;
    s.costumeIndex = st.getActive().costumeIndex;
    s.dirDeg = st.getActive().dirDeg;
    s.sizePct = st.getActive().sizePct;
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

    for (const auto& seg : st.penSegs) {
        drawThickLine(r, seg.x1, seg.y1, seg.x2, seg.y2, seg.c, seg.size);
    }

    for (const auto& sp : st.penStamps) {
        SDL_Texture* useTex = nullptr;

        if (!st.costumes.empty()) {
            int ci = sp.costumeIndex;
            if (ci < 0) ci = 0;
            ci %= (int)st.costumes.size();
            if (st.costumes[ci].tex) useTex = st.costumes[ci].tex;
        }
        if (!useTex && st.actorIcon.tex) useTex = st.actorIcon.tex;

        int sizePx = (int)clampT((int)round(80.0 * (sp.sizePct / 100.0)), 10, 300);
        SDL_Rect dst{(int)round(sp.x) - sizePx/2, (int)round(sp.y) - sizePx/2, sizePx, sizePx};

        if (useTex) {
            double angle = sp.dirDeg - 90.0;
            SDL_RenderCopyEx(r, useTex, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
            SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
            SDL_RenderDrawRect(r, &dst);
        } else {
            SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
            SDL_RenderFillRect(r, &dst);
            SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
            SDL_RenderDrawRect(r, &dst);
        }
    }
}


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
                    st.penColorPickerBlockIndex < (int)st.getActive().ws.blocks.size()) {
                    Block& b = st.getActive().ws.blocks[st.penColorPickerBlockIndex];
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


static SDL_Rect funcIOMenuRect(int w, int h) {
    return SDL_Rect{w/2 - 260, h/2 - 150, 520, 300};
}

static const vector<string>& funcList() {
    static vector<string> v = {"sqrt","abs","sin","cos","tan","round","floor","ceil"};
    return v;
}
static const vector<string>& funcInputList() {
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
    if (blockIndex < 0 || blockIndex >= (int)st.getActive().ws.blocks.size()) return;
    if (st.getActive().ws.blocks[blockIndex].cmd != "FUNC_APPLY") return;

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
        if (bi < 0 || bi >= (int)st.getActive().ws.blocks.size()) {
            closeFuncIOMenu(st, "invalid index");
            return true;
        }
        Block& b = st.getActive().ws.blocks[bi];

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
    if (bi >= 0 && bi < (int)st.getActive().ws.blocks.size()) {
        const Block& b = st.getActive().ws.blocks[bi];
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

static Button makePaletteBtn(int x, int y, int w, const string& label, const string& sub, function<void()> cb) {
    Button b;
    b.rect = SDL_Rect{x, y, w, 34};
    b.text = label;
    b.sub = sub;
    b.onPress = cb;
    return b;
}


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
    else if (b.cmd == "DEFINE_FN" || b.cmd == "END_FN" || b.cmd == "CALL_FN" || b.cmd == "GET_PARAM") b.color = SDL_Color{255, 105, 180, 255}; // Pink My Blocks
    else b.color = SDL_Color{80, 80, 90, 255};
}

static void addTypedBlock(AppState& st, const string& cmd, double a, double bb, const string& s1 = "", const string& s2 = "", int i1 = 0) {
    int spawnX = st.in.mx - 120;
    int spawnY = st.in.my - 26;
    st.getActive().ws.addBlock(spawnX, spawnY);

    if (!st.getActive().ws.blocks.empty()) {
        Block& b = st.getActive().ws.blocks.back();
        b.cmd = cmd;
        b.a = a;
        b.b = bb;
        b.s1 = s1;
        b.s2 = s2;
        b.i1 = i1;
        setBlockVisual(b);

        b.dragging = true;
        b.offX = st.in.mx - b.rect.x;
        b.offY = st.in.my - b.rect.y;

        st.log.info((int)st.getActive().ws.blocks.size() - 1, cmd, "Add & Drag block", "");
    }
}


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
    placeBtn("go to front layer", "", [&]{ addTypedBlock(st, "LAYER_FRONT", 0.0, 0.0); });
    placeBtn("go to back layer", "", [&]{ addTypedBlock(st, "LAYER_BACK", 0.0, 0.0); });
    placeBtn("go forward 1 layers", "", [&]{ addTypedBlock(st, "LAYER_FWD", 1.0, 0.0); });
    placeBtn("go backward 1 layers", "", [&]{ addTypedBlock(st, "LAYER_BWD", 1.0, 0.0); });
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
    placeBtn("touching [Sprite 2]?", "Shift+Click edit", [&]{ addTypedBlock(st, "TOUCH_SPRITE", 0.0, 0.0, "Sprite 2"); });
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
        if (!st.getActive().ws.blocks.empty()) {
            Block& b = st.getActive().ws.blocks.back();
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

    cat("Clones");
    placeBtn("create clone", "", [&]{ addTypedBlock(st, "CLONE_CREATE", 0.0, 0.0); });
    placeBtn("delete last clone", "", [&]{ addTypedBlock(st, "CLONE_DELETE_LAST", 0.0, 0.0); });
    placeBtn("clear clones", "", [&]{ addTypedBlock(st, "CLONE_CLEAR", 0.0, 0.0); });
    placeBtn("clone count (to last)", "", [&]{ addTypedBlock(st, "CLONE_COUNT", 0.0, 0.0); });

    cat("My Blocks");
    placeBtn("Define f(x)", "Shift+Click cycle name", [&]{ addTypedBlock(st, "DEFINE_FN", 0.0, 0.0, "myFunc", "x"); });
    placeBtn("End Function", "", [&]{ addTypedBlock(st, "END_FN", 0.0, 0.0); });
    placeBtn("Call f(n)", "Shift+Click cycle name", [&]{ addTypedBlock(st, "CALL_FN", 10.0, 0.0, "myFunc"); });
    placeBtn("Get param x", "", [&]{ addTypedBlock(st, "GET_PARAM", 0.0, 0.0, "x"); });

    if (st.penExtensionEnabled) {
        cat("Pen (Extension)");
        placeBtn("PEN Down", "", [&]{ addTypedBlock(st, "PEN_DOWN", 0.0, 0.0); });
        placeBtn("PEN Up", "", [&]{ addTypedBlock(st, "PEN_UP", 0.0, 0.0); });
        placeBtn("Stamp", "", [&]{ addTypedBlock(st, "PEN_STAMP", 0.0, 0.0); });
        placeBtn("All Erase", "", [&]{ addTypedBlock(st, "PEN_ERASE_ALL", 0.0, 0.0); });
        placeBtn("Set Size (3)", "", [&]{ addTypedBlock(st, "PEN_SET_SIZE", 3.0, 0.0); });
        placeBtn("Change Size (+1)", "", [&]{ addTypedBlock(st, "PEN_CHANGE_SIZE", 1.0, 0.0); });
        placeBtn("Set Color (picker)", "Shift+Click edit", [&]{
            addTypedBlock(st, "PEN_SET_COLOR", 0.0, 0.0);
            if (!st.getActive().ws.blocks.empty()) {
                st.getActive().ws.blocks.back().pickColor = st.penRGB;
                setBlockVisual(st.getActive().ws.blocks.back());
            }
        });
    }

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
        if (y < TOP_BAR_H + 55 || y > st.getActive().ws.bounds.y + st.getActive().ws.bounds.h) continue;
        renderText(r, st.uiFont, it.second, 12, y, c);
    }
}

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


static void stopScript(AppState& st, Sprite& sp, const string& reason, const string& level = "WARNING") {
    if (!sp.scriptRunning) return;
    sp.scriptRunning = false;
    sp.waiting = false;
    sp.waitUntilMs = 0;
    st.soundBusyUntilMs = 0;
    if (level == "ERROR") st.log.error(sp.scriptPC, "RUN", "Stop script", reason);
    else                  st.log.warn(sp.scriptPC, "RUN", "Stop script", reason);
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

static void clampActorPos(AppState& st, int blockIndex, const string& cmd, double beforeX, double beforeY, Sprite& sp) {
    double minX = st.stageBounds.x;
    double maxX = st.stageBounds.x + st.stageBounds.w;
    double minY = st.stageBounds.y;
    double maxY = st.stageBounds.y + st.stageBounds.h;

    double ox = sp.x, oy = sp.y;

    if (sp.x < minX) sp.x = minX;
    if (sp.x > maxX) sp.x = maxX;
    if (sp.y < minY) sp.y = minY;
    if (sp.y > maxY) sp.y = maxY;

    bool clamped = (sp.x != ox) || (sp.y != oy);
    if (clamped) {
        st.log.warn(blockIndex, cmd, "Boundary clamp",
                    "pos(" + to_string(beforeX) + "," + to_string(beforeY) + ")->(" +
                    to_string(sp.x) + "," + to_string(sp.y) + ")");
    }
}

static double degToRad(double d) { return d * 3.14159265358979323846 / 180.0; }

static double scratchRad(double dirDeg) {
    return degToRad(90.0 - dirDeg);
}

static double scratchToSDLAangle(double dirDeg) {
    return dirDeg - 90.0;
}

static void runnerPreScan(AppState& st, Sprite& sp) {
    int n = (int)sp.ws.blocks.size();
    sp.jumpTo.assign(n, -1);
    sp.jumpElse.assign(n, -1);
    sp.jumpEnd.assign(n, -1);
    sp.loopEnd.assign(n, -1);
    sp.loopStart.assign(n, -1);
    sp.repeatCounter.assign(n, 0);

    vector<int> ifStack;
    vector<int> ifElseStack;
    vector<int> repeatStack;
    vector<int> foreverStack;
    vector<int> repeatUntilStack;
    vector<int> funcStack;

    for (int i = 0; i < n; i++) {
        const string& c = sp.ws.blocks[i].cmd;

        if (c == "DEFINE_FN") {
            funcStack.push_back(i);
            sp.funcDefs[sp.ws.blocks[i].s1] = i;
            continue;
        }
        if (c == "END_FN") {
            if (!funcStack.empty()) {
                int start = funcStack.back();
                funcStack.pop_back();
                sp.jumpEnd[start] = i;
            }
            continue;
        }

        if (c == "IF" || c == "IFELSE") {
            ifStack.push_back(i);
            if (c == "IFELSE") ifElseStack.push_back(i);
            continue;
        }

        if (c == "ELSE") {
            if (!ifStack.empty()) {
                int start = ifStack.back();
                sp.jumpElse[start] = i;
            }
            continue;
        }

        if (c == "END_IF") {
            if (!ifStack.empty()) {
                int start = ifStack.back();
                ifStack.pop_back();
                sp.jumpEnd[start] = i;
                if (sp.jumpElse[start] != -1) {
                    int elseIdx = sp.jumpElse[start];
                    sp.jumpTo[elseIdx] = i;
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
                sp.loopEnd[start] = i;
                sp.loopStart[i] = start;
            } else if (!repeatStack.empty()) {
                int start = repeatStack.back();
                repeatStack.pop_back();
                sp.loopEnd[start] = i;
                sp.loopStart[i] = start;
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
                sp.loopEnd[start] = i;
                sp.loopStart[i] = start;
            }
            continue;
        }
    }

    for (int idx : ifStack) st.log.warn(idx, "IF", "Unmatched IF (missing END_IF)", "");
    for (int idx : repeatStack) st.log.warn(idx, "REPEAT", "Unmatched REPEAT (missing END_REPEAT)", "");
    for (int idx : repeatUntilStack) st.log.warn(idx, "REPEAT_UNTIL", "Unmatched REPEAT_UNTIL (missing END_REPEAT)", "");
    for (int idx : foreverStack) st.log.warn(idx, "FOREVER", "Unmatched FOREVER (missing END_FOREVER)", "");
    for (int idx : funcStack) st.log.warn(idx, "DEFINE_FN", "Unmatched DEFINE_FN (missing END_FN)", ""); // <--- این خط جدید
}

static void startScript(AppState& st) {
    st.timerStartMs = SDL_GetTicks();
    st.soundBusyUntilMs = 0;

    st.clones.clear();
    st.penSegs.clear();
    st.penStamps.clear();

    for (auto& sp : st.sprites) {
        sp.x = sp.backupX;
        sp.y = sp.backupY;
        sp.dirDeg = sp.backupDirDeg;
        sp.visible = sp.backupVisible;
        sp.sizePct = sp.backupSizePct;
        sp.colorEffect = sp.backupColorEffect;
        sp.costumeIndex = sp.backupCostumeIndex;
        sp.zOrder = sp.backupZOrder;


        if (!sp.ws.blocks.empty() && sp.ws.blocks.front().cmd == "EVENT_FLAG") {
            sp.scriptRunning = true;
        } else {
            sp.scriptRunning = false;
        }
        sp.scriptPC = 0;
        sp.stepRequested = false;
        sp.waiting = false;
        sp.waitUntilMs = 0;

        sp.funcDefs.clear();
        sp.callStack.clear();
        sp.currentParam = 0.0;

        runnerPreScan(st, sp);
    }
}

static SDL_Color applyLookEffect(SDL_Color base, double hueShiftDeg) {
    double h,s,v;
    rgbToHsv(base, h,s,v);
    h = fmod(h + hueShiftDeg, 360.0);
    if (h < 0) h += 360.0;
    return hsvToRgb(h,s,v);
}


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


static string openBMPDialog(SDL_Window* owner) {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(owner, &wmInfo);
    ofn.hwndOwner = wmInfo.info.win.window;

    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "BMP Files\0*.bmp\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return string(ofn.lpstrFile);
    }
#endif
    return "";
}


static void initAssets(AppState& st, SDL_Renderer* r) {
    destroyTextureAsset(st.actorIcon);
    st.actorIcon = loadBMPTexture(r, st.actorIconFile, st.log);

    st.costumes.clear();
    st.costumes.push_back(loadBMPTexture(r, "costume0.bmp", st.log));
    st.costumes.push_back(loadBMPTexture(r, "costume1.bmp", st.log));

    st.backdrops.clear();
}

static void shutdownAssets(AppState& st) {
    destroyTextureAsset(st.actorIcon);

    for (auto& t : st.costumes) destroyTextureAsset(t);
    st.costumes.clear();

    for (auto& t : st.backdrops) destroyTextureAsset(t);
    st.backdrops.clear();
}


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


    SDL_ClearQueuedAudio(st.audioDev);

    Uint8* mixBuf = (Uint8*)SDL_malloc(len);
    if (!mixBuf) {
        SDL_FreeWAV(srcBuf);
        if (cvtBuf) SDL_free(cvtBuf);
        st.log.warn(-1, "AUDIO", "malloc failed", "len=" + to_string(len));
        return 0;
    }
    SDL_memset(mixBuf, 0, len);

    int sdlVol = (int)llround((vol / 100.0) * SDL_MIX_MAXVOLUME);
    SDL_MixAudioFormat(mixBuf, buf, st.audioSpec.format, len, sdlVol);
    SDL_QueueAudio(st.audioDev, mixBuf, len);

    uint32_t ms = computeAudioMs(st.audioSpec, len);
    st.log.info(-1, "AUDIO", "Play WAV", wavFile + " ms=" + to_string(ms));

    SDL_free(mixBuf);
    SDL_FreeWAV(srcBuf);
    if (cvtBuf) SDL_free(cvtBuf);

    return ms;
}


static vector<Value>& getList(AppState& st, const string& name) {
    return st.lists[name];
}

static Value listItemFromBlock(const Block& b) {
    if (!b.s2.empty()) return Value::Str(b.s2);
    return Value::Num(b.a);
}

static bool listIndexOk1(int idx1, int n) {
    return (idx1 >= 1 && idx1 <= n);
}


static void cloneCreateFromActor(AppState& st) {
    CloneSprite c;
    c.x = st.getActive().x;
    c.y = st.getActive().y;
    c.dirDeg = st.getActive().dirDeg;
    c.visible = st.getActive().visible;
    c.sizePct = st.getActive().sizePct;
    st.clones.push_back(c);
}

static void cloneDeleteLast(AppState& st) {
    if (!st.clones.empty()) st.clones.pop_back();
}

static void cloneClearAll(AppState& st) {
    st.clones.clear();
}


static void drawSprite(SDL_Renderer* r, const AppState& st, const Sprite& sp) {
    if (!sp.visible) return;

    int sizePx = (int)clampT((int)round(80.0 * (sp.sizePct / 100.0)), 10, 300);
    SDL_Rect dst{(int)round(sp.x) - sizePx/2, (int)round(sp.y) - sizePx/2, sizePx, sizePx};

    SDL_Texture* useTex = sp.icon.tex;
    if (useTex) {
        double angle = scratchToSDLAangle(sp.dirDeg);
        SDL_Color mod = applyLookEffect(SDL_Color{255,255,255,255}, sp.colorEffect);
        SDL_SetTextureColorMod(useTex, mod.r, mod.g, mod.b);
        SDL_RenderCopyEx(r, useTex, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
        SDL_SetTextureColorMod(useTex, 255, 255, 255);

        SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
        SDL_RenderDrawRect(r, &dst);
    } else {
        SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
        SDL_RenderFillRect(r, &dst);
        SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
        SDL_RenderDrawRect(r, &dst);
    }

    double rad = scratchRad(sp.dirDeg);
    int x2 = (int)round(sp.x + cos(rad) * (sizePx/2 + 10));
    int y2 = (int)round(sp.y - sin(rad) * (sizePx/2 + 10));
    SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
    SDL_RenderDrawLine(r, (int)round(sp.x), (int)round(sp.y), x2, y2);
}

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
    if (inSel == "actorX") return st.getActive().x;
    if (inSel == "actorY") return st.getActive().y;
    if (inSel == "dir")    return st.getActive().dirDeg;

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
        out = sin(degToRad(x));
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

    out = x;
    return true;
}

static string cycleName3(const string& cur) {
    if (cur == "v") return "score";
    if (cur == "score") return "msg";
    return "v";
}

static string cycleFuncName(const string& cur) {
    if (cur == "myFunc") return "drawShape";
    if (cur == "drawShape") return "jumpUp";
    return "myFunc";
}

static string cycleListName3(const string& cur) {
    if (cur == "list") return "list2";
    if (cur == "list2") return "list3";
    return "list";
}

enum class StepResult { Advanced, Yielded, Stopped };

static StepResult executeOneBlock(AppState& st, Sprite& sp) {
    if (!sp.scriptRunning) return StepResult::Stopped;
    if (st.askDialogOpen)  return StepResult::Yielded;

    if (sp.waiting) {
        if (SDL_GetTicks() < sp.waitUntilMs) return StepResult::Yielded;
        sp.waiting = false;
        sp.waitUntilMs = 0;
    }

    if (sp.scriptPC < 0 || sp.scriptPC >= (int)sp.ws.blocks.size()) {
        stopScript(st, sp, "Reached end", "WARNING");
        return StepResult::Stopped;
    }

    Block& b = sp.ws.blocks[sp.scriptPC];
    int idx = sp.scriptPC;
    string cmd = b.cmd;

    if (cmd == "EVENT_FLAG" || cmd == "EVENT_KEY" || cmd == "EVENT_CLICK") {
        st.log.info(idx, cmd, "Event block", "pass");
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "BROADCAST") {
        st.lastBroadcast = b.s1.empty() ? "msg" : b.s1;
        st.log.info(idx, cmd, "Broadcast", "msg=" + st.lastBroadcast);
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "WHEN_RECEIVE") {
        st.log.info(idx, cmd, "When receive", "msg=" + b.s1);
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "MOVE") {
        double bx = sp.x, by = sp.y;
        sp.x += b.a;
        clampActorPos(st, idx, cmd, bx, by, sp);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, sp.x, sp.y);

        st.log.info(idx, cmd, "Change X", "x:" + to_string(bx) + "->" + to_string(sp.x));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "MOVE_STEPS") {
        double bx = sp.x, by = sp.y;
        double rad = scratchRad(sp.dirDeg);
        sp.x += cos(rad) * b.a;
        sp.y -= sin(rad) * b.a;
        clampActorPos(st, idx, cmd, bx, by, sp);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, sp.x, sp.y);

        st.log.info(idx, cmd, "Move steps",
                    "pos(" + to_string(bx) + "," + to_string(by) + ")->(" +
                    to_string(sp.x) + "," + to_string(sp.y) + ")");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "TURN_R" || cmd == "TURN_L") {
        double before = sp.dirDeg;
        double delta = (cmd == "TURN_R") ? b.a : -b.a;
        sp.dirDeg = fmod(sp.dirDeg + delta, 360.0);
        if (sp.dirDeg < 0) sp.dirDeg += 360.0;
        st.log.info(idx, cmd, "Turn", "dir:" + to_string(before) + "->" + to_string(sp.dirDeg));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SET_DIR") {
        double before = sp.dirDeg;
        sp.dirDeg = fmod(b.a, 360.0);
        if (sp.dirDeg < 0) sp.dirDeg += 360.0;
        st.log.info(idx, cmd, "Set direction", "dir:" + to_string(before) + "->" + to_string(sp.dirDeg));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "GOTO_XY") {
        double bx = sp.x, by = sp.y;
        sp.x = b.a;
        sp.y = b.b;
        clampActorPos(st, idx, cmd, bx, by, sp);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, sp.x, sp.y);

        st.log.info(idx, cmd, "Go to",
                    "pos(" + to_string(bx) + "," + to_string(by) + ")->(" +
                    to_string(sp.x) + "," + to_string(sp.y) + ")");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "CHANGE_X") {
        double bx = sp.x, by = sp.y;
        sp.x += b.a;
        clampActorPos(st, idx, cmd, bx, by, sp);
        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, sp.x, sp.y);

        st.log.info(idx, cmd, "Change X", "x:" + to_string(bx) + "->" + to_string(sp.x));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "CHANGE_Y") {
        double bx = sp.x, by = sp.y;
        sp.y += b.a;
        clampActorPos(st, idx, cmd, bx, by, sp);
        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, sp.x, sp.y);

        st.log.info(idx, cmd, "Change Y", "y:" + to_string(by) + "->" + to_string(sp.y));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "GOTO_RANDOM") {
        double bx = sp.x, by = sp.y;
        double minX = st.stageBounds.x;
        double maxX = st.stageBounds.x + st.stageBounds.w;
        double minY = st.stageBounds.y;
        double maxY = st.stageBounds.y + st.stageBounds.h;

        sp.x = minX + (rand() / (double)RAND_MAX) * (maxX - minX);
        sp.y = minY + (rand() / (double)RAND_MAX) * (maxY - minY);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, sp.x, sp.y);
        st.log.info(idx, cmd, "Go random", "pos(" + to_string(sp.x) + "," + to_string(sp.y) + ")");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "GOTO_MOUSE") {
        double bx = sp.x, by = sp.y;
        sp.x = st.in.mx;
        sp.y = st.in.my;
        clampActorPos(st, idx, cmd, bx, by, sp);

        if (st.penExtensionEnabled && st.penDown) penAddSegment(st, bx, by, sp.x, sp.y);

        st.log.info(idx, cmd, "Go mouse", "pos(" + to_string(sp.x) + "," + to_string(sp.y) + ")");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "BOUNCE_EDGE") {
        bool onEdge = (sp.x <= st.stageBounds.x + 0.5) ||
                      (sp.x >= st.stageBounds.x + st.stageBounds.w - 0.5) ||
                      (sp.y <= st.stageBounds.y + 0.5) ||
                      (sp.y >= st.stageBounds.y + st.stageBounds.h - 0.5);

        if (onEdge) {
            double before = sp.dirDeg;
            sp.dirDeg = fmod(180.0 - sp.dirDeg, 360.0);
            if (sp.dirDeg < 0) sp.dirDeg += 360.0;
            st.log.warn(idx, cmd, "Bounce", "dir:" + to_string(before) + "->" + to_string(sp.dirDeg));
        } else {
            st.log.info(idx, cmd, "Bounce", "not on edge");
        }
        sp.scriptPC++;
        return StepResult::Advanced;
    }


    if (cmd == "LAYER_FRONT") {
        int maxZ = sp.zOrder;
        for (const auto& s : st.sprites) if (s.zOrder > maxZ) maxZ = s.zOrder;
        sp.zOrder = maxZ + 1;
        st.log.info(idx, cmd, "Front layer", "z=" + to_string(sp.zOrder));
        sp.scriptPC++; return StepResult::Advanced;
    }
    if (cmd == "LAYER_BACK") {
        int minZ = sp.zOrder;
        for (const auto& s : st.sprites) if (s.zOrder < minZ) minZ = s.zOrder;
        sp.zOrder = minZ - 1;
        st.log.info(idx, cmd, "Back layer", "z=" + to_string(sp.zOrder));
        sp.scriptPC++; return StepResult::Advanced;
    }
    if (cmd == "LAYER_FWD") {
        sp.zOrder += (int)round(b.a);
        st.log.info(idx, cmd, "Forward layer", "z=" + to_string(sp.zOrder));
        sp.scriptPC++; return StepResult::Advanced;
    }
    if (cmd == "LAYER_BWD") {
        sp.zOrder -= (int)round(b.a);
        st.log.info(idx, cmd, "Backward layer", "z=" + to_string(sp.zOrder));
        sp.scriptPC++; return StepResult::Advanced;
    }
    auto setBubble = [&](bool think, const string& text, double seconds) {
        st.bubbleThink = think;
        st.bubbleText = text;
        if (seconds <= 0.0) st.bubbleUntilMs = 0;
        else st.bubbleUntilMs = SDL_GetTicks() + (uint32_t)max(0.0, seconds * 1000.0);
    };

    if (cmd == "SAY")      { setBubble(false, b.s1, 0.0); st.log.info(idx, cmd, "Say", "text=" + b.s1); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "SAY_T")    { setBubble(false, b.s1, b.a); st.log.info(idx, cmd, "Say for", "t=" + to_string(b.a) + " text=" + b.s1); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "THINK")    { setBubble(true,  b.s1, 0.0); st.log.info(idx, cmd, "Think", "text=" + b.s1); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "THINK_T")  { setBubble(true,  b.s1, b.a); st.log.info(idx, cmd, "Think for", "t=" + to_string(b.a) + " text=" + b.s1); sp.scriptPC++; return StepResult::Advanced; }

    if (cmd == "SHOW") { sp.visible = true;  st.log.info(idx, cmd, "Show", ""); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "HIDE") { sp.visible = false; st.log.info(idx, cmd, "Hide", ""); sp.scriptPC++; return StepResult::Advanced; }

    if (cmd == "SIZE_SET") {
        double before = sp.sizePct;
        sp.sizePct = clampT(b.a, 0.0, 300.0);
        st.log.info(idx, cmd, "Set size", "size:" + to_string(before) + "->" + to_string(sp.sizePct));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "SIZE_CHANGE") {
        double before = sp.sizePct;
        sp.sizePct = clampT(sp.sizePct + b.a, 0.0, 300.0);
        st.log.info(idx, cmd, "Change size", "size:" + to_string(before) + "->" + to_string(sp.sizePct));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "FX_COLOR_SET") {
        double before = sp.colorEffect;
        sp.colorEffect = fmod(b.a, 360.0);
        if (sp.colorEffect < 0) sp.colorEffect += 360.0;
        st.log.info(idx, cmd, "Set color effect", "h:" + to_string(before) + "->" + to_string(sp.colorEffect));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "FX_COLOR_CHANGE") {
        double before = sp.colorEffect;
        sp.colorEffect = fmod(sp.colorEffect + b.a, 360.0);
        if (sp.colorEffect < 0) sp.colorEffect += 360.0;
        st.log.info(idx, cmd, "Change color effect", "h:" + to_string(before) + "->" + to_string(sp.colorEffect));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "FX_CLEAR") {
        sp.colorEffect = 0.0;
        st.log.info(idx, cmd, "Clear effects", "");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "COSTUME_SET")  { sp.costumeIndex = (int)round(b.a); st.log.info(idx, cmd, "Set costume", "idx=" + to_string(sp.costumeIndex)); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "COSTUME_NEXT") { sp.costumeIndex++;                 st.log.info(idx, cmd, "Next costume", "idx=" + to_string(sp.costumeIndex)); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "BACKDROP_SET") { st.backdropIndex = (int)round(b.a); st.log.info(idx, cmd, "Set backdrop", "idx=" + to_string(st.backdropIndex)); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "BACKDROP_NEXT"){ st.backdropIndex++;                 st.log.info(idx, cmd, "Next backdrop", "idx=" + to_string(st.backdropIndex)); sp.scriptPC++; return StepResult::Advanced; }

    if (cmd == "SOUND_PLAY") {
        string name = b.s1.empty() ? "pop" : b.s1;
        string wav = name + ".wav";
        uint32_t ms = playWavOneShot(st, wav);
        st.soundBusyUntilMs = (ms > 0) ? (SDL_GetTicks() + ms) : 0;
        st.log.info(idx, cmd, "Play sound", "file=" + wav);
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_PLAY_UNTIL") {
        string name = b.s1.empty() ? "pop" : b.s1;
        string wav = name + ".wav";
        uint32_t now = SDL_GetTicks();

        if (st.soundBusyUntilMs != 0 && now < st.soundBusyUntilMs) {
            return StepResult::Yielded;
        }

        st.soundBusyUntilMs = 0;
        uint32_t ms = playWavOneShot(st, wav);
        st.soundBusyUntilMs = (ms > 0) ? (SDL_GetTicks() + ms) : 0;

        st.log.info(idx, cmd, "Play until done", "file=" + wav);

        if (st.soundBusyUntilMs != 0 && SDL_GetTicks() < st.soundBusyUntilMs) {
            return StepResult::Yielded;
        }

        st.soundBusyUntilMs = 0;
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_STOP_ALL") {
        if (st.audioReady && st.audioDev) SDL_ClearQueuedAudio(st.audioDev);
        st.soundBusyUntilMs = 0;
        st.log.info(idx, cmd, "Stop all sounds", "");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_SET_VOL") {
        int before = st.soundVolume;
        st.soundVolume = clampT((int)round(b.a), 0, 100);
        st.log.info(idx, cmd, "Set volume", "vol:" + to_string(before) + "->" + to_string(st.soundVolume));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SOUND_CHANGE_VOL") {
        int before = st.soundVolume;
        st.soundVolume = clampT((int)round(st.soundVolume + b.a), 0, 100);
        st.log.info(idx, cmd, "Change volume", "vol:" + to_string(before) + "->" + to_string(st.soundVolume));
        sp.scriptPC++;
        return StepResult::Advanced;
    }


    if (cmd == "TOUCH_SPRITE") {
        bool touching = false;
        string targetName = b.s1;
        int sizeA = (int)clampT((int)round(80.0 * (sp.sizePct / 100.0)), 10, 300);
        SDL_Rect rA{(int)round(sp.x) - sizeA/2, (int)round(sp.y) - sizeA/2, sizeA, sizeA};

        for (const auto& other : st.sprites) {
            if (&other == &sp) continue;
            if (other.name == targetName && other.visible) {
                int sizeB = (int)clampT((int)round(80.0 * (other.sizePct / 100.0)), 10, 300);
                SDL_Rect rB{(int)round(other.x) - sizeB/2, (int)round(other.y) - sizeB/2, sizeB, sizeB};

                if (!(rA.x + rA.w <= rB.x || rB.x + rB.w <= rA.x || rA.y + rA.h <= rB.y || rB.y + rB.h <= rA.y)) {
                    touching = true; break;
                }
            }
        }
        st.lastValue = Value::Num(touching ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Touch Sprite?", targetName + " -> " + st.lastValue.toString());
        sp.scriptPC++; return StepResult::Advanced;
    }
    if (cmd == "TOUCH_EDGE") {
        bool onEdge = (sp.x <= st.stageBounds.x + 0.5) ||
                      (sp.x >= st.stageBounds.x + st.stageBounds.w - 0.5) ||
                      (sp.y <= st.stageBounds.y + 0.5) ||
                      (sp.y >= st.stageBounds.y + st.stageBounds.h - 0.5);
        st.lastValue = Value::Num(onEdge ? 1.0 : 0.0);
        st.lastValue = Value::Num(onEdge ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Touch edge?", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "TOUCH_MOUSE") {
        double dx = sp.x - st.in.mx;
        double dy = sp.y - st.in.my;
        double dist = sqrt(dx*dx + dy*dy);
        bool touching = dist <= 10.0;
        st.lastValue = Value::Num(touching ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Touch mouse?", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "DIST_MOUSE") {
        double dx = sp.x - st.in.mx;
        double dy = sp.y - st.in.my;
        double dist = sqrt(dx*dx + dy*dy);
        st.lastValue = Value::Num(dist);
        st.log.info(idx, cmd, "Distance mouse", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "KEY_PRESSED") {
        int sc = b.i1;
        bool down = (sc >= 0 && sc < SDL_NUM_SCANCODES) ? st.in.keyDown[sc] : false;
        st.lastValue = Value::Num(down ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Key pressed?", "sc=" + to_string(sc) + " -> " + st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "MOUSE_DOWN") {
        st.lastValue = Value::Num(st.in.mouseDown ? 1.0 : 0.0);
        st.log.info(idx, cmd, "Mouse down?", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "MOUSE_X") {
        st.lastValue = Value::Num((double)st.in.mx);
        st.log.info(idx, cmd, "Mouse x", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "MOUSE_Y") {
        st.lastValue = Value::Num((double)st.in.my);
        st.log.info(idx, cmd, "Mouse y", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "ASK") {
        beginAskDialog(st, b.s1.empty() ? "?" : b.s1, idx + 1);
        return StepResult::Yielded;
    }
    if (cmd == "ANSWER") {
        st.lastValue = Value::Str(st.lastAnswer);
        st.log.info(idx, cmd, "Answer", "val=" + st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "TIMER") {
        double secs = (SDL_GetTicks() - st.timerStartMs) / 1000.0;
        st.lastValue = Value::Num(secs);
        st.log.info(idx, cmd, "Timer", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "RESET_TIMER") {
        st.timerStartMs = SDL_GetTicks();
        st.log.info(idx, cmd, "Reset timer", "0");
        sp.scriptPC++;
        return StepResult::Advanced;
    }


    if (cmd == "OP_ADD") { st.lastValue = Value::Num(b.a + b.b); st.log.info(idx, cmd, "Add", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_SUB") { st.lastValue = Value::Num(b.a - b.b); st.log.info(idx, cmd, "Sub", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_MUL") { st.lastValue = Value::Num(b.a * b.b); st.log.info(idx, cmd, "Mul", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }

    if (cmd == "DIV") {
        double out = 0.0;
        if (safeDiv(st, idx, b.a, b.b, out)) st.lastValue = Value::Num(out);
        st.log.info(idx, cmd, "Div", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "OP_EQ") { st.lastValue = Value::Num((b.a == b.b) ? 1.0 : 0.0); st.log.info(idx, cmd, "Eq", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_LT") { st.lastValue = Value::Num((b.a <  b.b) ? 1.0 : 0.0); st.log.info(idx, cmd, "Lt", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_GT") { st.lastValue = Value::Num((b.a >  b.b) ? 1.0 : 0.0); st.log.info(idx, cmd, "Gt", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }

    if (cmd == "OP_AND") { st.lastValue = Value::Num(((b.a != 0.0) && (b.b != 0.0)) ? 1.0 : 0.0); st.log.info(idx, cmd, "AND", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_OR")  { st.lastValue = Value::Num(((b.a != 0.0) || (b.b != 0.0)) ? 1.0 : 0.0); st.log.info(idx, cmd, "OR", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "OP_NOT") { st.lastValue = Value::Num((b.a == 0.0) ? 1.0 : 0.0); st.log.info(idx, cmd, "NOT", st.lastValue.toString()); sp.scriptPC++; return StepResult::Advanced; }

    if (cmd == "OP_STRLEN") {
        st.lastValue = Value::Num((double)b.s1.size());
        st.log.info(idx, cmd, "strlen", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "OP_LETTER") {
        int n = (int)round(b.a);
        if (n <= 0 || n > (int)b.s1.size()) st.lastValue = Value::Str("");
        else st.lastValue = Value::Str(string(1, b.s1[n-1]));
        st.log.info(idx, cmd, "letter", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "OP_JOIN") {
        st.lastValue = Value::Str(b.s1 + b.s2);
        st.log.info(idx, cmd, "join", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "FUNC_APPLY") {
        string fn = b.s1.empty() ? "sqrt" : b.s1;
        string inSel = b.inSel.empty() ? "last" : b.inSel;
        string outSel = b.outSel.empty() ? "last" : b.outSel;

        double x = funcReadInput(st, inSel);
        double y = 0.0;

        if (!funcApplyBuiltin(st, idx, fn, x, y)) {
            sp.scriptPC++;
            return StepResult::Advanced;
        }

        funcWriteOutput(st, outSel, y);

        st.log.info(idx, cmd, "Apply " + fn,
                    "in=" + inSel + " x=" + to_string(x) +
                    " -> out=" + outSel + " y=" + to_string(y));
        sp.scriptPC++;
        return StepResult::Advanced;
    }


    if (cmd == "DEFINE_FN") {

        int endIdx = (idx >= 0 && idx < (int)sp.jumpEnd.size()) ? sp.jumpEnd[idx] : -1;
        if (endIdx != -1) {
            st.log.info(idx, cmd, "Skip function definition", "jump to END_FN");
            sp.scriptPC = endIdx + 1;
        } else {
            sp.scriptPC++;
        }
        return StepResult::Advanced;
    }

    if (cmd == "CALL_FN") {
        string fnName = b.s1.empty() ? "myFunc" : b.s1;
        if (sp.funcDefs.count(fnName)) {
            sp.callStack.push_back(sp.scriptPC + 1);
            sp.currentParam = b.a;
            sp.scriptPC = sp.funcDefs[fnName] + 1;
            st.log.info(idx, cmd, "Call function", fnName + " arg=" + to_string(b.a));
        } else {
            st.log.warn(idx, cmd, "Function not found", fnName);
            sp.scriptPC++;
        }
        return StepResult::Advanced;
    }

    if (cmd == "END_FN") {
        if (!sp.callStack.empty()) {

            int retAddr = sp.callStack.back();
            sp.callStack.pop_back();
            sp.scriptPC = retAddr;
            st.log.info(idx, cmd, "Return from function", "to line " + to_string(retAddr));
        } else {
            sp.scriptPC++;
        }
        return StepResult::Advanced;
    }

    if (cmd == "GET_PARAM") {

        st.lastValue = Value::Num(sp.currentParam);
        st.log.info(idx, cmd, "Get param", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "VAR_SET_NUM") {
        string name = b.s1.empty() ? "v" : b.s1;
        Value before = st.vars.count(name) ? st.vars[name] : Value::Num(0);
        st.vars[name] = Value::Num(b.a);
        st.log.info(idx, cmd, "Set var", name + ":" + before.toString() + "->" + st.vars[name].toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_SET_STR") {
        string name = b.s1.empty() ? "msg" : b.s1;
        Value before = st.vars.count(name) ? st.vars[name] : Value::Str("");
        st.vars[name] = Value::Str(b.s2);
        st.log.info(idx, cmd, "Set var", name + ":" + before.toString() + "->" + st.vars[name].toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_CHANGE") {
        string name = b.s1.empty() ? "v" : b.s1;
        double before = st.vars.count(name) ? asNum(st.vars[name]) : 0.0;
        double after = before + b.a;
        st.vars[name] = Value::Num(after);
        st.log.info(idx, cmd, "Change var", name + ":" + to_string(before) + "->" + to_string(after));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_SHOW") {
        string name = b.s1.empty() ? "v" : b.s1;
        st.varVisible[name] = true;
        st.log.info(idx, cmd, "Show var", name);
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_HIDE") {
        string name = b.s1.empty() ? "v" : b.s1;
        st.varVisible[name] = false;
        st.log.info(idx, cmd, "Hide var", name);
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "VAR_GET") {
        string name = b.s1.empty() ? "v" : b.s1;
        if (st.vars.count(name)) st.lastValue = st.vars[name];
        else st.lastValue = Value::Num(0.0);
        st.log.info(idx, cmd, "Get var", name + " -> " + st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "LIST_ADD") {
        string name = b.s1.empty() ? "list" : b.s1;
        Value item = listItemFromBlock(b);
        getList(st, name).push_back(item);
        st.log.info(idx, cmd, "Add to list", name + " <- " + item.toString());
        sp.scriptPC++;
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
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_CLEAR") {
        string name = b.s1.empty() ? "list" : b.s1;
        getList(st, name).clear();
        st.log.info(idx, cmd, "Clear list", name);
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_LENGTH") {
        string name = b.s1.empty() ? "list" : b.s1;
        st.lastValue = Value::Num((double)getList(st, name).size());
        st.log.info(idx, cmd, "List length", name + " -> " + st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_ITEM") {
        string name = b.s1.empty() ? "list" : b.s1;
        auto& L = getList(st, name);
        int i1 = (int)round(b.a);
        if (listIndexOk1(i1, (int)L.size())) st.lastValue = L[i1 - 1];
        else st.lastValue = Value::Str("");
        st.log.info(idx, cmd, "List item", name + "[" + to_string(i1) + "] -> " + st.lastValue.toString());
        sp.scriptPC++;
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
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_SHOW") {
        string name = b.s1.empty() ? "list" : b.s1;
        st.listVisible[name] = true;
        st.log.info(idx, cmd, "Show list", name);
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "LIST_HIDE") {
        string name = b.s1.empty() ? "list" : b.s1;
        st.listVisible[name] = false;
        st.log.info(idx, cmd, "Hide list", name);
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "CLONE_CREATE") {
        cloneCreateFromActor(st);
        st.log.info(idx, cmd, "Create clone", "count=" + to_string((int)st.clones.size()));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "CLONE_DELETE_LAST") {
        int before = (int)st.clones.size();
        cloneDeleteLast(st);
        st.log.info(idx, cmd, "Delete last clone", "count:" + to_string(before) + "->" + to_string((int)st.clones.size()));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "CLONE_CLEAR") {
        cloneClearAll(st);
        st.log.info(idx, cmd, "Clear clones", "");
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "CLONE_COUNT") {
        st.lastValue = Value::Num((double)st.clones.size());
        st.log.info(idx, cmd, "Clone count", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "WAIT") {
        int ms = (int)max(0.0, b.a * 1000.0);
        sp.waiting = true;
        sp.waitUntilMs = SDL_GetTicks() + (uint32_t)ms;
        st.log.info(idx, cmd, "Wait", "ms=" + to_string(ms));
        sp.scriptPC++;
        return StepResult::Yielded;
    }

    if (cmd == "STOP_ALL") {
        st.log.warn(idx, cmd, "Stop all scripts", "stop");
        stopScript(st,sp, "STOP_ALL");
        return StepResult::Stopped;
    }

    if (cmd == "WAIT_UNTIL") {
        if (!st.lastValue.truthy()) {
            st.log.info(idx, cmd, "Wait until", "blocked (last=false)");
            return StepResult::Yielded;
        }
        st.log.info(idx, cmd, "Wait until", "pass");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "IF" || cmd == "IFELSE") {
        bool cond = st.lastValue.truthy();
        int endIdx  = (idx >= 0 && idx < (int)sp.jumpEnd.size())  ? sp.jumpEnd[idx]  : -1;
        int elseIdx = (idx >= 0 && idx < (int)sp.jumpElse.size()) ? sp.jumpElse[idx] : -1;

        if (!cond) {
            if (cmd == "IFELSE" && elseIdx != -1) {
                st.log.info(idx, cmd, "IF false", "jump to ELSE");
                sp.scriptPC = elseIdx + 1;
                return StepResult::Advanced;
            }
            if (endIdx != -1) {
                st.log.info(idx, cmd, "IF false", "jump to END_IF");
                sp.scriptPC = endIdx + 1;
                return StepResult::Advanced;
            }
        }

        st.log.info(idx, cmd, "IF true", "enter");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "ELSE") {
        int endIdx = (idx >= 0 && idx < (int)sp.jumpTo.size()) ? sp.jumpTo[idx] : -1;
        if (endIdx != -1) {
            st.log.info(idx, cmd, "ELSE", "jump END_IF");
            sp.scriptPC = endIdx + 1;
            return StepResult::Advanced;
        }
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "END_IF") {
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "REPEAT") {
        int endIdx = (idx >= 0 && idx < (int)sp.loopEnd.size()) ? sp.loopEnd[idx] : -1;
        int count = clampT((int)round(b.a), 0, 1000000);

        if (sp.repeatCounter[idx] == 0) sp.repeatCounter[idx] = count;

        if (count == 0 || sp.repeatCounter[idx] <= 0) {
            sp.repeatCounter[idx] = 0;
            if (endIdx != -1) {
                st.log.info(idx, cmd, "Repeat skip", "count=0");
                sp.scriptPC = endIdx + 1;
                return StepResult::Advanced;
            }
        }

        st.log.info(idx, cmd, "Repeat enter", "left=" + to_string(sp.repeatCounter[idx]));
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "REPEAT_UNTIL") {
        int endIdx = (idx >= 0 && idx < (int)sp.loopEnd.size()) ? sp.loopEnd[idx] : -1;
        if (st.lastValue.truthy()) {
            st.log.info(idx, cmd, "RepeatUntil", "cond true -> exit");
            sp.scriptPC = (endIdx != -1) ? (endIdx + 1) : (idx + 1);
            return StepResult::Advanced;
        }
        st.log.info(idx, cmd, "RepeatUntil", "cond false -> enter");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "END_REPEAT") {
        int startIdx = (idx >= 0 && idx < (int)sp.loopStart.size()) ? sp.loopStart[idx] : -1;
        if (startIdx != -1) {
            string startCmd = sp.ws.blocks[startIdx].cmd;
            if (startCmd == "REPEAT") {
                sp.repeatCounter[startIdx] = max(0, sp.repeatCounter[startIdx] - 1);
                if (sp.repeatCounter[startIdx] > 0) {
                    st.log.info(idx, cmd, "Repeat loop", "back to start");
                    sp.scriptPC = startIdx + 1;
                    return StepResult::Advanced;
                }
                sp.repeatCounter[startIdx] = 0;
                st.log.info(idx, cmd, "Repeat end", "done");
                sp.scriptPC++;
                return StepResult::Advanced;
            }
            if (startCmd == "REPEAT_UNTIL") {
                st.log.info(idx, cmd, "RepeatUntil loop", "back to start");
                sp.scriptPC = startIdx;
                return StepResult::Advanced;
            }
        }
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "FOREVER") {
        st.log.info(idx, cmd, "Forever enter", "");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "END_FOREVER") {
        int startIdx = (idx >= 0 && idx < (int)sp.loopStart.size()) ? sp.loopStart[idx] : -1;
        if (startIdx != -1) {
            st.log.warn(idx, cmd, "Forever loop", "back to start");
            sp.scriptPC = startIdx + 1;
            return StepResult::Advanced;
        }
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "SQRT") {
        double out = 0.0;
        if (safeSqrt(st, idx, b.a, out)) st.lastValue = Value::Num(out);
        st.log.info(idx, cmd, "Sqrt", st.lastValue.toString());
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "LOOP") {
        st.log.warn(idx, cmd, "Infinite loop block", "pc stays same");
        return StepResult::Yielded; // keep yielding so UI doesn't watchdog
    }


    if (isPenCmd(cmd) && !st.penExtensionEnabled) {
        st.log.warn(idx, cmd, "Pen extension not enabled", "skipped");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    if (cmd == "PEN_DOWN") { st.penDown = true;  st.log.info(idx, cmd, "Pen down", ""); sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "PEN_UP")   { st.penDown = false; st.log.info(idx, cmd, "Pen up", "");   sp.scriptPC++; return StepResult::Advanced; }
    if (cmd == "PEN_ERASE_ALL") {
        penClearAll(st);
        st.penDown = false;
        st.log.info(idx, cmd, "All erase", "cleared");
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "PEN_STAMP") { penAddStamp(st); st.log.info(idx, cmd, "Stamp", "count=" + to_string((int)st.penStamps.size())); sp.scriptPC++; return StepResult::Advanced; }

    if (cmd == "PEN_SET_SIZE") {
        int before = st.penSize;
        st.penSize = clampT((int)round(b.a), 1, 30);
        st.log.info(idx, cmd, "Set size", "size:" + to_string(before) + "->" + to_string(st.penSize));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "PEN_CHANGE_SIZE") {
        int before = st.penSize;
        st.penSize = clampT((int)round(st.penSize + b.a), 1, 30);
        st.log.info(idx, cmd, "Change size", "size:" + to_string(before) + "->" + to_string(st.penSize));
        sp.scriptPC++;
        return StepResult::Advanced;
    }
    if (cmd == "PEN_SET_COLOR") {
        SDL_Color c = b.pickColor;
        rgbToHsv(c, st.penHue, st.penSat, st.penBri);
        penSyncRGB(st);
        st.log.info(idx, cmd, "Set color (direct)",
                    "rgb=(" + to_string((int)c.r) + "," + to_string((int)c.g) + "," + to_string((int)c.b) + ")");
        sp.scriptPC++;
        return StepResult::Advanced;
    }

    st.log.warn(idx, cmd, "Unknown cmd skipped", "");
    sp.scriptPC++;
    return StepResult::Advanced;
}

static void runScriptTick(AppState& st) {
    if (st.isPaused) return;
    uint32_t now = SDL_GetTicks();
    if (st.runSpeedMs > 0 && now < st.nextStepAtMs) return;

    bool anyRan = false;
    for (auto& sp : st.sprites) {
        if (!sp.scriptRunning) continue;
        executeOneBlock(st, sp);
        anyRan = true;
    }

    if (anyRan && st.runSpeedMs > 0) st.nextStepAtMs = SDL_GetTicks() + st.runSpeedMs;
}
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
        st.getActive().ws.reset();
        st.penDown = false;
        penClearAll(st);
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
        st.getActive().ws.addBlock(st.getActive().ws.bounds.x + 60, st.getActive().ws.bounds.y + 60);
        if (!st.getActive().ws.blocks.empty()) {
            Block& b = st.getActive().ws.blocks.back();
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

    st.buttons.push_back(mkBtn(x, 70, "Run", "F5", [&] {
        st.isPaused = false;
        startScript(st);
    }));
    x += 80;

    st.buttons.push_back(mkBtn(x, 75, "Pause", "F7", [&] {
        st.isPaused = true;
        st.log.log("RUN", "Paused");
    }));
    x += 85;

    st.buttons.push_back(mkBtn(x, 85, "Resume", "F8", [&] {
        st.isPaused = false;
        st.log.log("RUN", "Resumed");
    }));
    x += 95;

    st.buttons.push_back(mkBtn(x, 70, "Stop", "F6", [&] {
        st.isPaused = false;
        for (auto& s : st.sprites) stopScript(st, s, "User stop (F6)");
    }));
    x += 80;

    st.buttons.push_back(mkBtn(x, 70, "Quit", "Esc", [&] {
        st.quit = true;
    }));
}
