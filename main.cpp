#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>

// --- Constants & Configuration ---
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 800;

// Colors
struct Color { Uint8 r, g, b, a; };
const Color COL_BLOCK_PALETTE = {249, 249, 249, 255};
const Color COL_CATEGORY_BAR = {255, 255, 255, 255};
const Color COL_SPRITE_PANE = {230, 240, 255, 255};
const Color COL_STAGE_BORDER = {200, 200, 200, 255};
const Color COL_TEXT = {87, 94, 117, 255};

// Category Colors
const Color COL_CAT_MOTION = {76, 151, 255, 255};
const Color COL_CAT_LOOKS = {153, 102, 255, 255};
const Color COL_CAT_SOUND = {207, 99, 207, 255};
const Color COL_CAT_EVENTS = {255, 191, 0, 255};

// --- Data Structures ---

struct Layout {
    SDL_Rect categoryRect;
    SDL_Rect paletteRect;
    SDL_Rect workspaceRect;
    SDL_Rect stageAreaRect;
    SDL_Rect stageRect;
    SDL_Rect spritePaneRect;
    SDL_Rect headerRect;
};

struct Sprite {
    std::string name;
    double x, y;
    double direction;
    double size;
    bool isVisible;
    bool isDraggable;
    SDL_Texture* texture; // This is the member causing error if struct is broken

    // Init function
    void init(std::string n, SDL_Renderer* renderer, const char* path) {
        name = n;
        x = 0; y = 0;
        direction = 90.0;
        size = 100.0;
        isVisible = true;
        isDraggable = true;

        SDL_Surface* surface = IMG_Load(path);
        if (surface) {
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        } else {
            texture = nullptr;
        }
    }
};

enum BlockType { MOTION, LOOKS, EVENTS, CONTROL };

struct Block {
    BlockType type;
    std::string text;
    SDL_Rect rect;
    bool isDragging;
    Color color;
};

struct AppState {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    Layout layout;
    std::vector<Sprite> sprites;
    int activeSpriteIndex;
    bool isRunning; // This is the member causing "No member named isRunning"

    std::vector<Block> workspaceBlocks;
    std::vector<Block> paletteBlocks;
};

// --- Helper Functions ---

void drawRectFilled(SDL_Renderer* r, SDL_Rect rect, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}

void drawRectOutline(SDL_Renderer* r, SDL_Rect rect, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(r, &rect);
}

void drawCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, centerX + dx, centerY + dy);
            }
        }
    }
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, std::string text, int x, int y, Color c) {
    if (!font) return;
    SDL_Color textColor = {c.r, c.g, c.b, c.a};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), textColor);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect dest = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
}

void updateLayout(AppState& app, int w, int h) {
    app.layout.headerRect = {0, 0, w, 45};
    int mainY = 45;
    int mainH = h - 45;

    app.layout.categoryRect = {0, mainY, 60, mainH};
    app.layout.paletteRect = {60, mainY, 300, mainH};

    int stageW = 480;
    int stageH = 360;
    int rightPanelX = w - stageW - 20;

    app.layout.stageAreaRect = {rightPanelX, mainY, stageW + 20, mainH};
    app.layout.stageRect = {rightPanelX + 10, mainY + 50, stageW, stageH};

    int spritePaneY = app.layout.stageRect.y + stageH + 10;
    app.layout.spritePaneRect = {rightPanelX + 10, spritePaneY, stageW, mainH - (spritePaneY - mainY) - 10};

    app.layout.workspaceRect = {
            360, mainY,
            rightPanelX - 360, mainH
    };
}

void drawUI(AppState& app) {
    SDL_Renderer* r = app.renderer;

    // Clear
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderClear(r);

    // Header
    drawRectFilled(r, app.layout.headerRect, {76, 151, 255, 255});
    renderText(r, app.font, "Scratch Clone", 20, 10, {255, 255, 255, 255});

    // Categories
    drawRectFilled(r, app.layout.categoryRect, COL_CATEGORY_BAR);
    drawCircle(r, 30, app.layout.categoryRect.y + 30, 10, COL_CAT_MOTION);
    drawCircle(r, 30, app.layout.categoryRect.y + 70, 10, COL_CAT_LOOKS);
    drawCircle(r, 30, app.layout.categoryRect.y + 110, 10, COL_CAT_SOUND);
    drawCircle(r, 30, app.layout.categoryRect.y + 150, 10, COL_CAT_EVENTS);

    // Palette
    drawRectFilled(r, app.layout.paletteRect, COL_BLOCK_PALETTE);
    drawRectOutline(r, app.layout.paletteRect, {230, 230, 230, 255});

    SDL_Rect blockMove = {70, app.layout.paletteRect.y + 20, 150, 40};
    drawRectFilled(r, blockMove, COL_CAT_MOTION);
    renderText(r, app.font, "move 10 steps", 80, app.layout.paletteRect.y + 30, {255, 255, 255, 255});

    SDL_Rect blockTurn = {70, app.layout.paletteRect.y + 70, 150, 40};
    drawRectFilled(r, blockTurn, COL_CAT_MOTION);
    renderText(r, app.font, "turn 15 degrees", 80, app.layout.paletteRect.y + 80, {255, 255, 255, 255});

    // Workspace
    drawRectFilled(r, app.layout.workspaceRect, {255, 255, 255, 255});
    SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
    for(int i = app.layout.workspaceRect.x; i < app.layout.workspaceRect.x + app.layout.workspaceRect.w; i+=40) {
        for(int j = app.layout.workspaceRect.y; j < WINDOW_HEIGHT; j+=40) {
            SDL_RenderDrawPoint(r, i, j);
        }
    }

    // Stage
    drawRectFilled(r, app.layout.stageRect, {255, 255, 255, 255});
    drawRectOutline(r, app.layout.stageRect, COL_STAGE_BORDER);

    // Draw Sprite
    if (!app.sprites.empty()) {
        Sprite& s = app.sprites[app.activeSpriteIndex];
        int stageCenterX = app.layout.stageRect.x + app.layout.stageRect.w / 2;
        int stageCenterY = app.layout.stageRect.y + app.layout.stageRect.h / 2;

        SDL_Rect dest = {
                (int)(stageCenterX + s.x - 25),
                (int)(stageCenterY - s.y - 25),
                50, 50
        };

        if (s.texture) {
            SDL_RenderCopyEx(r, s.texture, nullptr, &dest, s.direction - 90, nullptr, SDL_FLIP_NONE);
        } else {
            drawRectFilled(r, dest, {255, 140, 0, 255});
        }
    }

    // Sprite Pane
    drawRectFilled(r, app.layout.spritePaneRect, COL_SPRITE_PANE);
    drawRectOutline(r, app.layout.spritePaneRect, COL_STAGE_BORDER);

    // Info
    int infoX = app.layout.spritePaneRect.x + 10;
    int infoY = app.layout.spritePaneRect.y + 10;

    if (!app.sprites.empty()) {
        Sprite& s = app.sprites[app.activeSpriteIndex];
        renderText(r, app.font, s.name, infoX, infoY, COL_TEXT);
    }

    // Icon
    SDL_Rect iconRect = {app.layout.spritePaneRect.x + 20, app.layout.spritePaneRect.y + 80, 60, 60};
    drawRectFilled(r, iconRect, {255, 255, 255, 255});
    drawRectOutline(r, iconRect, {100, 100, 255, 255});

    SDL_RenderPresent(r);
}

// --- Main ---

int main(int argc, char* argv[]) {
    AppState app;
    app.isRunning = true;
    app.activeSpriteIndex = 0;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    if (TTF_Init() == -1) return -1;
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return -1;

    app.window = SDL_CreateWindow("Scratch Clone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED);

    app.font = TTF_OpenFont("arial.ttf", 16);
    if (!app.font) {
        std::cout << "Font warning (ignore if file missing)\n";
    }

    updateLayout(app, WINDOW_WIDTH, WINDOW_HEIGHT);

    Sprite cat;
    cat.init("Sprite1", app.renderer, "cat.png");
    app.sprites.push_back(cat);

    SDL_Event e;
    while (app.isRunning) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                app.isRunning = false;
            }
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                updateLayout(app, e.window.data1, e.window.data2);
            }
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RIGHT) app.sprites[0].x += 10;
                if (e.key.keysym.sym == SDLK_LEFT) app.sprites[0].x -= 10;
                if (e.key.keysym.sym == SDLK_UP) app.sprites[0].y += 10;
                if (e.key.keysym.sym == SDLK_DOWN) app.sprites[0].y -= 10;
            }
        }

        drawUI(app);
    }

    if (!app.sprites.empty() && app.sprites[0].texture) {
        SDL_DestroyTexture(app.sprites[0].texture);
    }
    if (app.font) TTF_CloseFont(app.font);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
