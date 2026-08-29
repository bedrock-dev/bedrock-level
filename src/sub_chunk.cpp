//
// Created by xhy on 2023/3/29.
//

#include "sub_chunk.h"

#include <cstdio>

#include "utils.h"

// #include "nbt.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "color.h"
#include "nbt.h"
#include "palette.h"

namespace bl {

    namespace {

        // sub chunk layout
        // https://user-images.githubusercontent.com/13713600/148380033-6223ac76-54b7-472c-a355-5923b87cb7c5.png

        bool read_header(sub_chunk *sub_chunk, const byte_t *stream, int &read) {
            if (!sub_chunk || !stream) return false;
            // assert that stream is long enough
            auto version = stream[0];
            if (version != 8        // 1.2~1.17
                && version != 9) {  // 1.18+
                LOG_F(INFO, "Unsupported sub chunk version: %u", stream[0]);
                return false;
            }
            sub_chunk->set_version(version);
            sub_chunk->set_layers_num(stream[1]);
            read = 2;
            // y-index for version 9
            if (version == 9) {
                int8_t y_index = stream[2];
                if (y_index != sub_chunk->y_index()) {
                    LOG_F(ERROR, "Invalid Y index value(new(%d)  != default(%d))", y_index, sub_chunk->y_index());
                }
                sub_chunk->set_y_index(y_index);
                read++;
            }
            return true;
        }
    }  // namespace

    bool sub_chunk::load(const byte_t *data, size_t len) {
        size_t idx = 0;
        int read{0};
        if (!read_header(this, data, read)) return false;
        idx += read;
        for (auto i = 0; i < (int)this->layers_num_; i++) {
            auto *layer = new bl::sub_chunk::layer();
            this->layers_.push_back(layer);
            layer->blocks = bl::read_block_indices(data + idx, read, layer->bits, layer->palette_len);
            idx += read;
            layer->palette = bl::read_palettes(data + idx, layer->palette_len, len - idx, read);
            idx += read;
        }
        return true;
    }

    void sub_chunk::dump_to_file(FILE *fp) const {}

    block_info sub_chunk::get_block(int rx, int ry, int rz) {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            LOG_F(ERROR, "Invalid in chunk position %d %d %d", rx, ry, rz);
            return {};
        }
        if (this->layers_.empty()) {
            LOG_F(ERROR, "sub_chunk has no layers");
            return {};
        }
        using bl::nbt::string_tag, bl::nbt::compound_tag;

        auto idx = ry + rz * 16 + rx * 256;
        auto block = this->layers_[0]->blocks[idx];

        auto &palette = this->layers_[0]->palette;
        if (block < 0 || block >= palette.size()) {
            LOG_F(ERROR, "Invalid block index with value %d", block);
            return {};
        }
        auto &b = palette[block].tag;

        std::string extra_tag;
        std::string name = palette[block].name;

        // states/color may be absent on simple blocks, guard each level
        if (auto *stat_tag = b->get("states"); stat_tag) {
            if (auto *st = stat_tag->as<compound_tag *>(); st) {
                if (auto *color_tag = st->get("color"); color_tag) {
                    if (auto *ct = color_tag->as<string_tag *>(); ct) {
                        extra_tag = ct->value;
                    }
                }
            }
        }
        return {name, bl::get_block_by_name_tag(name, extra_tag)};
    }

    const std::string &sub_chunk::get_block_name(int rx, int ry, int rz) {
        static const std::string unknown = "minecraft:unknown";
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15 || this->layers_.empty()) {
            return unknown;
        }

        auto idx = ry + rz * 16 + rx * 256;
        auto block = this->layers_[0]->blocks[idx];
        auto &palette = this->layers_[0]->palette;
        if (block < 0 || block >= static_cast<int>(palette.size())) {
            return unknown;
        }
        return palette[block].name;
    }

    block_info sub_chunk::get_block_fast(int rx, int ry, int rz) {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            LOG_F(ERROR, "Invalid in chunk position %d %d %d", rx, ry, rz);
            return {};
        }
        if (this->layers_.empty()) {
            LOG_F(ERROR, "sub_chunk has no layers");
            return {};
        }

        auto idx = ry + rz * 16 + rx * 256;
        auto block = this->layers_[0]->blocks[idx];

        auto &palette = this->layers_[0]->palette;
        if (block >= palette.size() || block < 0) {
            LOG_F(ERROR, "Invalid block index with value %d", block);
            return {};
        }

        return {palette[block].name, bl::color{}};
    }

    nbt::compound_tag *sub_chunk::get_block_raw(int rx, int ry, int rz) {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            LOG_F(ERROR, "Invalid in chunk position %d %d %d", rx, ry, rz);
            return nullptr;
        }
        if (this->layers_.empty()) {
            LOG_F(ERROR, "sub_chunk has no layers");
            return nullptr;
        }

        auto idx = ry + rz * 16 + rx * 256;
        auto block = this->layers_[0]->blocks[idx];

        if (block >= this->layers_[0]->palette.size() || block < 0) {
            LOG_F(ERROR, "Invalid block index with value %d", block);
            return nullptr;
        }

        return this->layers_[0]->palette[block].tag;
    }
    sub_chunk::~sub_chunk() {
        for (auto &layer : this->layers_) {
            delete layer;
        }
    }

    sub_chunk::layer::~layer() {
        for (auto &entry : this->palette) delete entry.tag;
    }
}  // namespace bl
