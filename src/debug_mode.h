//#ifndef DEBUG_MODE_H
//#define DEBUG_MODE_H
//
//#include "sprite.h"
//#include "logger.h"
//#include <SDL2/SDL.h>
//#include <SDL2/SDL_ttf.h>
//#include <string>
//#include <vector>
//
///* وضعیت دیباگ */
//struct DebugState {
//    bool is_debug_mode;
//    bool waiting_for_step;
//    bool step_requested;
//    int current_block_id;
//    int current_sprite_id;
//    int current_line;
//
//    std::vector<std::string> watch_variables;
//    std::vector<int> breakpoint_block_ids;
//};
//
///* توابع دیباگ */
//void debug_init(DebugState& ds);
//void debug_toggle(DebugState& ds);
//void debug_enable(DebugState& ds);
//void debug_disable(DebugState& ds);
//void debug_request_step(DebugState& ds);
//bool debug_should_pause(const DebugState& ds);
//void debug_set_current(DebugState& ds, int sprite_id, int block_id, int line);
//
///* Breakpoints */
//void debug_add_breakpoint(DebugState& ds, int block_id);
//void debug_remove_breakpoint(DebugState& ds, int block_id);
//bool debug_is_breakpoint(const DebugState& ds, int block_id);
//void debug_clear_breakpoints(DebugState& ds);
//
///* رندر */
//void debug_render_overlay(SDL_Renderer* renderer, TTF_Font* font,
//                          const DebugState& ds, const Sprite& sprite,
//                          int screen_x, int screen_y);
//void debug_render_variables(SDL_Renderer* renderer, TTF_Font* font,
//                            const Sprite& sprite, int x, int y);
//
///* مدیریت رویداد */
//bool debug_handle_event(DebugState& ds, const SDL_Event& event);
//
//#endif /* DEBUG_MODE_H */
