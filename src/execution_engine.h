//#ifndef EXECUTION_ENGINE_H
//#define EXECUTION_ENGINE_H
//
//#include "sprite.h"
//#include "block.h"
//#include "logger.h"
//#include "error_handler.h"
//#include "debug_mode.h"
//#include "pen.h"
//#include "sound_manager.h"
//#include <vector>
//#include <map>
//#include <string>
//
//enum ExecStatus {
//    EXEC_IDLE = 0,
//    EXEC_RUNNING = 1,
//    EXEC_PAUSED = 2,
//    EXEC_STOPPED = 3,
//    EXEC_ERROR = 4
//};
//
//struct LoopState {
//    int block_id;
//    int iteration;
//    int max_iterations;
//    int body_index; /* اندیس فعلی در body_block_ids */
//};
//
//struct ScriptExecution {
//    int sprite_index;
//    std::vector<int> block_chain;
//    int current_index;
//    bool active;
//    bool waiting;
//    float wait_timer;
//
//    /* Stack حلقه‌ها */
//    std::vector<LoopState> loop_stack;
//
//    /* Glide state */
//    bool gliding;
//    float glide_start_x, glide_start_y;
//    float glide_end_x, glide_end_y;
//    float glide_duration;
//    float glide_elapsed;
//};
//
//struct ExecutionEngine {
//    ExecStatus status;
//    std::vector<ScriptExecution> scripts;
//    float timer;
//    float delta_time;
//    Watchdog watchdog;
//    StageBounds bounds;
//    int total_instructions;
//};
//
///* Init */
//void engine_init(ExecutionEngine& engine, int stage_w, int stage_h);
//void engine_reset(ExecutionEngine& engine);
//
///* اجرا */
//void engine_start(ExecutionEngine& engine, std::vector<Sprite>& sprites,
//                  Logger& logger);
//void engine_stop(ExecutionEngine& engine, std::vector<Sprite>& sprites);
//void engine_update(ExecutionEngine& engine, std::vector<Sprite>& sprites,
//                   Logger& logger, DebugState& debug, PenCanvas& pen,
//                   SoundManager& sound, float dt);
//
///* Step-by-step */
//void engine_step_once(ExecutionEngine& engine, std::vector<Sprite>& sprites,
//                      Logger& logger, DebugState& debug, PenCanvas& pen,
//                      SoundManager& sound);
//
///* Timer */
//void engine_reset_timer(ExecutionEngine& engine);
//float engine_get_timer(const ExecutionEngine& engine);
//
///* اجرای یک بلوک منفرد */
//void engine_execute_block(ExecutionEngine& engine, Sprite& sprite, Block& block,
//                          ScriptExecution& script, Logger& logger,
//                          DebugState& debug, PenCanvas& pen, SoundManager& sound);
//
//#endif
