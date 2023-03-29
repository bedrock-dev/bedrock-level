//
// Created by xhy on 2023/3/29.
//

#include "sub_chunk.h"
#include "bitstream.h"
#include <cstdio>
#include "utils.h"

namespace bl {

    namespace {

        bool read_header(sub_chunk *sub_chunk, const uint8_t *stream, int &read) {
            if (!sub_chunk || !stream) return false;
            //assert that stream is long enough
            sub_chunk->set_version(stream[0]);
            sub_chunk->set_layers_num(stream[1]);
            sub_chunk->set_y_index(stream[2]);
            read = 3;
            return true;
        }

        bool read_one_layer(sub_chunk *sub_chunk, const uint8_t *stream, int &read) {
            if (!sub_chunk || !stream) return false;
            //TODO: read block data and palette

            return true;
        }
    }


    bool sub_chunk::load(const uint8_t *data, size_t len) {
        size_t idx = 0;

        int read{0};
        if (!read_header(this, data, read)) {
            return false;
        }
        idx += read;
        for (int i = 0; i < this->layers_num_; i++) {
            if (!read_one_layer(this, data + idx, read)) {
                BL_ERROR("can not read layer %d", i);
                return false;
            }
            idx += read;
        }
        return true;
    }

    void sub_chunk::dump_to_file(FILE *fp) const {
        fprintf(fp, "Version : %u\n", this->version_);
        fprintf(fp, "Y index : %u\n", this->y_index_);
        fprintf(fp, "Layers  : %u\n", this->layers_num_);
    }
}
