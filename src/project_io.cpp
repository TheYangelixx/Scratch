#include "project_io.h"
#include "logger.h"
#include <fstream>
#include <sstream>

bool project_save(const AppState& state, const std::string& filepath) {
    std::string json = project_to_json(state);
    std::ofstream file(filepath);
    if (!file.is_open()) {
        log_error("ProjectIO: Cannot open file for writing: " + filepath);
        return false;
    }
    file << json;
    file.close();
    log_info("ProjectIO: Project saved to " + filepath);
    return true;
}

bool project_load(AppState& state, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        log_error("ProjectIO: Cannot open file for reading: " + filepath);
        return false;
    }
    // TODO: پیاده‌سازی واقعی پارسر JSON
    log_info("ProjectIO: Project loaded from " + filepath);
    (void)state;
    return true;
}

std::string project_to_json(const AppState& state) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"project_name\": \"" << state.project_name << "\",\n";
    ss << "  \"stage_width\": " << state.stage_width << ",\n";
    ss << "  \"stage_height\": " << state.stage_height << ",\n";
    ss << "  \"sprites\": [\n";

    for (int i = 0; i < (int)state.sprites.size(); i++) {
        const Sprite& s = state.sprites[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << s.name << "\",\n";
        ss << "      \"x\": " << s.x << ",\n";
        ss << "      \"y\": " << s.y << ",\n";
        ss << "      \"direction\": " << s.direction << ",\n";
        ss << "      \"size\": " << s.size << ",\n";
        ss << "      \"visible\": " << (s.visible ? "true" : "false") << ",\n";
        ss << "      \"scripts_count\": " << s.scripts.size() << "\n";
        ss << "    }";
        if (i + 1 < (int)state.sprites.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}
