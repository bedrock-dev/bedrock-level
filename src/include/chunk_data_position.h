#ifndef BEDROCK_LEVEL_CHUNK_DATA_POSITION_H
#define BEDROCK_LEVEL_CHUNK_DATA_POSITION_H

#include "bedrock_key.h"
#include "nbt.h"

namespace bl {

    namespace chunk_data_position_detail {
        inline void offset_int_tag(nbt::compound_tag *tag, const char *key, int offset) {
            if (auto *value = dynamic_cast<nbt::int_tag *>(tag->get(key)); value) {
                value->value += offset;
            }
        }

        inline void set_int_tag(nbt::compound_tag *tag, const char *key, int value) {
            if (auto *current = dynamic_cast<nbt::int_tag *>(tag->get(key)); current) {
                current->value = value;
                return;
            }
            tag->remove(key);
            tag->put(new nbt::int_tag(key, value));
        }
    }  // namespace chunk_data_position_detail

    inline void offset_block_entity_pos(nbt::compound_tag *&block_entity, int dx, int dz) {
        if (!block_entity) return;

        chunk_data_position_detail::offset_int_tag(block_entity, "x", dx);
        chunk_data_position_detail::offset_int_tag(block_entity, "z", dz);

        const auto *id = dynamic_cast<nbt::string_tag *>(block_entity->get("id"));
        if (id && id->value == "Chest") {
            chunk_data_position_detail::offset_int_tag(block_entity, "pairx", dx);
            chunk_data_position_detail::offset_int_tag(block_entity, "pairz", dz);
        }
    }

    inline void set_block_entity_pos(nbt::compound_tag *block_entity, const block_pos &pos) {
        if (!block_entity) return;

        const auto *x = dynamic_cast<nbt::int_tag *>(block_entity->get("x"));
        const auto *z = dynamic_cast<nbt::int_tag *>(block_entity->get("z"));
        const int dx = x ? pos.x - x->value : 0;
        const int dz = z ? pos.z - z->value : 0;
        offset_block_entity_pos(block_entity, dx, dz);

        chunk_data_position_detail::set_int_tag(block_entity, "x", pos.x);
        chunk_data_position_detail::set_int_tag(block_entity, "y", pos.y);
        chunk_data_position_detail::set_int_tag(block_entity, "z", pos.z);
    }

    inline void offset_pending_ticks_pos(nbt::compound_tag *&pending_ticks, int dx, int dz) {
        if (!pending_ticks) return;

        auto *tick_list = dynamic_cast<nbt::list_tag *>(pending_ticks->get("tickList"));
        if (!tick_list) return;

        for (auto *item : tick_list->value) {
            auto *tick = dynamic_cast<nbt::compound_tag *>(item);
            if (!tick) continue;
            chunk_data_position_detail::offset_int_tag(tick, "x", dx);
            chunk_data_position_detail::offset_int_tag(tick, "z", dz);
        }
    }

    inline void offset_hardcoded_spawn_areas_pos(hardcoded_spawn_area_list &list, int dx, int dz) {
        for (auto &area : list.areas()) {
            area.min_pos.x += dx;
            area.min_pos.z += dz;
            area.max_pos.x += dx;
            area.max_pos.z += dz;
        }
    }

}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_DATA_POSITION_H
