//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_DATA_3D_H
#define BEDROCK_LEVEL_DATA_3D_H

#include <array>
#include <cstdint>
#include <cstdio>

#include "bedrock_key.h"
#include "utils.h"
namespace bl {

    // clang-format off

    enum biome  : uint8_t {
        ocean                            = 0,
        plains                           = 1,
        desert                           = 2,
        extreme_hills                    = 3,
        forest                           = 4,
        taiga                            = 5,
        swampland                        = 6,
        river                            = 7,
        hell                             = 8,
        the_end                          = 9,
        legacy_frozen_ocean              = 10,
        frozen_river                     = 11,
        ice_plains                       = 12,
        ice_mountains                    = 13,
        mushroom_island                  = 14,
        mushroom_island_shore            = 15,
        beach                            = 16,
        desert_hills                     = 17,
        forest_hills                     = 18,
        taiga_hills                      = 19,
        extreme_hills_edge               = 20,
        jungle                           = 21,
        jungle_hills                     = 22,
        jungle_edge                      = 23,
        deep_ocean                       = 24,
        stone_beach                      = 25,
        cold_beach                       = 26,
        birch_forest                     = 27,
        birch_forest_hills               = 28,
        roofed_forest                    = 29,
        cold_taiga                       = 30,
        cold_taiga_hills                 = 31,
        mega_taiga                       = 32,
        mega_taiga_hills                 = 33,
        extreme_hills_plus_trees         = 34,
        savanna                          = 35,
        savanna_plateau                  = 36,
        mesa                             = 37,
        mesa_plateau_stone               = 38,
        mesa_plateau                     = 39,
        warm_ocean                       = 40,
        deep_warm_ocean                  = 41,
        lukewarm_ocean                   = 42,
        deep_lukewarm_ocean              = 43,
        cold_ocean                       = 44,
        deep_cold_ocean                  = 45,
        frozen_ocean                     = 46,
        deep_frozen_ocean                = 47,
        bamboo_jungle                    = 48,
        bamboo_jungle_hills              = 49,
        sunflower_plains                 = 129,
        desert_mutated                   = 130,
        extreme_hills_mutated            = 131,
        flower_forest                    = 132,
        taiga_mutated                    = 133,
        swampland_mutated                = 134,
        ice_plains_spikes                = 140,
        jungle_mutated                   = 149,
        jungle_edge_mutated              = 151,
        birch_forest_mutated             = 155,
        birch_forest_hills_mutated       = 156,
        roofed_forest_mutated            = 157,
        cold_taiga_mutated               = 158,
        redwood_taiga_mutated            = 160,
        redwood_taiga_hills_mutated      = 161,
        extreme_hills_plus_trees_mutated = 162,
        savanna_mutated                  = 163,
        savanna_plateau_mutated          = 164,
        mesa_bryce                       = 165,
        mesa_plateau_stone_mutated       = 166,
        mesa_plateau_mutated             = 167,
        soulsand_valley                  = 178,
        crimson_forest                   = 179,
        warped_forest                    = 180,
        basalt_deltas                    = 181,
        lofty_peaks                      = 182,
        snow_capped_peaks                = 183,
        snowy_slopes                     = 184,
        mountain_grove                   = 185,
        mountain_meadow                  = 186,
        lush_caves                       = 187,
        dripstone_caves                  = 188,
        stony_peaks                      = 189,
        deep_dark                        = 190,
        mangrove_swamp                   = 191,
        cherry_groves                    = 192,
        none                             = 255,
    };
    // clang-format on

    class biome3d {
       public:
        bool load_from_d3d(const byte_t *data, size_t len);

        bool load_from_d2d(const byte_t *data, size_t len);

        inline int height(int x, int z) {
            auto [my, _] = this->pos_.get_y_range(this->version_);
            return this->height_map_[x + z * 16] + my;
        }

        [[nodiscard]] inline std::array<int16_t, 256> height_map() const {
            return this->height_map_;
        }

        biome get_biome(int cx, int y, int cz);

        std::vector<std::vector<bl::biome>> get_biome_y(int y);

        biome get_top_biome(int cx, int cz);

        void set_chunk_pos(const bl::chunk_pos &cp) { this->pos_ = cp; }
        void set_version(ChunkVersion version) { this->version_ = version; }

       private:
        std::array<int16_t, 256> height_map_;
        std::vector<std::vector<std::vector<biome>>> biomes_;
        bl::chunk_pos pos_;
        ChunkVersion version_;
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_DATA_3D_H
