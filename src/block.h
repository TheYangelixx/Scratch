#ifndef MYSCRATCH_BLOCK_H
#define MYSCRATCH_BLOCK_H

#include <string>
#include <vector>
#include <functional>

// ============================================================
//  دسته‌بندی بلوک‌ها
// ============================================================
enum BlockCategory {
    CAT_MOTION = 0,
    CAT_LOOKS,
    CAT_SOUND,
    CAT_EVENTS,
    CAT_CONTROL,
    CAT_SENSING,
    CAT_OPERATORS,
    CAT_VARIABLES,
    CAT_PEN,
    CAT_COUNT           // تعداد کل دسته‌بندی‌ها
};

// ============================================================
//  نوع شکل بلوک
// ============================================================
enum BlockType {
    BTYPE_STACK = 0,    // بلوک معمولی (مستطیل با زبانه)
    BTYPE_HAT,          // بلوک شروع (سر گرد)
    BTYPE_CAP,          // بلوک پایانی
    BTYPE_REPORTER,     // بلوک مقدار‌دهنده (بیضی)
    BTYPE_BOOLEAN,      // بلوک بولی (شش‌ضلعی)
    BTYPE_C_BLOCK,      // بلوک C شکل (if / repeat)
    BTYPE_DEFINE        // تعریف بلوک سفارشی
};

// ============================================================
//  فیلد ورودی بلوک
// ============================================================
enum FieldType {
    FIELD_NUMBER = 0,
    FIELD_STRING,
    FIELD_DROPDOWN,
    FIELD_COLOR,
    FIELD_VARIABLE,
    FIELD_BOOLEAN_SLOT
};

struct BlockField {
    std::string name;           // نام فیلد (مثلاً "steps", "message")
    FieldType   type = FIELD_NUMBER;
    std::string value;          // مقدار فعلی (به صورت رشته)
    std::string default_value;  // مقدار پیش‌فرض
    std::vector<std::string> dropdown_options; // گزینه‌های کشویی

    // محدوده عددی (اختیاری)
    float min_val = -99999.0f;
    float max_val =  99999.0f;

    // مستطیل رسم (برای کلیک)
    int render_x = 0, render_y = 0, render_w = 0, render_h = 0;
};

// ============================================================
//  ساختار بلوک
// ============================================================
struct Block {
    int             id = -1;                // شناسه یکتا
    std::string     opcode;                 // کد عملیات (مثلاً "motion_move")
    std::string     text;                   // متن نمایشی (مثلاً "move %1 steps")
    BlockCategory   category = CAT_MOTION;
    BlockType       block_type = BTYPE_STACK;
    std::vector<BlockField> fields;         // فیلدهای ورودی

    // موقعیت رسم در ناحیه اسکریپت
    int x = 0, y = 0;
    int width = 150, height = 40;

    // اتصال‌ها
    int next_block_id = -1;                 // بلوک بعدی (پایین)
    int prev_block_id = -1;                 // بلوک قبلی (بالا)
    int parent_id     = -1;                 // بلوک والد (C-block)

    // بلوک‌های داخلی C-block
    std::vector<int> body_block_ids;        // شاخه اول
    std::vector<int> else_block_ids;        // شاخه دوم (else)

    // وضعیت
    bool is_shadow    = false;              // بلوک سایه (reporter جاسازی‌شده)
    bool is_top_level = false;              // بلوک سطح بالا
    bool disabled     = false;              // غیرفعال
    bool highlighted  = false;              // هایلایت شده

    // رنگ سفارشی (اگر صفر باشد از رنگ دسته‌بندی استفاده می‌شود)
    unsigned char custom_r = 0, custom_g = 0, custom_b = 0;

    // توضیح (Comment)
    std::string comment;
};

// ============================================================
//  توابع ساخت و مدیریت بلوک
// ============================================================

// ساختن یک بلوک جدید با شناسه یکتا
Block block_create(const std::string& opcode, const std::string& text,
                   BlockCategory cat, BlockType btype);

// ساختن یک بلوک با یک فیلد عددی
Block block_create_with_field(const std::string& opcode, const std::string& text,
                              BlockCategory cat, BlockType btype,
                              const std::string& field_name, const std::string& default_val);

// ساختن یک بلوک با یک فیلد رشته‌ای
Block block_create_with_string_field(const std::string& opcode, const std::string& text,
                                     BlockCategory cat, BlockType btype,
                                     const std::string& field_name, const std::string& default_val);

// ساختن یک بلوک با فیلد کشویی
Block block_create_with_dropdown(const std::string& opcode, const std::string& text,
                                 BlockCategory cat, BlockType btype,
                                 const std::string& field_name,
                                 const std::vector<std::string>& options,
                                 const std::string& default_val);

// کپی عمیق یک بلوک (با شناسه جدید)
Block block_clone(const Block& src);

// محاسبه ارتفاع بلوک (شامل بدنه C-block)
int block_calc_height(const Block& b);

// بررسی قابلیت اتصال دو بلوک
bool block_can_connect(const Block& top, const Block& bottom);

// دریافت مقدار یک فیلد به صورت float
float block_get_field_float(const Block& b, int field_index, float fallback = 0.0f);

// دریافت مقدار یک فیلد به صورت string
std::string block_get_field_string(const Block& b, int field_index,
                                   const std::string& fallback = "");

#endif // MYSCRATCH_BLOCK_H
