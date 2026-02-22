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