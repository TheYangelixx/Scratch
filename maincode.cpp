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


    string cmd = "MOVE";   // extended commands in section 4
    double a = 40.0;       // numeric param A
    double b = 0.0;        // numeric param B
    string s1 = "";        // string param
    string s2 = "";        // string param 2
    int i1 = 0;            // int param


    string opt = "";                 // dropdown: "COLOR", "SAT", "BRI"
    SDL_Color pickColor = {0, 255, 0, 255}; // for PEN_SET_COLOR block


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



    Sprite& getActive() {
        return sprites[activeSprite];
    }


    const Sprite& getActive() const {
        return sprites[activeSprite];
    }

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
        } else {

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

        if (pointInRect(mx,my,rowCostPrev)) {
            if (!st.costumes.empty()) {
                st.getActive().costumeIndex--;
                if (st.getActive().costumeIndex < 0) st.getActive().costumeIndex = (int)st.costumes.size() - 1;
            }
            st.log.info(-1, "SET", "Costume prev", "idx=" + to_string(st.getActive().costumeIndex));
            return true;
        }
        if (pointInRect(mx,my,rowCostNext)) {
            if (!st.costumes.empty()) {
                st.getActive().costumeIndex = (st.getActive().costumeIndex + 1) % (int)st.costumes.size();
            }
            st.log.info(-1, "SET", "Costume next", "idx=" + to_string(st.getActive().costumeIndex));
            return true;
        }


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


        if (pointInRect(mx,my,rowMusicMid)) {
            int rel = mx - rowMusicMid.x;
            int v = (int)llround((rel / (double)max(1, rowMusicMid.w)) * 100.0);
            st.musicVolume = clampT(v, 0, 100);
            st.log.info(-1, "SET", "Music volume set", "musicVolume=" + to_string(st.musicVolume));
            return true;
        }


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

static bool saveProjectNamed(const string& saveStem, AppState& st){
    const string path = buildSavePath(saveStem);
    ofstream f(path.c_str());
    if (!f) {
        st.log.log("SAVE", "Cannot open: " + path);
        return false;
    }


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


    f << "PEN_STATE "
      << (st.penDown ? 1 : 0) << " "
      << st.penHue << " " << st.penSat << " " << st.penBri << " "
      << st.penSize << "\n";

    f << "PEN_SEGS " << st.penSegs.size() << "\n";
    for (const auto& seg : st.penSegs) {
        f << "SEG "
          << seg.x1 << " " << seg.y1 << " " << seg.x2 << " " << seg.y2 << " "
          << (int)seg.c.r << " " << (int)seg.c.g << " " << (int)seg.c.b << " "
          << seg.size << "\n";
    }

    f << "PEN_STAMPS " << st.penStamps.size() << "\n";
    for (const auto& sp : st.penStamps) {
        f << "STAMP "
          << sp.x << " " << sp.y << " "
          << sp.costumeIndex << " "
          << sp.dirDeg << " "
          << sp.sizePct
          << "\n";
    }



    f << "VARS " << st.vars.size() << "\n";
    for (const auto& kv : st.vars) {
        const string& name = kv.first;
        const Value& v = kv.second;
        f << "VAR " << name << " " << (v.isNum ? 1 : 0) << " ";
        if (v.isNum) f << v.num << " " << std::quoted("") << "\n";
        else         f << 0.0  << " " << std::quoted(v.str) << "\n";
    }

    f << "VARVIS " << st.varVisible.size() << "\n";
    for (const auto& kv : st.varVisible) {
        f << "VARV " << kv.first << " " << (kv.second ? 1 : 0) << "\n";
    }


    f << "LISTS " << st.lists.size() << "\n";
    for (const auto& kv : st.lists) {
        const string& name = kv.first;
        const auto& items = kv.second;
        f << "LIST " << name << " " << items.size() << "\n";
        for (const auto& it : items) {
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
    if (!f) {
        st.log.log("LOAD", "Cannot open: " + path);
        return false;
    }


    st.getActive().ws.reset();
    st.penSegs.clear();
    st.penStamps.clear();
    st.vars.clear();
    st.varVisible.clear();
    st.lists.clear();
    st.listVisible.clear();

    string header;
    if (!getline(f, header)) return false;
    if (header != "SAVE_V1") {
        st.log.log("LOAD", "Invalid save header: " + header);
        return false;
    }

    string line;
    int maxId = 0;

    size_t expectBlocks = 0;
    size_t expectPenSegs = 0, expectPenStamps = 0;
    size_t expectVars = 0, expectVarVis = 0;
    size_t expectLists = 0, expectListVis = 0;

    while (getline(f, line)) {
        if (line.empty()) continue;

        stringstream ss(line);
        string tag;
        ss >> tag;

        if (tag == "BLOCKS") {
            ss >> expectBlocks;
            continue;
        }

        if (tag == "BLOCK") {
            Block b;
            int r=80,g=80,bl=90;
            int pr=0,pg=255,pb=0;

            ss >> b.id
               >> b.rect.x >> b.rect.y >> b.rect.w >> b.rect.h
               >> r >> g >> bl
               >> b.cmd
               >> b.a >> b.b
               >> b.i1
               >> pr >> pg >> pb
               >> b.inSel >> b.outSel
               >> std::quoted(b.s1) >> std::quoted(b.s2);

            b.color = SDL_Color{(Uint8)r,(Uint8)g,(Uint8)bl,255};
            b.pickColor = SDL_Color{(Uint8)pr,(Uint8)pg,(Uint8)pb,255};

            setBlockVisual(b);

            maxId = max(maxId, b.id);
            st.getActive().ws.blocks.push_back(b);
            continue;
        }

        if (tag == "SETTINGS") {
            int drawFlag = 1;
            ss >> st.runSpeedMs >> drawFlag;
            st.drawActorWhenStopped = (drawFlag != 0);
            continue;
        }

        if (tag == "LOOKS") {
            ss >> st.getActive().costumeIndex >> st.backdropIndex >> st.getActive().colorEffect;
            continue;
        }

        if (tag == "EXT_PEN") {
            int v=0; ss >> v;
            st.penExtensionEnabled = (v != 0);
            continue;
        }

        if (tag == "PEN_STATE") {
            int down=0;
            ss >> down >> st.penHue >> st.penSat >> st.penBri >> st.penSize;
            st.penDown = (down != 0);
            penSyncRGB(st);
            continue;
        }

        if (tag == "PEN_SEGS") { ss >> expectPenSegs; continue; }
        if (tag == "SEG") {
            PenSegment seg;
            int cr=0,cg=255,cb=0;
            ss >> seg.x1 >> seg.y1 >> seg.x2 >> seg.y2 >> cr >> cg >> cb >> seg.size;
            seg.c = SDL_Color{(Uint8)cr,(Uint8)cg,(Uint8)cb,255};
            st.penSegs.push_back(seg);
            continue;
        }

        if (tag == "PEN_STAMPS") { ss >> expectPenStamps; continue; }
        if (tag == "STAMP") {
            PenStamp sp;
            ss >> sp.x >> sp.y >> sp.costumeIndex >> sp.dirDeg >> sp.sizePct;
            st.penStamps.push_back(sp);
            continue;
        }


        if (tag == "VARS") { ss >> expectVars; continue; }
        if (tag == "VAR") {
            string name; int isNum=1; double num=0.0; string str;
            ss >> name >> isNum >> num >> std::quoted(str);
            if (isNum) st.vars[name] = Value::Num(num);
            else       st.vars[name] = Value::Str(str);
            continue;
        }

        if (tag == "VARVIS") { ss >> expectVarVis; continue; }
        if (tag == "VARV") {
            string name; int v=0;
            ss >> name >> v;
            st.varVisible[name] = (v!=0);
            continue;
        }

        if (tag == "LISTS") { ss >> expectLists; continue; }
        if (tag == "LIST") {
            string name; size_t n=0;
            ss >> name >> n;
            auto& L = st.lists[name];
            L.clear();

            for (size_t i = 0; i < n; i++) {
                string itemLine;
                if (!getline(f, itemLine)) break;
                stringstream is(itemLine);
                string itag; is >> itag;
                if (itag != "ITEM") break;

                int isNum=1; double num=0.0; string s;
                is >> isNum >> num >> std::quoted(s);
                if (isNum) L.push_back(Value::Num(num));
                else       L.push_back(Value::Str(s));
            }
            continue;
        }

        if (tag == "LISTVIS") { ss >> expectListVis; continue; }
        if (tag == "LISTV") {
            string name; int v=0;
            ss >> name >> v;
            st.listVisible[name] = (v!=0);
            continue;
        }
    }

    st.getActive().ws.nextId = maxId + 1;
    st.paletteDirty = true; // because pen enabled affects palette
    st.log.log("LOAD", "Loaded: " + path);
    return true;
}
