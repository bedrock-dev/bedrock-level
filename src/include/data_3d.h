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
