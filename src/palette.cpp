//
// Created by xhy on 2023/3/29.
//

#include "palette.h"

#include <sstream>

#include "utils.h"

namespace bl {
    namespace {
        constexpr auto BLOCK_NUM = 16 * 16 * 16;
    }  // namespace

    std::vector<uint16_t> read_block_indices(const byte_t *stream, int &read, uint8_t &bits, uint32_t &palette_len) {
        read = 0;
        auto layer_header = stream[0];
        read++;
        bits = layer_header >> 1u;
        std::vector<uint16_t> blocks;
        if (bits != 0) {
            blocks.resize(BLOCK_NUM);
            int block_per_word = 32 / bits;
            auto wordCount = BLOCK_NUM / block_per_word;
            if (BLOCK_NUM % block_per_word != 0) wordCount++;
            int position = 0;
            for (int wordi = 0; wordi < wordCount; wordi++) {
                auto word = *reinterpret_cast<const int *>(stream + read + wordi * 4);
                for (int block = 0; block < block_per_word; block++) {
                    int state = (word >> ((position % block_per_word) * bits)) & ((1 << bits) - 1);
                    if (position < static_cast<int>(blocks.size())) {
                        blocks[position] = static_cast<uint16_t>(state);
                    }
                    position++;
                }
            }

            read += wordCount << 2;
            palette_len = *reinterpret_cast<const int *>(stream + read);
            read += 4;
        } else {  // uniform
            blocks = std::vector<uint16_t>(BLOCK_NUM, 0);
            palette_len = 1;
        }
        return blocks;
    }

    std::vector<palette_entry> read_palettes(const byte_t *stream, size_t number, size_t len, int &read) {
        read = 0;
        std::vector<palette_entry> result;
        result.reserve(number);
        for (auto i = 0u; i < number; i++) {
            int r = 0;
            auto *tag = bl::nbt::read_one_palette(stream + read, len - read, r);
            if (tag) {
                result.push_back(make_palette_entry(tag));
            } else {
                BL_ERROR("Can not read block palette");
                return result;
            }
            read += r;
        }
        return result;
    }

    palette_entry make_palette_entry(bl::nbt::compound_tag *tag) {
        palette_entry entry;
        entry.tag = tag;
        tag->remove("version");  // remove version tag(compatibility for color table)
        // pre-resolve block name so per-block lookups become O(1) indexing
        std::string name{"minecraft:unknown"};
        if (auto *name_tag = tag->get("name"); name_tag) {
            if (auto *st = name_tag->as<bl::nbt::string_tag *>(); st) {
                name = st->value;
            }
        }
        entry.name = std::move(name);
        return entry;
    }

    mcstructure parse_mcstructure(const byte_t *data, size_t len) {
        mcstructure result;
        int read = 0;
        auto *root = bl::nbt::read_one_palette(data, len, read);
        if (!root) return result;

        auto read_vec3 = [](bl::nbt::list_tag *list, int &x, int &y, int &z) {
            if (!list || list->value.size() < 3) return;
            if (auto *a = list->value[0]->as<bl::nbt::int_tag *>(); a) x = a->value;
            if (auto *b = list->value[1]->as<bl::nbt::int_tag *>(); b) y = b->value;
            if (auto *c = list->value[2]->as<bl::nbt::int_tag *>(); c) z = c->value;
        };
        auto get_list = [&](const char *path) -> bl::nbt::list_tag * {
            auto *tag = root->getByPath(path);
            return tag ? tag->as<bl::nbt::list_tag *>() : nullptr;
        };
        auto get_compound = [&](const char *path) -> bl::nbt::compound_tag * {
            auto *tag = root->getByPath(path);
            return tag ? tag->as<bl::nbt::compound_tag *>() : nullptr;
        };

        read_vec3(get_list("size"), result.size_x, result.size_y, result.size_z);
        read_vec3(get_list("structure_world_origin"), result.origin_x, result.origin_y, result.origin_z);

        // block_indices: two int lists, each an index into the palette (ZYX order, -1 = void)
        if (auto *bi = get_list("structure.block_indices")) {
            for (int layer = 0; layer < 2 && layer < static_cast<int>(bi->value.size()); layer++) {
                auto *layer_list = bi->value[layer]->as<bl::nbt::list_tag *>();
                if (!layer_list) continue;
                result.layers[layer].reserve(layer_list->value.size());
                for (auto *item : layer_list->value) {
                    auto *it = item->as<bl::nbt::int_tag *>();
                    result.layers[layer].push_back(it ? it->value : 0);
                }
            }
        }

        // entities: list of entity NBT compounds
        if (auto *e_list = get_list("structure.entities")) {
            result.entities.reserve(e_list->value.size());
            for (auto *child : e_list->value) {
                if (auto *e = child->as<bl::nbt::compound_tag *>(); e) {
                    result.entities.push_back(static_cast<bl::nbt::compound_tag *>(e->copy()));
                }
            }
        }

        // palette -> default -> block_palette
        if (auto *bp_list = get_list("structure.palette.default.block_palette")) {
            result.palette.reserve(bp_list->value.size());
            for (auto *child : bp_list->value) {
                if (auto *comp = child->as<bl::nbt::compound_tag *>(); comp) {
                    result.palette.push_back(make_palette_entry(static_cast<bl::nbt::compound_tag *>(comp->copy())));
                }
            }
        }

        // block_position_data: key = flat index, value -> block_entity_data
        if (auto *bpd = get_compound("structure.palette.default.block_position_data")) {
            for (auto &kv : bpd->value) {
                auto *entry = kv.second->as<bl::nbt::compound_tag *>();
                if (!entry) continue;
                if (auto *be = entry->get("block_entity_data"); be) {
                    if (auto *comp = be->as<bl::nbt::compound_tag *>(); comp) {
                        result.block_entities.push_back(static_cast<bl::nbt::compound_tag *>(comp->copy()));
                    }
                }
            }
        }

        delete root;
        return result;
    }

    mcstructure::~mcstructure() {
        for (auto &entry : palette) delete entry.tag;
        for (auto *e : entities) delete e;
        for (auto *be : block_entities) delete be;
    }

    std::string mcstructure::dump() const {
        std::ostringstream oss;
        oss << "mcstructure size=(" << size_x << ", " << size_y << ", " << size_z << ")"
            << " origin=(" << origin_x << ", " << origin_y << ", " << origin_z << ")\n";
        oss << "palette (" << palette.size() << "):\n";
        for (size_t i = 0; i < palette.size(); i++) {
            oss << "  [" << i << "] " << palette[i].name << "\n";
        }
        for (int layer = 0; layer < 2; layer++) {
            int voids = 0;
            for (int idx : layers[layer]) {
                if (idx == -1) voids++;
            }
            oss << "layer " << layer << ": " << layers[layer].size() << " blocks (" << voids << " void)\n";
        }
        oss << "entities: " << entities.size() << ", block_entities: " << block_entities.size() << "\n";
        return oss.str();
    }
}  // namespace bl