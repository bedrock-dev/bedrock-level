//
// Created by xhy on 2023/3/29.
//

#include "sub_chunk.h"

#include <cstdio>
#include <unordered_map>

#include "utils.h"

// #include "nbt.hpp"
#include <cstdio>

#include "color.h"
#include "nbt.h"
#include "palette.h"

namespace bl {

    namespace {

        constexpr auto BLOCK_NUM = 16 * 16 * 16;

        // Minimal block state compound: {"name": <name>}. Caller owns the result.
        [[nodiscard]] nbt::compound_tag* make_block_state(const std::string& name) {
            auto* tag = new nbt::compound_tag("");
            tag->put(new nbt::string_tag("name", name));
            return tag;
        }

        // Gives an untouched layer a valid body: palette index 0 is air, and every block starts
        // there. Without this the first written block would occupy index 0 and silently become
        // the background for all the positions that were never set.
        void seed_layer_with_air(sub_chunk::layer& target) {
            if (!target.palette.empty()) return;
            target.palette.push_back(bl::make_palette_entry(make_block_state("minecraft:air")));
            target.blocks.assign(BLOCK_NUM, 0);
        }

        // Shared bounds check for the in-chunk accessors: a sub-chunk is always 16x16x16.
        [[nodiscard]] bool is_valid_in_chunk_pos(int rx, int ry, int rz) {
            if (rx >= 0 && rx <= 15 && ry >= 0 && ry <= 15 && rz >= 0 && rz <= 15) return true;
            LOG_F(ERROR, "Invalid in chunk position %d %d %d", rx, ry, rz);
            return false;
        }

        // sub chunk layout
        // https://user-images.githubusercontent.com/13713600/148380033-6223ac76-54b7-472c-a355-5923b87cb7c5.png
        bool read_header(sub_chunk* sub_chunk, const byte_t* stream, int& read, uint8_t& layers_num) {
            if (!sub_chunk || !stream) return false;
            // assert that stream is long enough
            const auto raw_version = static_cast<uint8_t>(stream[0]);
            if (!is_supported_sub_chunk_version(raw_version)) {
                LOG_F(INFO, "Unsupported sub chunk version: %u", raw_version);
                return false;
            }
            const auto version = static_cast<SubChunkVersion>(raw_version);
            sub_chunk->set_version(version);
            // Only load() needs the count, and only until the layers are read.
            layers_num = static_cast<uint8_t>(stream[1]);
            read = 2;
            // y-index for version 9
            if (version == SubChunkVersion::V9) {
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

    sub_chunk::~sub_chunk() {
        for (auto& layer : this->layers_) {
            delete layer;
        }
    }

    sub_chunk::layer::~layer() {
        for (auto& entry : this->palette) delete entry.tag;
    }

    bool sub_chunk::load(const byte_t* data, size_t len) {
        size_t idx = 0;
        int read{0};
        uint8_t layers_num = 0;
        if (!read_header(this, data, read, layers_num)) return false;
        idx += read;
        for (auto i = 0; i < (int)layers_num; i++) {
            auto* layer = new bl::sub_chunk::layer();
            this->layers_.push_back(layer);
            layer->blocks = bl::read_block_indices(data + idx, read, layer->bits, layer->palette_len);
            idx += read;
            layer->palette = bl::read_palettes(data + idx, layer->palette_len, len - idx, read);
            idx += read;
        }
        return true;
    }

    std::string sub_chunk::to_raw() const {
        // A sub_chunk that was built instead of loaded still carries the unset marker, which is
        // not a valid on-disk value; fall back to the modern layout. Bit packing below is
        // identical for both versions, so only the header bytes depend on this.
        uint8_t version = this->version_;
        if (!is_supported_sub_chunk_version(version)) {
            LOG_F(WARNING, "Sub chunk version %u is not a valid on-disk value, writing v9", version);
            version = static_cast<uint8_t>(SubChunkVersion::V9);
        }

        std::string out;
        out.push_back(static_cast<char>(version));
        // The header count has to describe the layers actually held, so it comes from the
        // vector rather than from a separately tracked count.
        out.push_back(static_cast<char>(this->layers_.size()));
        if (version == static_cast<uint8_t>(SubChunkVersion::V9)) {
            out.push_back(static_cast<char>(this->y_index_));
        }
        for (const auto* layer : this->layers_) {
            if (!layer) continue;
            bl::write_layer(out, layer->blocks, layer->palette);
        }
        return out;
    }

    void sub_chunk::layer::set_block(int rx, int ry, int rz, const nbt::compound_tag* tag) {
        if (!is_valid_in_chunk_pos(rx, ry, rz)) return;
        if (!tag) return;
        seed_layer_with_air(*this);

        // Append unconditionally; compact() is what makes the palette unique.
        auto* clone = static_cast<nbt::compound_tag*>(tag->copy());
        this->palette.push_back(bl::make_palette_entry(clone));
        this->blocks[ry + rz * 16 + rx * 256] = static_cast<uint16_t>(this->palette.size() - 1);
    }

    void sub_chunk::layer::fill_blocks(const nbt::compound_tag* tag) {
        if (!tag) return;
        for (auto& entry : this->palette) delete entry.tag;
        this->palette.clear();
        this->palette.push_back(bl::make_palette_entry(static_cast<nbt::compound_tag*>(tag->copy())));
        this->blocks.assign(BLOCK_NUM, 0);
    }

    void sub_chunk::layer::fill_blocks(const block_box& box, const nbt::compound_tag* tag) {
        if (!tag) return;
        const auto area = box.normalized().intersected(block_box::from_min_and_size({0, 0, 0}, 16, 16, 16));
        if (!area.is_valid()) return;
        seed_layer_with_air(*this);

        // Append rather than replace: the cells outside the box keep what they held.
        const auto index = static_cast<uint16_t>(this->palette.size());
        this->palette.push_back(bl::make_palette_entry(static_cast<nbt::compound_tag*>(tag->copy())));

        for (int x = area.min_pos.x; x < area.max_pos.x; x++) {
            for (int y = area.min_pos.y; y < area.max_pos.y; y++) {
                for (int z = area.min_pos.z; z < area.max_pos.z; z++) {
                    this->blocks[y + z * 16 + x * 256] = index;
                }
            }
        }
    }

    void sub_chunk::layer::compact() {
        if (this->palette.empty()) return;
        // A layer that was never populated is written as 4096 zeros, so it really only uses
        // entry 0; normalize first so pruning sees the same thing the writer will.
        if (this->blocks.size() != BLOCK_NUM) this->blocks.assign(BLOCK_NUM, 0);

        std::vector<bool> referenced(this->palette.size(), false);
        for (auto& index : this->blocks) {
            if (index >= this->palette.size()) {
                LOG_F(ERROR, "Invalid block index %d, clamping to 0", index);
                index = 0;
            }
            referenced[index] = true;
        }

        // Tags reaching this point already went through make_palette_entry, so their
        // serialized form is stable and safe to use as the identity key.
        std::unordered_map<std::string, uint16_t> first_index_by_raw;
        first_index_by_raw.reserve(this->palette.size());

        std::vector<uint16_t> remap(this->palette.size(), 0);
        std::vector<palette_entry> rebuilt;
        rebuilt.reserve(this->palette.size());

        for (size_t i = 0; i < this->palette.size(); i++) {
            auto& entry = this->palette[i];
            if (!referenced[i]) {
                delete entry.tag;  // nothing points here any more
                entry.tag = nullptr;
                continue;
            }
            const std::string raw = entry.tag ? entry.tag->to_raw() : std::string();
            const auto it = first_index_by_raw.find(raw);
            if (it != first_index_by_raw.end()) {
                remap[i] = it->second;
                delete entry.tag;  // duplicate: the survivor keeps ownership
                entry.tag = nullptr;
            } else {
                const auto index = static_cast<uint16_t>(rebuilt.size());
                first_index_by_raw.emplace(raw, index);
                remap[i] = index;
                rebuilt.push_back(entry);
            }
        }

        this->palette = std::move(rebuilt);
        for (auto& index : this->blocks) index = remap[index];
        // bits / palette_len are derived while writing, so there is nothing to refresh here.
    }

    sub_chunk::layer* sub_chunk::ensure_layer(int index) {
        if (index < 0) return nullptr;
        while (static_cast<int>(this->layers_.size()) <= index) {
            // Padding layers are seeded as air because they may never receive a set_block call,
            // and a layer with an empty palette has no valid on-disk form.
            auto* created = new layer();
            seed_layer_with_air(*created);
            this->push_back_layer(created);
        }
        return this->layers_[index];
    }

    void sub_chunk::set_block(int rx, int ry, int rz, const nbt::compound_tag* tag, int layer_index) {
        auto* target = this->ensure_layer(layer_index);
        if (!target) return;
        target->set_block(rx, ry, rz, tag);
    }

    void sub_chunk::fill_layer(const nbt::compound_tag* tag, int layer_index) {
        if (!tag) return;
        if (layer_index < 0) {
            for (auto* existing : this->layers_) {
                if (existing) existing->fill_blocks(tag);
            }
            return;
        }
        if (auto* target = this->ensure_layer(layer_index)) target->fill_blocks(tag);
    }

    void sub_chunk::fill_blocks(const block_box& box, const nbt::compound_tag* tag, int layer_index) {
        if (!tag) return;
        if (layer_index < 0) {
            for (auto* existing : this->layers_) {
                if (existing) existing->fill_blocks(box, tag);
            }
            return;
        }
        if (auto* target = this->ensure_layer(layer_index)) target->fill_blocks(box, tag);
    }

    void sub_chunk::compact() {
        for (auto* layer : this->layers_) {
            if (layer) layer->compact();
        }
    }

    const std::string& sub_chunk::get_block_name(int rx, int ry, int rz, int layer) {
        static const std::string unknown = "minecraft:unknown";
        const auto* entry = this->palette_entry_at(rx, ry, rz, layer);
        return entry ? entry->name : unknown;
    }

    nbt::compound_tag* sub_chunk::get_block_raw(int rx, int ry, int rz, int layer) {
        const auto* entry = this->palette_entry_at(rx, ry, rz, layer);
        return entry ? entry->tag : nullptr;
    }

    block_appearance sub_chunk::get_block_with_color(int rx, int ry, int rz, int layer) {
        const auto* entry = this->palette_entry_at(rx, ry, rz, layer);
        if (!entry) return {};

        using bl::nbt::string_tag, bl::nbt::compound_tag;
        std::string extra_tag;
        // states/color may be absent on simple blocks, guard each level
        if (auto* stat_tag = entry->tag->get("states"); stat_tag) {
            if (auto* st = stat_tag->as<compound_tag*>(); st) {
                if (auto* color_tag = st->get("color"); color_tag) {
                    if (auto* ct = color_tag->as<string_tag*>(); ct) {
                        extra_tag = ct->value;
                    }
                }
            }
        }
        return {entry->name, bl::get_block_by_name_tag(entry->name, extra_tag)};
    }

    const palette_entry* sub_chunk::palette_entry_at(int rx, int ry, int rz, int layer) const {
        if (!is_valid_in_chunk_pos(rx, ry, rz)) return nullptr;
        if (layer < 0 || layer >= static_cast<int>(this->layers_.size())) {
            return nullptr;  // requested layer not present
        }
        auto& ly = *this->layers_[layer];
        auto idx = ry + rz * 16 + rx * 256;
        auto block = ly.blocks[idx];
        if (block >= ly.palette.size()) {
            LOG_F(ERROR, "Invalid block index with value %d", block);
            return nullptr;
        }
        return &ly.palette[block];
    }
}  // namespace bl
