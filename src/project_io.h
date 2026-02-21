#ifndef MYSCRATCH_PROJECT_IO_H
#define MYSCRATCH_PROJECT_IO_H

#include "app_state.h"
#include <string>

// ============================================================
//  ذخیره و بارگذاری پروژه
// ============================================================

bool project_save(const AppState& state, const std::string& filepath);
bool project_load(AppState& state, const std::string& filepath);

// خروجی JSON ساده (برای debug)
std::string project_to_json(const AppState& state);

#endif // MYSCRATCH_PROJECT_IO_H
