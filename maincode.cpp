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
static const int LEFT_PANEL_W = 180;

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
// Logger
// =========================
struct Logger {
    ofstream out;
    uint64_t cycle = 0;

    explicit Logger(const string& path) {
        out.open(path.c_str(), ios::app);
        if (!out) cerr << "Failed to open log file: " << path << "\n";
    }

    void log(const string& tag, const string& msg) {
        if (!out) return;
        out << "[Cycle:" << cycle << "] [" << tag << "] " << msg << "\n";
        out.flush();
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

    // fallback: maybe user put a font next to exe
    TTF_Font* f2 = TTF_OpenFont("font.ttf", pt);
    if (f2) return f2;

    return nullptr;
}

// =========================
// UI Button
// =========================
struct Button {
    SDL_Rect rect{};
    string text;      // main label
    string sub;       // shortcut label
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
        b.rect = SDL_Rect{x, y, 220, 48};
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
                clampIntoBounds(b);
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

    // strip ".txt" if user typed it
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
        string name = fd.cFileName; // e.g. abc.txttttttttttt
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

    // Human-readable format
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
// App State
// =========================
struct AppState {
    bool quit = false;

    InputState in;
    Logger log = Logger("log.txt");

    Workspace ws;
    vector<Button> buttons;

    string baseTitle = "YKP Base (SDL2)";

    // Font
    TTF_Font* uiFont = nullptr;

    // Save/Load dialog state
    bool saveDialogOpen = false;
    bool loadDialogOpen = false;

    string saveNameInput = "";
    vector<string> saveList;
    int loadHoverIndex = -1;
    int loadScroll = 0;
};

// =========================
// Dialog helpers
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
            return true; // consume clicks while modal is open
        }

        return true; // consume everything while modal is open
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
            // scroll list
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

            // click outside closes? (like many apps)
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

        return true; // consume everything while modal is open
    }

    return false;
}

static void renderDialogs(const AppState& st, SDL_Renderer* r, int winW, int winH) {
    if (!st.saveDialogOpen && !st.loadDialogOpen) return;

    // overlay dim
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, winW, winH};
    SDL_RenderFillRect(r, &full);

    SDL_Color white = {240, 240, 240, 255};

    if (st.saveDialogOpen) {
        SDL_Rect modal = {winW/2 - 220, winH/2 - 90, 440, 180};
        SDL_SetRenderDrawColor(r, 40, 40, 46, 255);
        SDL_RenderFillRect(r, &modal);
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderDrawRect(r, &modal);

        renderText(r, st.uiFont, "Save Project", modal.x + 20, modal.y + 14, white);

        // input field
        SDL_Rect field = {modal.x + 20, modal.y + 55, modal.w - 40, 40};
        SDL_SetRenderDrawColor(r, 25, 25, 28, 255);
        SDL_RenderFillRect(r, &field);
        SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
        SDL_RenderDrawRect(r, &field);

        string shown = st.saveNameInput.empty() ? "type a name..." : st.saveNameInput;
        renderText(r, st.uiFont, shown, field.x + 10, field.y + 8, white);

        // buttons
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
        st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 40);
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
        st.log.log("ADD", "Added block");
    }));
    x += 120;

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
        // update mouse coords early
        if (e.type == SDL_MOUSEMOTION) {
            st.in.mx = e.motion.x;
            st.in.my = e.motion.y;
        }

        // if modal open -> consume via dialog handler
        if (st.saveDialogOpen || st.loadDialogOpen) {
            if (handleDialogsEvent(st, e, winW, winH)) continue;
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
    bool ctrl = st.in.keyDown[SDL_SCANCODE_LCTRL] || st.in.keyDown[SDL_SCANCODE_RCTRL];

    if (ctrl && st.in.keyPressed[SDL_SCANCODE_N]) {
        st.ws.reset();
        st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 40);
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

    st.ws.bounds = SDL_Rect{LEFT_PANEL_W, TOP_BAR_H, w - LEFT_PANEL_W, h - TOP_BAR_H};

    // If modal open: only update hover for load list (no interaction behind modal)
    if (st.loadDialogOpen) updateLoadHover(st, w, h);
    if (st.saveDialogOpen || st.loadDialogOpen) return;

    for (size_t i = 0; i < st.buttons.size(); i++) st.buttons[i].update(st.in);

    handleShortcuts(st);

    st.log.cycle++;
    st.ws.update(st.in, st.log);
}

static void render(const AppState& st, SDL_Renderer* r, SDL_Window* window) {
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);

    SDL_SetRenderDrawColor(r, 30, 30, 34, 255);
    SDL_RenderClear(r);

    SDL_Rect top = {0, 0, w, TOP_BAR_H};
    SDL_SetRenderDrawColor(r, 20, 20, 22, 255);
    SDL_RenderFillRect(r, &top);

    SDL_Rect left = {0, TOP_BAR_H, LEFT_PANEL_W, h - TOP_BAR_H};
    SDL_SetRenderDrawColor(r, 24, 24, 26, 255);
    SDL_RenderFillRect(r, &left);
    SDL_SetRenderDrawColor(r, 12, 12, 12, 255);
    SDL_RenderDrawRect(r, &left);

    for (size_t i = 0; i < st.buttons.size(); i++) st.buttons[i].draw(r, st.uiFont);

    SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
    SDL_RenderDrawRect(r, &st.ws.bounds);

    st.ws.draw(r);

    // dialogs (overlay)
    renderDialogs(st, r, w, h);

    SDL_RenderPresent(r);
}

// =========================
// Run
// =========================
static int RunApp() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
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

    st.ws.bounds = SDL_Rect{LEFT_PANEL_W, TOP_BAR_H, WINDOW_W - LEFT_PANEL_W, WINDOW_H - TOP_BAR_H};
    st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 40);
    st.ws.addBlock(st.ws.bounds.x + 40, st.ws.bounds.y + 120);

    setupUI(st);

    while (!st.quit) {
        st.in.beginFrame();
        processEvents(st, window);
        update(st, window);
        render(st, renderer, window);
        SDL_Delay(1);
    }

    SDL_StopTextInput();
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
