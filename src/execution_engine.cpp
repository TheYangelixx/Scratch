//#include "execution_engine.h"
//#include <cmath>
//#include <cstdlib>
//#include <ctime>
//#include <sstream>
//#include <algorithm>
//
//#ifndef M_PI
//#define M_PI 3.14159265358979323846
//#endif
//
//void engine_init(ExecutionEngine& engine, int stage_w, int stage_h) {
//    engine.status = EXEC_IDLE;
//    engine.scripts.clear();
//    engine.timer = 0.0f;
//    engine.delta_time = 0.0f;
//    engine.total_instructions = 0;
//    engine.bounds = error_default_bounds(stage_w, stage_h);
//    watchdog_init(engine.watchdog, 1000);
//    srand((unsigned int)time(nullptr));
//}
//
//void engine_reset(ExecutionEngine& engine) {
//    engine.status = EXEC_IDLE;
//    engine.scripts.clear();
//    engine.timer = 0.0f;
//    engine.total_instructions = 0;
//    watchdog_reset(engine.watchdog);
//}
//
//void engine_start(ExecutionEngine& engine, std::vector<Sprite>& sprites,
//                  Logger& logger) {
//    engine.scripts.clear();
//    engine.status = EXEC_RUNNING;
//    engine.timer = 0.0f;
//    engine.total_instructions = 0;
//
//    logger_log_simple(logger, LOG_INFO, "Execution started (Green Flag)");
//
//    /* هر sprite را بررسی کن و اسکریپت‌هایی که با WHEN_FLAG_CLICKED شروع می‌شوند پیدا کن */
//    for (int si = 0; si < (int)sprites.size(); si++) {
//        Sprite& s = sprites[si];
//        for (int bi = 0; bi < (int)s.blocks.size(); bi++) {
//            if (s.blocks[bi].type == BLOCK_WHEN_FLAG_CLICKED &&
//                s.blocks[bi].prev_block_id < 0) {
//
//                /* زنجیره بلوک‌ها */
//                ScriptExecution se;
//                se.sprite_index = si;
//                se.current_index = 0;
//                se.active = true;
//                se.waiting = false;
//                se.wait_timer = 0.0f;
//                se.gliding = false;
//                se.glide_start_x = 0;
//                se.glide_start_y = 0;
//                se.glide_end_x = 0;
//                se.glide_end_y = 0;
//                se.glide_duration = 0;
//                se.glide_elapsed = 0;
//
//                /* ساخت chain */
//                int current_id = s.blocks[bi].id;
//                int safety = 0;
//                while (current_id >= 0 && safety < 10000) {
//                    se.block_chain.push_back(current_id);
//                    Block* cb = sprite_find_block(s, current_id);
//                    if (!cb) break;
//                    current_id = cb->next_block_id;
//                    safety++;
//                }
//
//                engine.scripts.push_back(se);
//
//                std::ostringstream oss;
//                oss << "Script found for sprite '" << s.name
//                    << "' with " << se.block_chain.size() << " blocks";
//                logger_log_simple(logger, LOG_INFO, oss.str());
//            }
//        }
//    }
//}
//
//void engine_stop(ExecutionEngine& engine, std::vector<Sprite>& sprites) {
//    engine.status = EXEC_STOPPED;
//    engine.scripts.clear();
//}
//
//static void execute_single_script(ExecutionEngine& engine, ScriptExecution& se,
//                                  std::vector<Sprite>& sprites, Logger& logger,
//                                  DebugState& debug, PenCanvas& pen, SoundManager& sound) {
//    if (!se.active) return;
//    if (se.sprite_index < 0 || se.sprite_index >= (int)sprites.size()) {
//        se.active = false;
//        return;
//    }
//
//    Sprite& sprite = sprites[se.sprite_index];
//
//    /* Waiting */
//    if (se.waiting) {
//        se.wait_timer -= engine.delta_time;
//        if (se.wait_timer <= 0.0f) {
//            se.waiting = false;
//            se.current_index++;
//        }
//        return;
//    }
//
//    /* Gliding */
//    if (se.gliding) {
//        se.glide_elapsed += engine.delta_time;
//        float t = se.glide_elapsed / se.glide_duration;
//        if (t >= 1.0f) {
//            t = 1.0f;
//            se.gliding = false;
//            se.current_index++;
//        }
//        sprite.x = se.glide_start_x + (se.glide_end_x - se.glide_start_x) * t;
//        sprite.y = se.glide_start_y + (se.glide_end_y - se.glide_start_y) * t;
//        return;
//    }
//
//    /* آیا به انتهای زنجیره رسیدیم؟ */
//    if (se.current_index >= (int)se.block_chain.size()) {
//        /* اگر loop stack خالی نیست، بررسی حلقه */
//        if (!se.loop_stack.empty()) {
//            LoopState& ls = se.loop_stack.back();
//            ls.iteration++;
//            Block* loop_block = sprite_find_block(sprite, ls.block_id);
//
//            if (loop_block && loop_block->type == BLOCK_FOREVER) {
//                /* حلقه forever: برگرد به ابتدای حلقه */
//                /* بازیابی chain به body */
//                se.current_index = 0;
//                /* rebuild chain from body */
//                se.block_chain.clear();
//                for (int i = 0; i < (int)loop_block->body_block_ids.size(); i++) {
//                    se.block_chain.push_back(loop_block->body_block_ids[i]);
//                }
//                return;
//            }
//            else if (loop_block && loop_block->type == BLOCK_REPEAT) {
//                if (ls.iteration < ls.max_iterations) {
//                    se.current_index = 0;
//                    se.block_chain.clear();
//                    for (int i = 0; i < (int)loop_block->body_block_ids.size(); i++) {
//                        se.block_chain.push_back(loop_block->body_block_ids[i]);
//                    }
//                    return;
//                } else {
//                    se.loop_stack.pop_back();
//                    /* ادامه بعد از حلقه */
//                    /* TODO: restore parent chain */
//                }
//            }
//        }
//        se.active = false;
//        return;
//    }
//
//    int block_id = se.block_chain[se.current_index];
//    Block* block = sprite_find_block(sprite, block_id);
//    if (!block) {
//        se.current_index++;
//        return;
//    }
//
//    /* Watchdog check */
//    if (watchdog_tick(engine.watchdog, logger, se.current_index)) {
//        se.active = false;
//        engine.status = EXEC_ERROR;
//        return;
//    }
//
//    /* Debug */
//    int line_num = se.current_index + 1;
//    debug_set_current(debug, se.sprite_index, block_id, line_num);
//
//    /* اجرای بلوک */
//    engine_execute_block(engine, sprite, *block, se, logger, debug, pen, sound);
//    engine.total_instructions++;
//}
//
//void engine_update(ExecutionEngine& engine, std::vector<Sprite>& sprites,
//                   Logger& logger, DebugState& debug, PenCanvas& pen,
//                   SoundManager& sound, float dt) {
//    if (engine.status != EXEC_RUNNING) return;
//
//    engine.delta_time = dt;
//    engine.timer += dt;
//    watchdog_reset(engine.watchdog);
//    logger_increment_cycle(logger);
//
//    /* Debug mode: pause */
//    if (debug_should_pause(debug)) return;
//
//    /* Debug step: فقط یک دستور */
//    if (debug.is_debug_mode && debug.step_requested) {
//        debug.step_requested = false;
//        debug.waiting_for_step = true;
//
//        /* فقط یک دستور از هر اسکریپت */
//        for (int i = 0; i < (int)engine.scripts.size(); i++) {
//            execute_single_script(engine, engine.scripts[i], sprites,
//                                  logger, debug, pen, sound);
//        }
//        return;
//    }
//
//    /* اجرای عادی */
//    bool any_active = false;
//    for (int i = 0; i < (int)engine.scripts.size(); i++) {
//        if (engine.scripts[i].active) {
//            any_active = true;
//            execute_single_script(engine, engine.scripts[i], sprites,
//                                  logger, debug, pen, sound);
//        }
//    }
//
//    /* Say timer update */
//    for (int si = 0; si < (int)sprites.size(); si++) {
//        if (sprites[si].say_timer > 0) {
//            sprites[si].say_timer -= dt;
//            if (sprites[si].say_timer <= 0) {
//                sprites[si].say_text = "";
//                sprites[si].say_timer = 0;
//            }
//        }
//    }
//
//    if (!any_active) {
//        engine.status = EXEC_IDLE;
//        logger_log_simple(logger, LOG_INFO, "All scripts finished");
//    }
//}
//
//void engine_step_once(ExecutionEngine& engine, std::vector<Sprite>& sprites,
//                      Logger& logger, DebugState& debug, PenCanvas& pen,
//                      SoundManager& sound) {
//    if (engine.status != EXEC_RUNNING && engine.status != EXEC_PAUSED) return;
//
//    engine.delta_time = 1.0f / 30.0f; /* فرض فریم ۳۰fps */
//    watchdog_reset(engine.watchdog);
//
//    for (int i = 0; i < (int)engine.scripts.size(); i++) {
//        if (engine.scripts[i].active) {
//            execute_single_script(engine, engine.scripts[i], sprites,
//                                  logger, debug, pen, sound);
//            break; /* فقط یک اسکریپت */
//        }
//    }
//}
//
//void engine_reset_timer(ExecutionEngine& engine) {
//    engine.timer = 0.0f;
//}
//
//float engine_get_timer(const ExecutionEngine& engine) {
//    return engine.timer;
//}
//
///* ============= اجرای بلوک منفرد ============= */
//
//void engine_execute_block(ExecutionEngine& engine, Sprite& sprite, Block& block,
//                          ScriptExecution& script, Logger& logger,
//                          DebugState& debug, PenCanvas& pen, SoundManager& sound) {
//
//    std::ostringstream log_data;
//    float old_x = sprite.x;
//    float old_y = sprite.y;
//
//    switch (block.type) {
//
//        /* ========== MOTION ========== */
//        case BLOCK_MOVE_STEPS: {
//            float steps = block.param_num1;
//            if (steps == 0) steps = 10.0f;
//            float old_px = sprite.x, old_py = sprite.y;
//            sprite_move(sprite, steps);
//            /* Pen drawing */
//            if (sprite.pen_down) {
//                pen_draw_line(pen, old_px, old_py, sprite.x, sprite.y,
//                              sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b,
//                              255, sprite.pen_size);
//            }
//            error_clamp_position(sprite.x, sprite.y, engine.bounds);
//            log_data << "X: " << old_px << " -> " << sprite.x
//                     << ", Y: " << old_py << " -> " << sprite.y;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "MOVE_STEPS", "Moved", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_TURN_RIGHT: {
//            float deg = block.param_num1;
//            if (deg == 0) deg = 15.0f;
//            float old_dir = sprite.direction;
//            sprite_turn_right(sprite, deg);
//            log_data << "Dir: " << old_dir << " -> " << sprite.direction;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "TURN_RIGHT", "Turned right", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_TURN_LEFT: {
//            float deg = block.param_num1;
//            if (deg == 0) deg = 15.0f;
//            float old_dir = sprite.direction;
//            sprite_turn_left(sprite, deg);
//            log_data << "Dir: " << old_dir << " -> " << sprite.direction;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "TURN_LEFT", "Turned left", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_GO_TO_XY: {
//            float old_px = sprite.x, old_py = sprite.y;
//            float nx = block.param_num1;
//            float ny = block.param_num2;
//            if (sprite.pen_down) {
//                pen_draw_line(pen, old_px, old_py, nx, ny,
//                              sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b,
//                              255, sprite.pen_size);
//            }
//            sprite_go_to(sprite, nx, ny);
//            error_clamp_position(sprite.x, sprite.y, engine.bounds);
//            log_data << "X: " << old_px << " -> " << sprite.x
//                     << ", Y: " << old_py << " -> " << sprite.y;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "GO_TO_XY", "Go to", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_GLIDE_TO_XY: {
//            script.gliding = true;
//            script.glide_start_x = sprite.x;
//            script.glide_start_y = sprite.y;
//            script.glide_end_x = block.param_num1;
//            script.glide_end_y = block.param_num2;
//            script.glide_duration = (block.param_num1 > 0) ? block.param_num1 : 1.0f;
//            /* NOTE: param_num1 used for duration in glide, param_num2 for x,
//               in practice need a third param. Simplified: duration = 1 sec */
//            script.glide_duration = 1.0f;
//            script.glide_elapsed = 0.0f;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "GLIDE", "Gliding to target", "");
//            /* current_index advances when glide completes */
//        } break;
//
//        case BLOCK_CHANGE_X: {
//            float dx = block.param_num1;
//            if (dx == 0) dx = 10.0f;
//            float old_px = sprite.x;
//            if (sprite.pen_down) {
//                pen_draw_line(pen, sprite.x, sprite.y, sprite.x + dx, sprite.y,
//                              sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b,
//                              255, sprite.pen_size);
//            }
//            sprite_change_x(sprite, dx);
//            error_clamp_position(sprite.x, sprite.y, engine.bounds);
//            log_data << "X: " << old_px << " -> " << sprite.x;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "CHANGE_X", "Changed X", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_CHANGE_Y: {
//            float dy = block.param_num1;
//            if (dy == 0) dy = 10.0f;
//            float old_py = sprite.y;
//            if (sprite.pen_down) {
//                pen_draw_line(pen, sprite.x, sprite.y, sprite.x, sprite.y + dy,
//                              sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b,
//                              255, sprite.pen_size);
//            }
//            sprite_change_y(sprite, dy);
//            error_clamp_position(sprite.x, sprite.y, engine.bounds);
//            log_data << "Y: " << old_py << " -> " << sprite.y;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "CHANGE_Y", "Changed Y", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_SET_X: {
//            float old_px = sprite.x;
//            float nx = block.param_num1;
//            if (sprite.pen_down) {
//                pen_draw_line(pen, sprite.x, sprite.y, nx, sprite.y,
//                              sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b,
//                              255, sprite.pen_size);
//            }
//            sprite_set_x(sprite, nx);
//            error_clamp_position(sprite.x, sprite.y, engine.bounds);
//            log_data << "X: " << old_px << " -> " << sprite.x;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SET_X", "Set X", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_SET_Y: {
//            float old_py = sprite.y;
//            float ny = block.param_num1;
//            if (sprite.pen_down) {
//                pen_draw_line(pen, sprite.x, sprite.y, sprite.x, ny,
//                              sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b,
//                              255, sprite.pen_size);
//            }
//            sprite_set_y(sprite, ny);
//            error_clamp_position(sprite.x, sprite.y, engine.bounds);
//            log_data << "Y: " << old_py << " -> " << sprite.y;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SET_Y", "Set Y", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_SET_DIRECTION: {
//            float old_dir = sprite.direction;
//            float nd = block.param_num1;
//            if (nd == 0) nd = 90.0f;
//            sprite_set_direction(sprite, nd);
//            log_data << "Dir: " << old_dir << " -> " << sprite.direction;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SET_DIR", "Set direction", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_BOUNCE_ON_EDGE: {
//            sprite_bounce_on_edge(sprite, (int)(engine.bounds.max_x * 2),
//                                  (int)(engine.bounds.max_y * 2));
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "BOUNCE", "Bounce on edge", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_GO_TO_RANDOM: {
//            float old_px = sprite.x, old_py = sprite.y;
//            float rx = (float)(rand() % (int)(engine.bounds.max_x * 2)) + engine.bounds.min_x;
//            float ry = (float)(rand() % (int)(engine.bounds.max_y * 2)) + engine.bounds.min_y;
//            if (sprite.pen_down) {
//                pen_draw_line(pen, old_px, old_py, rx, ry,
//                              sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b,
//                              255, sprite.pen_size);
//            }
//            sprite_go_to(sprite, rx, ry);
//            log_data << "Random position: (" << rx << ", " << ry << ")";
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "GO_RANDOM", "Go to random", log_data.str());
//            script.current_index++;
//        } break;
//
//            /* ========== LOOKS ========== */
//        case BLOCK_SAY: {
//            std::string text = block.param_str1;
//            if (text.empty()) text = "Hello!";
//            sprite_say(sprite, text, 0);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SAY", "Say", text);
//            script.current_index++;
//        } break;
//
//        case BLOCK_SAY_FOR_SECS: {
//            std::string text = block.param_str1;
//            if (text.empty()) text = "Hello!";
//            float secs = block.param_num1;
//            if (secs <= 0) secs = 2.0f;
//            sprite_say(sprite, text, secs);
//            script.waiting = true;
//            script.wait_timer = secs;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SAY_SECS", "Say for seconds", text);
//        } break;
//
//        case BLOCK_THINK: {
//            std::string text = block.param_str1;
//            if (text.empty()) text = "Hmm...";
//            sprite_think(sprite, text, 0);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "THINK", "Think", text);
//            script.current_index++;
//        } break;
//
//        case BLOCK_THINK_FOR_SECS: {
//            std::string text = block.param_str1;
//            if (text.empty()) text = "Hmm...";
//            float secs = block.param_num1;
//            if (secs <= 0) secs = 2.0f;
//            sprite_think(sprite, text, secs);
//            script.waiting = true;
//            script.wait_timer = secs;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "THINK_SECS", "Think for seconds", text);
//        } break;
//
//        case BLOCK_SHOW: {
//            sprite_show(sprite);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SHOW", "Show sprite", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_HIDE: {
//            sprite_hide(sprite);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "HIDE", "Hide sprite", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_SET_SIZE: {
//            float old_size = sprite.size;
//            float ns = block.param_num1;
//            if (ns == 0) ns = 100.0f;
//            sprite_set_size(sprite, ns);
//            log_data << "Size: " << old_size << " -> " << sprite.size;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SET_SIZE", "Set size", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_CHANGE_SIZE: {
//            float old_size = sprite.size;
//            float ds = block.param_num1;
//            if (ds == 0) ds = 10.0f;
//            sprite_change_size(sprite, ds);
//            log_data << "Size: " << old_size << " -> " << sprite.size;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "CHANGE_SIZE", "Changed size", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_NEXT_COSTUME: {
//            sprite_next_costume(sprite);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "NEXT_COSTUME", "Next costume", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_CLEAR_EFFECTS: {
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "CLEAR_FX", "Clear effects", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_GO_TO_FRONT: {
//            sprite.layer = 999;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "TO_FRONT", "Go to front", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_GO_TO_BACK: {
//            sprite.layer = -999;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "TO_BACK", "Go to back", "");
//            script.current_index++;
//        } break;
//
//            /* ========== SOUND ========== */
//        case BLOCK_PLAY_SOUND: {
//            std::string snd = block.param_str1;
//            if (snd.empty()) snd = "pop";
//            sound_play(sound, snd);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PLAY_SOUND", "Play sound", snd);
//            script.current_index++;
//        } break;
//
//        case BLOCK_PLAY_SOUND_UNTIL_DONE: {
//            std::string snd = block.param_str1;
//            if (snd.empty()) snd = "pop";
//            sound_play(sound, snd);
//            /* TODO: wait until done بدون بلوکه کردن حلقه اصلی */
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PLAY_UNTIL_DONE", "Play sound until done", snd);
//            script.current_index++;
//        } break;
//
//        case BLOCK_STOP_ALL_SOUNDS: {
//            sound_stop_all(sound);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "STOP_SOUNDS", "All sounds stopped", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_CHANGE_VOLUME: {
//            float dv = block.param_num1;
//            if (dv == 0) dv = -10.0f;
//            sprite.volume = error_clamp_value(sprite.volume + dv, 0.0f, 100.0f);
//            log_data << "Volume: " << sprite.volume;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "CHANGE_VOL", "Changed volume", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_SET_VOLUME: {
//            float nv = block.param_num1;
//            if (nv == 0) nv = 100.0f;
//            sprite.volume = error_clamp_value(nv, 0.0f, 100.0f);
//            log_data << "Volume: " << sprite.volume;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SET_VOL", "Set volume", log_data.str());
//            script.current_index++;
//        } break;
//
//            /* ========== EVENTS ========== */
//        case BLOCK_WHEN_FLAG_CLICKED: {
//            /* Event block - فقط skip */
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "FLAG", "Green flag clicked", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_WHEN_KEY_PRESSED:
//        case BLOCK_WHEN_SPRITE_CLICKED:
//        case BLOCK_WHEN_BACKDROP_CHANGES:
//        case BLOCK_WHEN_I_RECEIVE: {
//            script.current_index++;
//        } break;
//
//        case BLOCK_BROADCAST: {
//            /* TODO: broadcast message to other scripts */
//            std::string msg = block.param_str1;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "BROADCAST", "Broadcast", msg);
//            script.current_index++;
//        } break;
//
//            /* ========== CONTROL ========== */
//        case BLOCK_WAIT: {
//            float secs = block.param_num1;
//            if (secs <= 0) secs = 1.0f;
//            script.waiting = true;
//            script.wait_timer = secs;
//            log_data << "Wait: " << secs << "s";
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "WAIT", "Waiting", log_data.str());
//        } break;
//
//        case BLOCK_REPEAT: {
//            int times = (int)block.param_num1;
//            if (times <= 0) times = 10;
//            LoopState ls;
//            ls.block_id = block.id;
//            ls.iteration = 0;
//            ls.max_iterations = times;
//            ls.body_index = 0;
//            script.loop_stack.push_back(ls);
//
//            if (!block.body_block_ids.empty()) {
//                /* Rebuild chain from body */
//                /* Save current remaining chain */
//                /* Simplified: just continue with body */
//                logger_log(logger, LOG_INFO, script.current_index + 1,
//                           "REPEAT", "Repeat started", "");
//                script.current_index++;
//            } else {
//                script.current_index++;
//            }
//        } break;
//
//        case BLOCK_FOREVER: {
//            LoopState ls;
//            ls.block_id = block.id;
//            ls.iteration = 0;
//            ls.max_iterations = -1; /* infinite */
//            ls.body_index = 0;
//            script.loop_stack.push_back(ls);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "FOREVER", "Forever loop started", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_IF: {
//            /* simplified: always true for now */
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "IF", "If (simplified - true)", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_IF_ELSE: {
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "IF_ELSE", "If-Else (simplified)", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_STOP: {
//            std::string mode = block.param_str1;
//            if (mode == "all" || mode.empty()) {
//                engine.status = EXEC_STOPPED;
//                script.active = false;
//            } else if (mode == "this script") {
//                script.active = false;
//            }
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "STOP", "Stop", mode);
//        } break;
//
//        case BLOCK_CLONE: {
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "CLONE", "Create clone", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_DELETE_CLONE: {
//            if (sprite.is_clone) {
//                sprite.visible = false;
//                script.active = false;
//            }
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "DEL_CLONE", "Delete clone", "");
//        } break;
//
//            /* ========== SENSING ========== */
//        case BLOCK_TIMER: {
//            /* Reporter block - store to param */
//            log_data << "Timer: " << engine.timer;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "TIMER", "Timer value", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_RESET_TIMER: {
//            engine_reset_timer(engine);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "RESET_TIMER", "Timer reset", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_ASK_AND_WAIT: {
//            std::string question = block.param_str1;
//            if (question.empty()) question = "What's your name?";
//            sprite_say(sprite, question, 0);
//            /* Simplified: just continue */
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "ASK", "Ask and wait", question);
//            script.current_index++;
//        } break;
//
//            /* ========== OPERATORS ========== */
//        case BLOCK_ADD: {
//            float result = block.param_num1 + block.param_num2;
//            log_data << block.param_num1 << " + " << block.param_num2 << " = " << result;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "ADD", "Addition", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_SUBTRACT: {
//            float result = block.param_num1 - block.param_num2;
//            log_data << block.param_num1 << " - " << block.param_num2 << " = " << result;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SUB", "Subtraction", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_MULTIPLY: {
//            float result = block.param_num1 * block.param_num2;
//            log_data << block.param_num1 << " * " << block.param_num2 << " = " << result;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "MUL", "Multiplication", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_DIVIDE: {
//            float result = error_safe_divide(block.param_num1, block.param_num2,
//                                             logger, script.current_index + 1);
//            log_data << block.param_num1 << " / " << block.param_num2 << " = " << result;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "DIV", "Division", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_RANDOM: {
//            int lo = (int)block.param_num1;
//            int hi = (int)block.param_num2;
//            if (lo == 0 && hi == 0) { lo = 1; hi = 10; }
//            if (lo > hi) { int tmp = lo; lo = hi; hi = tmp; }
//            int result = lo + rand() % (hi - lo + 1);
//            log_data << "Random(" << lo << ", " << hi << ") = " << result;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "RANDOM", "Random", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_MOD: {
//            float result = error_safe_mod(block.param_num1, block.param_num2,
//                                          logger, script.current_index + 1);
//            log_data << block.param_num1 << " mod " << block.param_num2 << " = " << result;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "MOD", "Modulo", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_ROUND: {
//            float result = std::round(block.param_num1);
//            log_data << "round(" << block.param_num1 << ") = " << result;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "ROUND", "Round", log_data.str());
//            script.current_index++;
//        } break;
//
//            /* ========== VARIABLES ========== */
//        case BLOCK_SET_VARIABLE: {
//            std::string var_name = block.param_str1;
//            if (var_name.empty()) var_name = "myVar";
//            float val = block.param_num1;
//            sprite.variables[var_name] = val;
//            log_data << var_name << " = " << val;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "SET_VAR", "Set variable", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_CHANGE_VARIABLE: {
//            std::string var_name = block.param_str1;
//            if (var_name.empty()) var_name = "myVar";
//            float change = block.param_num1;
//            if (change == 0) change = 1.0f;
//            float old_val = sprite.variables[var_name];
//            sprite.variables[var_name] = old_val + change;
//            log_data << var_name << ": " << old_val << " -> " << sprite.variables[var_name];
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "CHANGE_VAR", "Changed variable", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_SHOW_VARIABLE:
//        case BLOCK_HIDE_VARIABLE: {
//            script.current_index++;
//        } break;
//
//            /* ========== PEN ========== */
//        case BLOCK_PEN_ERASE_ALL: {
//            pen_canvas_clear(pen);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_ERASE", "Erase all", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_STAMP: {
//            pen_stamp(pen, sprite.id, sprite.x, sprite.y, sprite.size, sprite.current_costume);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_STAMP", "Stamp", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_DOWN: {
//            sprite.pen_down = true;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_DOWN", "Pen down", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_UP: {
//            sprite.pen_down = false;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_UP", "Pen up", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_SET_COLOR: {
//            /* param_str1 could be "#RRGGBB" */
//            std::string color_str = block.param_str1;
//            if (color_str.size() == 7 && color_str[0] == '#') {
//                int r = std::stoi(color_str.substr(1, 2), nullptr, 16);
//                int g = std::stoi(color_str.substr(3, 2), nullptr, 16);
//                int b = std::stoi(color_str.substr(5, 2), nullptr, 16);
//                sprite.pen_color_r = r;
//                sprite.pen_color_g = g;
//                sprite.pen_color_b = b;
//            } else {
//                /* use HSB */
//                pen_hsb_to_rgb(sprite.pen_hue, sprite.pen_saturation, sprite.pen_brightness,
//                               sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b);
//            }
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_SET_COLOR", "Set pen color", color_str);
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_CHANGE_COLOR: {
//            sprite.pen_hue = std::fmod(sprite.pen_hue + block.param_num1, 360.0f);
//            pen_hsb_to_rgb(sprite.pen_hue, sprite.pen_saturation, sprite.pen_brightness,
//                           sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_CHG_COLOR", "Changed pen color", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_SET_SIZE: {
//            float ns = block.param_num1;
//            if (ns < 1.0f) ns = 1.0f;
//            if (ns > 100.0f) ns = 100.0f;
//            sprite.pen_size = ns;
//            log_data << "Pen size: " << sprite.pen_size;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_SET_SIZE", "Set pen size", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_CHANGE_SIZE: {
//            sprite.pen_size += block.param_num1;
//            if (sprite.pen_size < 1.0f) sprite.pen_size = 1.0f;
//            if (sprite.pen_size > 100.0f) sprite.pen_size = 100.0f;
//            log_data << "Pen size: " << sprite.pen_size;
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_CHG_SIZE", "Changed pen size", log_data.str());
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_SET_SHADE: {
//            sprite.pen_shade = error_clamp_value(block.param_num1, 0.0f, 100.0f);
//            sprite.pen_brightness = sprite.pen_shade;
//            pen_hsb_to_rgb(sprite.pen_hue, sprite.pen_saturation, sprite.pen_brightness,
//                           sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_SET_SHADE", "Set pen shade", "");
//            script.current_index++;
//        } break;
//
//        case BLOCK_PEN_CHANGE_SHADE: {
//            sprite.pen_shade = error_clamp_value(sprite.pen_shade + block.param_num1, 0.0f, 100.0f);
//            sprite.pen_brightness = sprite.pen_shade;
//            pen_hsb_to_rgb(sprite.pen_hue, sprite.pen_saturation, sprite.pen_brightness,
//                           sprite.pen_color_r, sprite.pen_color_g, sprite.pen_color_b);
//            logger_log(logger, LOG_INFO, script.current_index + 1,
//                       "PEN_CHG_SHADE", "Changed pen shade", "");
//            script.current_index++;
//        } break;
//
//            /* ========== DEFAULT ========== */
//        default: {
//            logger_log(logger, LOG_WARNING, script.current_index + 1,
//                       block_type_to_string(block.type),
//                       "Unhandled block type", "");
//            script.current_index++;
//        } break;
//
//    } /* end switch */
//}
