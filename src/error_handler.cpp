//#include "error_handler.h"
//#include <cmath>
//#include <sstream>
//
//StageBounds error_default_bounds(int stage_w, int stage_h) {
//    StageBounds b;
//    b.min_x = -(float)stage_w / 2.0f;
//    b.max_x = (float)stage_w / 2.0f;
//    b.min_y = -(float)stage_h / 2.0f;
//    b.max_y = (float)stage_h / 2.0f;
//    return b;
//}
//
//float error_clamp_x(float x, const StageBounds& bounds) {
//    if (x < bounds.min_x) return bounds.min_x;
//    if (x > bounds.max_x) return bounds.max_x;
//    return x;
//}
//
//float error_clamp_y(float y, const StageBounds& bounds) {
//    if (y < bounds.min_y) return bounds.min_y;
//    if (y > bounds.max_y) return bounds.max_y;
//    return y;
//}
//
//bool error_is_out_of_bounds(float x, float y, const StageBounds& bounds) {
//    return (x < bounds.min_x || x > bounds.max_x ||
//            y < bounds.min_y || y > bounds.max_y);
//}
//
//void error_clamp_position(float& x, float& y, const StageBounds& bounds) {
//    x = error_clamp_x(x, bounds);
//    y = error_clamp_y(y, bounds);
//}
//
//float error_safe_divide(float a, float b, Logger& logger, int line) {
//    if (b == 0.0f) {
//        std::ostringstream oss;
//        oss << "Division by zero: " << a << " / 0 at line " << line;
//        logger_log(logger, LOG_ERROR, line, "DIVIDE", "Division by zero", oss.str());
//        return 0.0f;
//    }
//    return a / b;
//}
//
//float error_safe_sqrt(float val, Logger& logger, int line) {
//    if (val < 0.0f) {
//        std::ostringstream oss;
//        oss << "Sqrt of negative: sqrt(" << val << ") at line " << line;
//        logger_log(logger, LOG_ERROR, line, "SQRT", "Negative sqrt", oss.str());
//        return 0.0f;
//    }
//    return std::sqrt(val);
//}
//
//float error_safe_mod(float a, float b, Logger& logger, int line) {
//    if (b == 0.0f) {
//        std::ostringstream oss;
//        oss << "Mod by zero: " << a << " mod 0 at line " << line;
//        logger_log(logger, LOG_ERROR, line, "MOD", "Mod by zero", oss.str());
//        return 0.0f;
//    }
//    return std::fmod(a, b);
//}
//
//void watchdog_init(Watchdog& wd, int max_per_frame) {
//    wd.instructions_this_frame = 0;
//    wd.max_instructions_per_frame = max_per_frame;
//    wd.triggered = false;
//    wd.last_trigger_info = "";
//}
//
//void watchdog_reset(Watchdog& wd) {
//    wd.instructions_this_frame = 0;
//    wd.triggered = false;
//}
//
//bool watchdog_tick(Watchdog& wd, Logger& logger, int current_line) {
//    wd.instructions_this_frame++;
//    if (wd.instructions_this_frame >= wd.max_instructions_per_frame) {
//        wd.triggered = true;
//        std::ostringstream oss;
//        oss << "Infinite loop detected at line " << current_line
//            << " (exceeded " << wd.max_instructions_per_frame << " instructions/frame)";
//        wd.last_trigger_info = oss.str();
//        logger_log(logger, LOG_ERROR, current_line, "WATCHDOG",
//                   "Infinite loop detected", wd.last_trigger_info);
//        return true;
//    }
//    return false;
//}
//
//bool error_validate_variable_name(const std::string& name) {
//    if (name.empty()) return false;
//    for (int i = 0; i < (int)name.size(); i++) {
//        char c = name[i];
//        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
//              (c >= '0' && c <= '9') || c == '_')) {
//            return false;
//        }
//    }
//    if (name[0] >= '0' && name[0] <= '9') return false;
//    return true;
//}
//
//bool error_validate_number_range(float val, float min_v, float max_v) {
//    return (val >= min_v && val <= max_v);
//}
//
//float error_clamp_value(float val, float min_v, float max_v) {
//    if (val < min_v) return min_v;
//    if (val > max_v) return max_v;
//    return val;
//}
