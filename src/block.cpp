#include "block.h"
#include <stdexcept>

// ============================================================
//  شمارنده جهانی برای تولید شناسه یکتا
// ============================================================
static int g_next_block_id = 1;

// ============================================================
//  ساختن یک بلوک ساده
// ============================================================
Block block_create(const std::string& opcode, const std::string& text,
                   BlockCategory cat, BlockType btype)
{
    Block b;
    b.id         = g_next_block_id++;
    b.opcode     = opcode;
    b.text       = text;
    b.category   = cat;
    b.block_type = btype;
    return b;
}

// ============================================================
//  ساختن یک بلوک با یک فیلد عددی
// ============================================================
Block block_create_with_field(const std::string& opcode, const std::string& text,
                              BlockCategory cat, BlockType btype,
                              const std::string& field_name, const std::string& default_val)
{
    Block b = block_create(opcode, text, cat, btype);
    BlockField f;
    f.name          = field_name;
    f.type          = FIELD_NUMBER;
    f.default_value = default_val;
    f.value         = default_val;
    b.fields.push_back(f);
    return b;
}

// ============================================================
//  ساختن یک بلوک با یک فیلد رشته‌ای
// ============================================================
Block block_create_with_string_field(const std::string& opcode, const std::string& text,
                                     BlockCategory cat, BlockType btype,
                                     const std::string& field_name, const std::string& default_val)
{
    Block b = block_create(opcode, text, cat, btype);
    BlockField f;
    f.name          = field_name;
    f.type          = FIELD_STRING;
    f.default_value = default_val;
    f.value         = default_val;
    b.fields.push_back(f);
    return b;
}

// ============================================================
//  ساختن یک بلوک با فیلد کشویی (dropdown)
// ============================================================
Block block_create_with_dropdown(const std::string& opcode, const std::string& text,
                                 BlockCategory cat, BlockType btype,
                                 const std::string& field_name,
                                 const std::vector<std::string>& options,
                                 const std::string& default_val)
{
    Block b = block_create(opcode, text, cat, btype);
    BlockField f;
    f.name             = field_name;
    f.type             = FIELD_DROPDOWN;
    f.dropdown_options = options;
    f.default_value    = default_val;
    f.value            = default_val;
    b.fields.push_back(f);
    return b;
}

// ============================================================
//  کپی عمیق یک بلوک (با شناسه جدید)
// ============================================================
Block block_clone(const Block& src)
{
    Block b  = src;
    b.id     = g_next_block_id++;
    b.next_block_id = -1;
    b.prev_block_id = -1;
    b.parent_id     = -1;
    b.body_block_ids.clear();
    b.else_block_ids.clear();
    return b;
}

// ============================================================
//  محاسبه ارتفاع بلوک
// ============================================================
int block_calc_height(const Block& b)
{
    if (b.block_type == BTYPE_C_BLOCK) {
        int inner = (int)b.body_block_ids.size() * 44 + 20;
        return 40 + inner;
    }
    return 40;
}

// ============================================================
//  بررسی قابلیت اتصال دو بلوک
// ============================================================
bool block_can_connect(const Block& top, const Block& bottom)
{
    if (bottom.block_type == BTYPE_HAT)    return false;
    if (top.block_type    == BTYPE_CAP)    return false;
    if (top.block_type    == BTYPE_REPORTER || top.block_type    == BTYPE_BOOLEAN) return false;
    if (bottom.block_type == BTYPE_REPORTER || bottom.block_type == BTYPE_BOOLEAN) return false;
    return true;
}

// ============================================================
//  دریافت مقدار یک فیلد به صورت float
// ============================================================
float block_get_field_float(const Block& b, int field_index, float fallback)
{
    if (field_index < 0 || field_index >= (int)b.fields.size()) return fallback;
    const std::string& val = b.fields[field_index].value;
    if (val.empty()) return fallback;
    try { return std::stof(val); }
    catch (...) { return fallback; }
}

// ============================================================
//  دریافت مقدار یک فیلد به صورت string
// ============================================================
std::string block_get_field_string(const Block& b, int field_index,
                                   const std::string& fallback)
{
    if (field_index < 0 || field_index >= (int)b.fields.size()) return fallback;
    const std::string& val = b.fields[field_index].value;
    return val.empty() ? fallback : val;
}
