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

    const palette_entry *sub_chunk::palette_entry_at(int rx, int ry, int rz, int layer) const {
        if (rx < 0 || rx > 15 || ry < 0 || ry > 15 || rz < 0 || rz > 15) {
            LOG_F(ERROR, "Invalid in chunk position %d %d %d", rx, ry, rz);
            return nullptr;
        }
        if (layer < 0 || layer >= static_cast<int>(this->layers_.size())) {
            return nullptr;  // requested layer not present
        }
        auto &ly = *this->layers_[layer];
        auto idx = ry + rz * 16 + rx * 256;
        auto block = ly.blocks[idx];
        if (block >= ly.palette.size()) {
            LOG_F(ERROR, "Invalid block index with value %d", block);
            return nullptr;
        }
        return &ly.palette[block];
    }

    block_info sub_chunk::get_block_with_color(int rx, int ry, int rz, int layer) {
        const auto *entry = this->palette_entry_at(rx, ry, rz, layer);
        if (!entry) return {};

        using bl::nbt::string_tag, bl::nbt::compound_tag;
        std::string extra_tag;
        // states/color may be absent on simple blocks, guard each level
        if (auto *stat_tag = entry->tag->get("states"); stat_tag) {
            if (auto *st = stat_tag->as<compound_tag *>(); st) {
                if (auto *color_tag = st->get("color"); color_tag) {
                    if (auto *ct = color_tag->as<string_tag *>(); ct) {
                        extra_tag = ct->value;
                    }
                }
            }
        }
        return {entry->name, bl::get_block_by_name_tag(entry->name, extra_tag)};
    }

    const std::string &sub_chunk::get_block_name(int rx, int ry, int rz, int layer) {
        static const std::string unknown = "minecraft:unknown";
        const auto *entry = this->palette_entry_at(rx, ry, rz, layer);
        return entry ? entry->name : unknown;
    }

    nbt::compound_tag *sub_chunk::get_block_raw(int rx, int ry, int rz, int layer) {
        const auto *entry = this->palette_entry_at(rx, ry, rz, layer);
        return entry ? entry->tag : nullptr;
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
