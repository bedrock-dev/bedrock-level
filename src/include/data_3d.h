//
// Created by xhy on 2023/3/30.
//

#ifndef BEDROCK_LEVEL_DATA_3D_H
#define BEDROCK_LEVEL_DATA_3D_H


#include <array>
#include <cstdint>

namespace bl {


    class data_3d {

    public:

        bool load(const uint8_t *data, size_t len);

    private:

        std::array<int16_t, 256> height_map_;
        int biome_info_; //TODO
    };
}


#endif //BEDROCK_LEVEL_DATA_3D_H
