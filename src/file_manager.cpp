//#include "file_manager.h"
//#include <fstream>
//#include <sstream>
//#include <iostream>
//
//ProjectData file_new_project(const std::string& name) {
//    ProjectData pd;
//    pd.project_name = name;
//    pd.version = 1;
//    pd.stage_width = 480;
//    pd.stage_height = 360;
//    pd.backdrop_path = "";
//
//    /* اسپرایت پیش‌فرض */
//    Sprite cat = sprite_create(1, "Cat", 0, 0);
//    sprite_add_costume(cat, "costume1", "assets/cat.png");
//    pd.sprites.push_back(cat);
//
//    return pd;
//}
//
//bool file_save_project(const std::string& filepath, const ProjectData& data) {
//    std::ofstream out(filepath, std::ios::out | std::ios::trunc);
//    if (!out.is_open()) {
//        std::cerr << "[FileManager] Cannot open file for writing: " << filepath << std::endl;
//        return false;
//    }
//
//    out << "SCRATCH_PROJECT" << std::endl;
//    out << "VERSION|" << data.version << std::endl;
//    out << "NAME|" << data.project_name << std::endl;
//    out << "STAGE|" << data.stage_width << "|" << data.stage_height << std::endl;
//    out << "BACKDROP|" << data.backdrop_path << std::endl;
//
//    /* Sprites */
//    out << "SPRITES_BEGIN" << std::endl;
//    for (int i = 0; i < (int)data.sprites.size(); i++) {
//        const Sprite& s = data.sprites[i];
//        out << "SPRITE_BEGIN" << std::endl;
//        out << sprite_serialize(s) << std::endl;
//
//        /* Costumes */
//        out << "COSTUMES_BEGIN" << std::endl;
//        for (int c = 0; c < (int)s.costumes.size(); c++) {
//            out << s.costumes[c].name << "|" << s.costumes[c].filepath
//                << "|" << s.costumes[c].width << "|" << s.costumes[c].height << std::endl;
//        }
//        out << "COSTUMES_END" << std::endl;
//
//        /* Blocks */
//        out << "BLOCKS_BEGIN" << std::endl;
//        for (int b = 0; b < (int)s.blocks.size(); b++) {
//            out << block_serialize(s.blocks[b]) << std::endl;
//        }
//        out << "BLOCKS_END" << std::endl;
//
//        /* Variables */
//        out << "VARS_BEGIN" << std::endl;
//        for (auto it = s.variables.begin(); it != s.variables.end(); ++it) {
//            out << it->first << "|" << it->second << std::endl;
//        }
//        out << "VARS_END" << std::endl;
//
//        out << "SPRITE_END" << std::endl;
//    }
//    out << "SPRITES_END" << std::endl;
//
//    /* Global blocks */
//    out << "GLOBAL_BLOCKS_BEGIN" << std::endl;
//    for (int i = 0; i < (int)data.global_blocks.size(); i++) {
//        out << block_serialize(data.global_blocks[i]) << std::endl;
//    }
//    out << "GLOBAL_BLOCKS_END" << std::endl;
//
//    out << "END_PROJECT" << std::endl;
//    out.close();
//    return true;
//}
//
//bool file_load_project(const std::string& filepath, ProjectData& data) {
//    std::ifstream in(filepath, std::ios::in);
//    if (!in.is_open()) {
//        std::cerr << "[FileManager] Cannot open file for reading: " << filepath << std::endl;
//        return false;
//    }
//
//    data.sprites.clear();
//    data.global_blocks.clear();
//
//    std::string line;
//    std::getline(in, line);
//    if (line != "SCRATCH_PROJECT") {
//        std::cerr << "[FileManager] Invalid file format" << std::endl;
//        return false;
//    }
//
//    while (std::getline(in, line)) {
//        if (line.empty()) continue;
//
//        if (line.substr(0, 8) == "VERSION|") {
//            data.version = std::stoi(line.substr(8));
//        }
//        else if (line.substr(0, 5) == "NAME|") {
//            data.project_name = line.substr(5);
//        }
//        else if (line.substr(0, 6) == "STAGE|") {
//            std::string rest = line.substr(6);
//            size_t pos = rest.find('|');
//            if (pos != std::string::npos) {
//                data.stage_width = std::stoi(rest.substr(0, pos));
//                data.stage_height = std::stoi(rest.substr(pos + 1));
//            }
//        }
//        else if (line.substr(0, 9) == "BACKDROP|") {
//            data.backdrop_path = line.substr(9);
//        }
//        else if (line == "SPRITES_BEGIN") {
//            while (std::getline(in, line)) {
//                if (line == "SPRITES_END") break;
//                if (line == "SPRITE_BEGIN") {
//                    std::string sprite_line;
//                    std::getline(in, sprite_line);
//                    Sprite s = sprite_deserialize(sprite_line);
//
//                    /* Costumes */
//                    std::getline(in, line);
//                    while (std::getline(in, line)) {
//                        if (line == "COSTUMES_END") break;
//                        std::vector<std::string> parts;
//                        std::stringstream ss(line);
//                        std::string tok;
//                        while (std::getline(ss, tok, '|')) parts.push_back(tok);
//                        if (parts.size() >= 4) {
//                            Costume c;
//                            c.name = parts[0];
//                            c.filepath = parts[1];
//                            c.width = std::stoi(parts[2]);
//                            c.height = std::stoi(parts[3]);
//                            c.texture_ptr = nullptr;
//                            s.costumes.push_back(c);
//                        }
//                    }
//
//                    /* Blocks */
//                    std::getline(in, line);
//                    while (std::getline(in, line)) {
//                        if (line == "BLOCKS_END") break;
//                        Block b = block_deserialize(line);
//                        s.blocks.push_back(b);
//                    }
//
//                    /* Variables */
//                    std::getline(in, line);
//                    while (std::getline(in, line)) {
//                        if (line == "VARS_END") break;
//                        size_t pos = line.find('|');
//                        if (pos != std::string::npos) {
//                            std::string key = line.substr(0, pos);
//                            float val = std::stof(line.substr(pos + 1));
//                            s.variables[key] = val;
//                        }
//                    }
//
//                    std::getline(in, line);
//                    data.sprites.push_back(s);
//                }
//            }
//        }
//        else if (line == "GLOBAL_BLOCKS_BEGIN") {
//            while (std::getline(in, line)) {
//                if (line == "GLOBAL_BLOCKS_END") break;
//                Block b = block_deserialize(line);
//                data.global_blocks.push_back(b);
//            }
//        }
//        else if (line == "END_PROJECT") {
//            break;
//        }
//    }
//
//    in.close();
//    return true;
//}
//
//std::string file_get_extension(const std::string& filepath) {
//    size_t pos = filepath.rfind('.');
//    if (pos == std::string::npos) return "";
//    return filepath.substr(pos);
//}
//
//bool file_exists(const std::string& filepath) {
//    std::ifstream f(filepath);
//    return f.good();
//}
