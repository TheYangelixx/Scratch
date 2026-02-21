#ifndef MYSCRATCH_BLOCK_MANAGER_H
#define MYSCRATCH_BLOCK_MANAGER_H

#include "block.h"
#include <vector>
#include <unordered_map>

// ============================================================
//  مدیریت مرکزی بلوک‌ها
//  همه بلوک‌های پروژه در یک مخزن مرکزی ذخیره می‌شوند
// ============================================================

struct BlockManager {
    std::unordered_map<int, Block> blocks;   // id -> Block

    // افزودن بلوک و دریافت id
    int add(const Block& b);

    // دریافت بلوک با id (اشاره‌گر - nullptr اگر نباشد)
    Block* get(int id);
    const Block* get(int id) const;

    // حذف بلوک با id
    void remove(int id);

    // حذف تمام بلوک‌ها
    void clear();

    // تعداد بلوک‌ها
    int count() const;

    // اتصال دو بلوک (top.next = bottom, bottom.prev = top)
    bool connect(int top_id, int bottom_id);

    // قطع اتصال یک بلوک از بلوک قبلی‌اش
    void disconnect(int block_id);

    // دریافت زنجیره بلوک‌ها از یک بلوک شروع (دنبال کردن next)
    std::vector<int> get_chain(int start_id) const;

    // دریافت بلوک ابتدایی زنجیره (بالا رفتن از prev)
    int get_chain_top(int block_id) const;

    // کپی یک زنجیره کامل
    std::vector<int> clone_chain(int start_id);

    // دریافت لیست تمام id ها
    std::vector<int> all_ids() const;
};

// تابع سراسری دسترسی به BlockManager
BlockManager& bm_global();

#endif // MYSCRATCH_BLOCK_MANAGER_H
