//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_DATA_3D_H
#define BEDROCK_LEVEL_DATA_3D_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include "bedrock_key.h"
#include "magic-enum/magic_enum.hpp"
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
        //1.21
        pale_garden                      = 193,
        none                             = 255,
    };
    // clang-format on

}  // namespace bl

namespace magic_enum::customize {
    template <>
    struct enum_range<bl::biome> {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
}  // namespace magic_enum::customize

namespace bl {

    class biome3d {
       public:
        // Height map entry for a position with no height/biome record. height()
        // shifts the raw value by the dimension's min_y, so this stays unshifted.
        static constexpr int16_t INVALID_HEIGHT = 0xFFFF;

        // Raw height map value the game stores for a column that holds no block at all.
        static constexpr int16_t VOID_HEIGHT = -128;

        bool load_from_d3d(const byte_t* data, size_t len);

        bool load_from_d2d(const byte_t* data, size_t len);

        inline int height(int x, int z) {
            // Data2D stores no Y anchor (single layer); only the Data3D path shifts.
            const int my = this->use_3d_biome_maps_ ? dimension_min_y(this->pos_.dim) : 0;
            return this->height_map_[x + z * 16] + my;
        }

        [[nodiscard]] inline std::array<int16_t, 256> height_map() const { return this->height_map_; }

        biome get_biome(int cx, int y, int cz);

        std::vector<std::vector<bl::biome>> get_biome_y(int y);

        biome get_top_biome(int cx, int cz);

        void set_chunk_pos(const bl::chunk_pos& cp) { this->pos_ = cp; }

        void set_all(biome b);

        /// True when the payload is the 3D (Data3D) layout, false for the legacy 2D one.
        [[nodiscard]] inline bool is_3d() const { return this->use_3d_biome_maps_; }

        /// Records the world Y of the highest block of a column, in the same encoding
        /// height() reads back.
        void set_height(int x, int z, int world_y);

        /// Marks a column as holding no block; see VOID_HEIGHT.
        void set_void_height(int x, int z) { this->height_map_[x + z * 16] = VOID_HEIGHT; }

        [[nodiscard]] std::string to_raw() const;

       private:
        static constexpr std::array<int16_t, 256> make_invalid_height_map() {
            std::array<int16_t, 256> map{};
            for (auto& h : map) h = INVALID_HEIGHT;
            return map;
        }

        std::array<int16_t, 256> height_map_ = make_invalid_height_map();
        // one 16x16 biome layer per y slice, indexed [layer][x*16+z]
        std::vector<std::array<biome, 256>> biomes_;
        bl::chunk_pos pos_;
        // Which layout the payload uses, set by load_from_d3d / load_from_d2d. This is a
        // property of the payload itself: a 1.18+ chunk can still carry legacy Data2D biomes.
        bool use_3d_biome_maps_{true};
    };
}  // namespace bl

#endif  // BEDROCK_LEVEL_DATA_3D_H
