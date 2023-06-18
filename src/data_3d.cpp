//
// Created by xhy on 2023/3/30.
//

#include "data_3d.h"

#include <cstdio>
#include <memory>

#include "utils.h"

namespace bl {

    bool data_3d::load(const byte_t *data, size_t len) {
        if (len < 512) {
            BL_LOGGER("Invalid Data3d format");
            return false;
        }
        memcpy(this->height_map_.data(), data, 512);
        return true;
    }
    
    void data_3d::dump_to_file(FILE *fp) const {
        for (int i = 0; i < 256; i++) {
            fprintf(fp, "%03d ", this->height_map_[i] - 64);
            if (i % 16 == 15) {
                fprintf(fp, "\n");
            }
        }
    }

}  // namespace bl
