//#include "debug_mode.h"
//#include "ui.h"
//#include <sstream>
//#include <iomanip>
//
//void debug_init(DebugState& ds) {
//    ds.is_debug_mode = false;
//    ds.waiting_for_step = false;
//    ds.step_requested = false;
//    ds.current_block_id = -1;
//    ds.current_sprite_id = -1;
//    ds.current_line = 0;
//}
//
//void debug_toggle(DebugState& ds) {
//    ds.is_debug_mode = !ds.is_debug_mode;
//    if (!ds.is_debug_mode) {
//        ds.waiting_for_step = false;
//        ds.step_requested = false;
//    } else {
//        ds.waiting_for_step = true;
//    }
//}
//
//void debug_enable(DebugState& ds) {
//    ds.is_debug_mode = true;
//    ds.waiting_for_step = true;
//}
//
//void debug_disable(DebugState& ds) {
//    ds.is_debug_mode = false;
//    ds.waiting_for_step = false;
//    ds.step_requested = false;
//}
//
//void debug_request_step(DebugState& ds) {
//    if (ds.is_debug_mode && ds.waiting_for_step) {
//        ds.step_requested = true;
//    }
//}
//
//bool debug_should_pause(const DebugState& ds) {
//    return ds.is_debug_mode && ds.waiting_for_step && !ds.step_requested;
//}
//
//void debug_set_current(DebugState& ds, int sprite_id, int block_id, int line) {
//    ds.current_sprite_id = sprite_id;
//    ds.current_block_id = block_id;
//    ds.current_line = line;
//}
//
//void debug_add_breakpoint(DebugState& ds, int block_id) {
//    for (int i = 0; i < (int)ds.breakpoint_block_ids.size(); i++) {
//        if (ds.breakpoint_block_ids[i] == block_id) return;
//    }
//    ds.breakpoint_block_ids.push_back(block_id);
//}
//
//void debug_remove_breakpoint(DebugState& ds, int block_id) {
//    for (int i = 0; i < (int)ds.breakpoint_block_ids.size(); i++) {
//        if (ds.breakpoint_block_ids[i] == block_id) {
//            ds.breakpoint_block_ids.erase(ds.breakpoint_block_ids.begin() + i);
//            return;
//        }
//    }
//}
//
//bool debug_is_breakpoint(const DebugState& ds, int block_id) {
//    for (int i = 0; i < (int)ds.breakpoint_block_ids.size(); i++) {
//        if (ds.breakpoint_block_ids[i] == block_id) return true;
//    }
//    return false;
//}
//
//void debug_clear_breakpoints(DebugState& ds) {
//    ds.breakpoint_block_ids.clear();
//}
//
//void debug_render_overlay(SDL_Renderer* renderer, TTF_Font* font,
//                          const DebugState& ds, const Sprite& sprite,
//                          int screen_x, int screen_y) {
//    if (!ds.is_debug_mode) return;
//
//    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
//    SDL_Rect panel = {screen_x, screen_y, 280, 200};
//    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
//    SDL_RenderFillRect(renderer, &panel);
//    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
//    SDL_RenderDrawRect(renderer, &panel);
//
//    SDL_Color cyan = {0, 200, 255, 255};
//    SDL_Color white = {220, 220, 220, 255};
//    SDL_Color yellow_c = {255, 255, 0, 255};
//
//    int ly = screen_y + 5;
//    int lx = screen_x + 10;
//
//    render_text(renderer, font, "=== DEBUG MODE ===", lx, ly, cyan);
//    ly += 18;
//
//    std::ostringstream oss;
//    oss << "Sprite: " << sprite.name;
//    render_text(renderer, font, oss.str(), lx, ly, white);
//    ly += 16;
//
//    oss.str(""); oss << "X: " << std::fixed << std::setprecision(1) << sprite.x;
//    render_text(renderer, font, oss.str(), lx, ly, white);
//    ly += 16;
//
//    oss.str(""); oss << "Y: " << std::fixed << std::setprecision(1) << sprite.y;
//    render_text(renderer, font, oss.str(), lx, ly, white);
//    ly += 16;
//
//    oss.str(""); oss << "Direction: " << std::fixed << std::setprecision(1) << sprite.direction;
//    render_text(renderer, font, oss.str(), lx, ly, white);
//    ly += 16;
//
//    oss.str(""); oss << "Size: " << std::fixed << std::setprecision(1) << sprite.size << "%";
//    render_text(renderer, font, oss.str(), lx, ly, white);
//    ly += 16;
//
//    oss.str(""); oss << "Visible: " << (sprite.visible ? "Yes" : "No");
//    render_text(renderer, font, oss.str(), lx, ly, white);
//    ly += 16;
//
//    oss.str(""); oss << "Pen: " << (sprite.pen_down ? "DOWN" : "UP");
//    render_text(renderer, font, oss.str(), lx, ly, white);
//    ly += 16;
//
//    oss.str(""); oss << "Block ID: " << ds.current_block_id
//                     << " | Line: " << ds.current_line;
//    render_text(renderer, font, oss.str(), lx, ly, yellow_c);
//    ly += 18;
//
//    render_text(renderer, font, "[Space] = Step | [Esc] = Exit Debug", lx, ly, cyan);
//}
//
//void debug_render_variables(SDL_Renderer* renderer, TTF_Font* font,
//                            const Sprite& sprite, int x, int y) {
//    SDL_Color white = {220, 220, 220, 255};
//    SDL_Color orange = {255, 180, 50, 255};
//
//    render_text(renderer, font, "Variables:", x, y, orange);
//    y += 16;
//
//    for (auto it = sprite.variables.begin(); it != sprite.variables.end(); ++it) {
//        std::ostringstream oss;
//        oss << it->first << " = " << std::fixed << std::setprecision(2) << it->second;
//        render_text(renderer, font, oss.str(), x + 5, y, white);
//        y += 14;
//    }
//}
//
//bool debug_handle_event(DebugState& ds, const SDL_Event& event) {
//    if (!ds.is_debug_mode) return false;
//
//    if (event.type == SDL_KEYDOWN) {
//        switch (event.key.keysym.sym) {
//            case SDLK_SPACE:
//                debug_request_step(ds);
//                return true;
//            case SDLK_F7:
//                debug_request_step(ds);
//                return true;
//            case SDLK_ESCAPE:
//                debug_disable(ds);
//                return true;
//            default:
//                break;
//        }
//    }
//    return false;
//}