//
// Created by xhy on 2023/3/30.
//

#include "data_3d.h"
#include <memory>
#include "utils.h"

namespace bl {

    bool data_3d::load(const uint8_t *data, size_t len) {
        if (len < 512) {
            BL_LOGGER("Invalid Data3d format");
            return false;
        }
        memcpy(this->height_map_.data(), data, 512);
        return true;
    }
}

