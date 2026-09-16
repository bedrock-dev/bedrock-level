#ifndef BEDROCK_LEVEL_CHUNK_DATA_POSITION_H
#define BEDROCK_LEVEL_CHUNK_DATA_POSITION_H

#include "bedrock_key.h"

namespace bl {

    namespace nbt {
        class compound_tag;
    }  // namespace nbt

    /// World position held in the block entity's x/y/z; false when one of them is missing.
    bool read_block_entity_pos(const nbt::compound_tag* block_entity, block_pos& out);

    /// Moves a block entity to a world position. A chest is stored as two block entities that
    /// point at each other through pairx/pairz, so the two have to move together.
    void set_block_entity_pos(nbt::compound_tag* block_entity, const block_pos& pos);

    void offset_pending_ticks_pos(nbt::compound_tag*& pending_ticks, int dx, int dz);

    void offset_hardcoded_spawn_areas_pos(hardcoded_spawn_area_list& list, int dx, int dz);

}  // namespace bl

#endif  // BEDROCK_LEVEL_CHUNK_DATA_POSITION_H
