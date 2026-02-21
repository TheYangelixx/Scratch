#include "block_manager.h"
#include "logger.h"

// نمونه سراسری
static BlockManager g_bm;

BlockManager& bm_global() {
    return g_bm;
}

int BlockManager::add(const Block& b) {
    blocks[b.id] = b;
    return b.id;
}

Block* BlockManager::get(int id) {
    auto it = blocks.find(id);
    if (it != blocks.end()) return &it->second;
    return nullptr;
}

const Block* BlockManager::get(int id) const {
    auto it = blocks.find(id);
    if (it != blocks.end()) return &it->second;
    return nullptr;
}

void BlockManager::remove(int id) {
    auto it = blocks.find(id);
    if (it != blocks.end()) {
        // قطع اتصال‌ها
        Block& b = it->second;
        if (b.prev_block_id != -1) {
            Block* prev = get(b.prev_block_id);
            if (prev && prev->next_block_id == id) {
                prev->next_block_id = -1;
            }
        }
        if (b.next_block_id != -1) {
            Block* next = get(b.next_block_id);
            if (next && next->prev_block_id == id) {
                next->prev_block_id = -1;
            }
        }
        blocks.erase(it);
    }
}

void BlockManager::clear() {
    blocks.clear();
}

int BlockManager::count() const {
    return (int)blocks.size();
}

bool BlockManager::connect(int top_id, int bottom_id) {
    Block* top = get(top_id);
    Block* bot = get(bottom_id);
    if (!top || !bot) return false;

    if (!block_can_connect(*top, *bot)) return false;

    // اگر top قبلاً next داشت، قطعش کن
    if (top->next_block_id != -1) {
        Block* old_next = get(top->next_block_id);
        if (old_next) old_next->prev_block_id = -1;
    }
    // اگر bot قبلاً prev داشت، قطعش کن
    if (bot->prev_block_id != -1) {
        Block* old_prev = get(bot->prev_block_id);
        if (old_prev) old_prev->next_block_id = -1;
    }

    top->next_block_id = bottom_id;
    bot->prev_block_id = top_id;
    return true;
}

void BlockManager::disconnect(int block_id) {
    Block* b = get(block_id);
    if (!b) return;

    if (b->prev_block_id != -1) {
        Block* prev = get(b->prev_block_id);
        if (prev && prev->next_block_id == block_id) {
            prev->next_block_id = -1;
        }
        b->prev_block_id = -1;
    }
}

std::vector<int> BlockManager::get_chain(int start_id) const {
    std::vector<int> chain;
    int current = start_id;
    int safety = 10000;
    while (current != -1 && safety-- > 0) {
        chain.push_back(current);
        const Block* b = get(current);
        if (!b) break;
        current = b->next_block_id;
    }
    return chain;
}

int BlockManager::get_chain_top(int block_id) const {
    int current = block_id;
    int safety = 10000;
    while (safety-- > 0) {
        const Block* b = get(current);
        if (!b || b->prev_block_id == -1) return current;
        current = b->prev_block_id;
    }
    return current;
}

std::vector<int> BlockManager::clone_chain(int start_id) {
    std::vector<int> original_chain = get_chain(start_id);
    std::vector<int> new_ids;

    // ابتدا کلون‌ها را بساز
    for (int oid : original_chain) {
        const Block* ob = get(oid);
        if (!ob) continue;
        Block nb = block_clone(*ob);
        add(nb);
        new_ids.push_back(nb.id);
    }

    // اتصال زنجیره جدید
    for (int i = 0; i + 1 < (int)new_ids.size(); i++) {
        connect(new_ids[i], new_ids[i + 1]);
    }

    return new_ids;
}

std::vector<int> BlockManager::all_ids() const {
    std::vector<int> ids;
    ids.reserve(blocks.size());
    for (auto& pair : blocks) {
        ids.push_back(pair.first);
    }
    return ids;
}
