//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_DATA_3D_H
#define BEDROCK_LEVEL_DATA_3D_H

#include <array>
#include <cstdint>
#include <cstdio>

#include "utils.h"

namespace bl {

    enum biome {
        plains = 1L,
        desert = 2L,
        extreme_hills = 3L,
        forest = 4L,
        taiga = 5L,
        swampland = 6L,
        river = 7L,
        hell = 8L,
        the_end = 9L,
        legacy_frozen_ocean = 10L,
        frozen_river = 11L,
        ice_plains = 12L,
        ice_mountains = 13L,
        mushroom_island = 14L,
        mushroom_island_shore = 15L,
        beach = 16L,
        desert_hills = 17L,
        forest_hills = 18L,
        taiga_hills = 19L,
        extreme_hills_edge = 20L,
        jungle = 21L,
        jungle_hills = 22L,
        jungle_edge = 23L,
        stone_beach = 25L,
        cold_beach = 26L,
        birch_forest = 27L,
        birch_forest_hills = 28L,
        roofed_forest = 29L,
        cold_taiga = 30L,
        cold_taiga_hills = 31L,
        mega_taiga = 32L,
        mega_taiga_hills = 33L,
        extreme_hills_plus_trees = 34L,
        savanna = 35L,
        savanna_plateau = 36L,
        mesa = 37L,
        mesa_plateau_stone = 38L,
        mesa_plateau = 39L,
        ocean = 0L,
        deep_ocean = 24L,
        warm_ocean = 40L,
        deep_warm_ocean = 41L,
        lukewarm_ocean = 42L,
        deep_lukewarm_ocean = 43L,
        cold_ocean = 44L,
        deep_cold_ocean = 45L,
        frozen_ocean = 46L,
        deep_frozen_ocean = 47L,
        bamboo_jungle = 48L,
        bamboo_jungle_hills = 49L,
        sunflower_plains = 129L,
        swampland_mutated = 134L,
        ice_plains_spikes = 140L,
        cold_taiga_mutated = 158L,
        savanna_mutated = 163L,
        savanna_plateau_mutated = 164L,
        roofed_forest_mutated = 157L,
        desert_mutated = 130L,
        flower_forest = 132L,
        taiga_mutated = 133L,
        jungle_mutated = 149L,
        jungle_edge_mutated = 151L,
        mesa_bryce = 165L,
        mesa_plateau_stone_mutated = 166L,
        mesa_plateau_mutated = 167L,
        birch_forest_mutated = 155L,
        birch_forest_hills_mutated = 156L,
        redwood_taiga_mutated = 160L,
        extreme_hills_mutated = 131L,
        extreme_hills_plus_trees_mutated = 162L,
        redwood_taiga_hills_mutated = 161L,
        soulsand_valley = 178L,
        crimson_forest = 179L,
        warped_forest = 180L,
        basalt_deltas = 181L,
        lofty_peaks = 182L,
        snow_capped_peaks = 183L,
        snowy_slopes = 184L,
        mountain_grove = 185L,
        mountain_meadow = 186L,
        lush_caves = 187L,
        dripstone_caves = 188L,
        stony_peaks = 189L,
        deep_dark = 190,
        mangrove_swamp = 191,
        cherry_groves = 192,
        LEN,
    };

    class data_3d {
       public:
        bool load(const byte_t *data, size_t len);
        void dump_to_file(FILE *fp) const;

        inline int height(int16_t x, int16_t z) {
            return this->height_map_[static_cast<size_t>((x << 4u) + z)] - int16_t{64};
        }

        [[nodiscard]] inline std::array<int16_t, 256> height_map() const {
            return this->height_map_;
        }

       private:
        std::array<int16_t, 256> height_map_;
        int biome_info_;  // TODO
    };

}  // namespace bl

#endif  // BEDROCK_LEVEL_DATA_3D_H
