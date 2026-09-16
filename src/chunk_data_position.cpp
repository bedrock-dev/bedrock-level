//
// Created by xhy on 2026/9/17.
//

#include "chunk_data_position.h"

#include "nbt.h"

namespace bl {

    namespace {
        // get() may miss, so the cast only happens on a hit; as<>() on a null tag is not safe.
        nbt::int_tag* get_int_tag(nbt::compound_tag* tag, const char* key) {
            auto* value = tag->get(key);
            return value ? value->as<nbt::int_tag*>() : nullptr;
        }

        const nbt::int_tag* peek_int_tag(const nbt::compound_tag* tag, const char* key) {
            const auto* value = tag->get(key);
            return value ? value->as<const nbt::int_tag*>() : nullptr;
        }

        void offset_int_tag(nbt::compound_tag* tag, const char* key, int offset) {
            if (auto* value = get_int_tag(tag, key); value) {
                value->value += offset;
            }
        }

        void set_int_tag(nbt::compound_tag* tag, const char* key, int value) {
            if (auto* current = get_int_tag(tag, key); current) {
                current->value = value;
                return;
            }
            tag->remove(key);
            tag->put(new nbt::int_tag(key, value));
        }
    }  // namespace

    bool read_block_entity_pos(const nbt::compound_tag* block_entity, block_pos& out) {
        if (!block_entity) return false;
        const auto* x = peek_int_tag(block_entity, "x");
        const auto* y = peek_int_tag(block_entity, "y");
        const auto* z = peek_int_tag(block_entity, "z");
        if (!x || !y || !z) return false;
        out = block_pos{x->value, y->value, z->value};
        return true;
    }

    void set_block_entity_pos(nbt::compound_tag* block_entity, const block_pos& pos) {
        if (!block_entity) return;

        const auto* id_tag = block_entity->get("id");
        const auto* id = id_tag ? id_tag->as<const nbt::string_tag*>() : nullptr;
        if (id && id->value == "Chest") {
            const auto* x = peek_int_tag(block_entity, "x");
            const auto* z = peek_int_tag(block_entity, "z");
            if (x) offset_int_tag(block_entity, "pairx", pos.x - x->value);
            if (z) offset_int_tag(block_entity, "pairz", pos.z - z->value);
        }

        set_int_tag(block_entity, "x", pos.x);
        set_int_tag(block_entity, "y", pos.y);
        set_int_tag(block_entity, "z", pos.z);
    }

    void offset_pending_ticks_pos(nbt::compound_tag*& pending_ticks, int dx, int dz) {
        if (!pending_ticks) return;

        const auto* list_tag = pending_ticks->get("tickList");
        auto* tick_list = list_tag ? list_tag->as<nbt::list_tag*>() : nullptr;
        if (!tick_list) return;

        for (auto* item : tick_list->value) {
            auto* tick = item ? item->as<nbt::compound_tag*>() : nullptr;
            if (!tick) continue;
            offset_int_tag(tick, "x", dx);
            offset_int_tag(tick, "z", dz);
        }
    }

    void offset_hardcoded_spawn_areas_pos(hardcoded_spawn_area_list& list, int dx, int dz) {
        for (auto& area : list.areas()) {
            area.min_pos.x += dx;
            area.min_pos.z += dz;
            area.max_pos.x += dx;
            area.max_pos.z += dz;
        }
    }

}  // namespace bl
