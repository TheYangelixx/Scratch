//#ifndef ERROR_HANDLER_H
//#define ERROR_HANDLER_H
//
//#include "logger.h"
//#include <string>
//
///* محدوده صحنه */
//struct StageBounds {
//    float min_x;
//    float max_x;
//    float min_y;
//    float max_y;
//};
//
///* Watchdog برای تشخیص حلقه بی‌نهایت */
//struct Watchdog {
//    int instructions_this_frame;
//    int max_instructions_per_frame;
//    bool triggered;
//    std::string last_trigger_info;
//};
//
///* توابع بررسی محدوده */
//float error_clamp_x(float x, const StageBounds& bounds);
//float error_clamp_y(float y, const StageBounds& bounds);
//bool error_is_out_of_bounds(float x, float y, const StageBounds& bounds);
//void error_clamp_position(float& x, float& y, const StageBounds& bounds);
//
///* توابع ریاضی ایمن */
//float error_safe_divide(float a, float b, Logger& logger, int line);
//float error_safe_sqrt(float val, Logger& logger, int line);
//float error_safe_mod(float a, float b, Logger& logger, int line);
//
///* Watchdog */
//void watchdog_init(Watchdog& wd, int max_per_frame);
//void watchdog_reset(Watchdog& wd);
//bool watchdog_tick(Watchdog& wd, Logger& logger, int current_line);
//
///* اعتبارسنجی */
//bool error_validate_variable_name(const std::string& name);
//bool error_validate_number_range(float val, float min_v, float max_v);
//float error_clamp_value(float val, float min_v, float max_v);
//
///* محدوده پیش‌فرض */
//StageBounds error_default_bounds(int stage_w, int stage_h);
//
//#endif /* ERROR_HANDLER_H */
