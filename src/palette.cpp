//
// Created by xhy on 2023/3/29.
//

#include "palette.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "chunk_data_position.h"
#include "utils.h"

namespace bl {
    namespace {
        constexpr auto BLOCK_NUM = 16 * 16 * 16;

        [[nodiscard]] size_t flat_index_from_xyz(int x, int y, int z, int size_y, int size_z) {
            return static_cast<size_t>(size_z) * static_cast<size_t>(size_y) * static_cast<size_t>(x) +
                   static_cast<size_t>(size_z) * static_cast<size_t>(y) + static_cast<size_t>(z);
        }

        [[nodiscard]] block_pos block_pos_from_flat_index(size_t flat, int size_y, int size_z) {
            const auto yz = static_cast<size_t>(size_y) * static_cast<size_t>(size_z);
            if (yz == 0) return {0, 0, 0};
            const int x = static_cast<int>(flat / yz);
            const size_t rem = flat % yz;
            const int y = static_cast<int>(rem / static_cast<size_t>(size_z));
            const int z = static_cast<int>(rem % static_cast<size_t>(size_z));
            return {x, y, z};
        }

        [[nodiscard]] bool is_visible_block_name(const std::string &name) {
            return name != "minecraft:air" && name != "minecraft:cave_air" && name != "minecraft:void_air" && name != "minecraft:unknown";
        }

        bl::nbt::list_tag *make_int_list(const std::string &key, int a, int b, int c) {
            auto *list = new bl::nbt::list_tag(key);
            list->append(new bl::nbt::int_tag("", a));
            list->append(new bl::nbt::int_tag("", b));
            list->append(new bl::nbt::int_tag("", c));
            return list;
        }

        [[nodiscard]] block_pos to_world_pos(const block_pos &origin, const block_pos &local_pos) { return origin + local_pos; }
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

    const mcstructure::layer_type &mcstructure::layer(size_t index) const noexcept {
        static const layer_type empty;
        return index < layer_count() ? layers_[index] : empty;
    }

    size_t mcstructure::layer_size(size_t index) const noexcept { return index < layer_count() ? layers_[index].size() : 0; }

    const palette_entry *mcstructure::palette_entry_at(size_t index) const noexcept {
        return index < palette_.size() ? &palette_[index] : nullptr;
    }

    block_pos mcstructure::block_entity_position(size_t index) const noexcept {
        return index < block_entity_positions_.size() ? block_entity_positions_[index] : block_pos{0, 0, 0};
    }

    int32_t mcstructure::block_index(int layer, int x, int y, int z) const noexcept {
        if (layer < 0 || layer >= static_cast<int>(layer_count())) return -1;
        if (x < 0 || y < 0 || z < 0) return -1;
        if (x >= size_x_ || y >= size_y_ || z >= size_z_) return -1;
        const auto &layer_values = layers_[layer];
        const size_t index = flat_index_from_xyz(x, y, z, size_y_, size_z_);
        if (index >= layer_values.size()) return -1;
        return layer_values[index];
    }

    int32_t mcstructure::block_index(int x, int y, int z) const noexcept {
        for (int layer = 0; layer < static_cast<int>(layer_count()); ++layer) {
            const int32_t index = block_index(layer, x, y, z);
            if (index < 0) continue;
            const auto *entry = palette_entry_at(static_cast<size_t>(index));
            if (entry && is_visible_block_name(entry->name)) return index;
        }
        return -1;
    }

    const palette_entry *mcstructure::block_at(int layer, int x, int y, int z) const noexcept {
        const int32_t idx = block_index(layer, x, y, z);
        return idx >= 0 ? palette_entry_at(static_cast<size_t>(idx)) : nullptr;
    }

    const palette_entry *mcstructure::block_at(int x, int y, int z) const noexcept {
        for (int layer = 0; layer < static_cast<int>(layer_count()); ++layer) {
            const auto *entry = block_at(layer, x, y, z);
            if (entry && is_visible_block_name(entry->name)) return entry;
        }
        return nullptr;
    }

    std::string mcstructure::to_raw() const {
        auto root = std::make_unique<bl::nbt::compound_tag>("");
        root->put(new bl::nbt::int_tag("format_version", version_));
        root->put(make_int_list("size", size_x_, size_y_, size_z_));

        auto *structure = new bl::nbt::compound_tag("structure");

        auto *block_indices = new bl::nbt::list_tag("block_indices");
        for (int layer = 0; layer < 2; ++layer) {
            // Version 2 stores each layer as an IntArrayTag and omits an empty extra layer.
            const bool zero_is_empty = palette_.empty() || !is_visible_block_name(palette_.front().name);
            const bool extra_layer_empty = std::all_of(layers_[layer].begin(), layers_[layer].end(), [zero_is_empty](int32_t index) {
                return index < 0 || (zero_is_empty && index == 0);
            });
            if (version_ == 2 && layer == 1 && extra_layer_empty) {
                continue;
            }
            if (version_ == 2) {
                block_indices->append(new bl::nbt::int_array_tag("", layers_[layer]));
            } else {
                auto *layer_list = new bl::nbt::list_tag("");
                layer_list->value.reserve(layers_[layer].size());
                for (int32_t index : layers_[layer]) {
                    layer_list->append(new bl::nbt::int_tag("", index));
                }
                block_indices->append(layer_list);
            }
        }
        structure->put(block_indices);

        auto *entities = new bl::nbt::list_tag("entities");
        entities->value.reserve(entities_.size());
        for (auto *entity : entities_) {
            if (entity) entities->append(entity->copy());
        }
        structure->put(entities);

        auto *palette = new bl::nbt::compound_tag("palette");
        auto *def = new bl::nbt::compound_tag("default");

        auto *block_palette = new bl::nbt::list_tag("block_palette");
        block_palette->value.reserve(palette_.size());
        for (const auto &entry : palette_) {
            if (entry.tag) block_palette->append(entry.tag->copy());
        }
        def->put(block_palette);

        auto *block_position_data = new bl::nbt::compound_tag("block_position_data");
        for (size_t i = 0; i < block_entities_.size(); ++i) {
            auto *block_entity = block_entities_[i];
            if (!block_entity) continue;
            const auto pos = i < block_entity_positions_.size() ? block_entity_positions_[i] : block_pos{0, 0, 0};
            const int x = pos.x;
            const int y = pos.y;
            const int z = pos.z;

            const auto flat = flat_index_from_xyz(x, y, z, size_y_, size_z_);
            auto *entry = new bl::nbt::compound_tag(std::to_string(flat));
            auto *block_entity_data = static_cast<bl::nbt::compound_tag *>(block_entity->copy());
            block_entity_data->set_key("block_entity_data");
            set_block_entity_pos(block_entity_data, to_world_pos(origin(), pos));
            entry->put(block_entity_data);
            block_position_data->put(entry);
        }
        def->put(block_position_data);
        palette->put(def);
        structure->put(palette);

        root->put(structure);
        root->put(make_int_list("structure_world_origin", origin_.x, origin_.y, origin_.z));
        return root->to_raw();
    }

    bool mcstructure::save_to_file(const std::string &file_name) const {
        const auto raw = to_raw();
        std::ofstream output(std::filesystem::u8path(file_name), std::ios::binary);
        if (!output.is_open()) {
            BL_ERROR("Can not open file %s", file_name.c_str());
            return false;
        }
        output.write(raw.data(), static_cast<std::streamsize>(raw.size()));
        return output.good();
    }

    void mcstructure_builder::reset_layers() {
        const auto total = static_cast<size_t>(std::max(0, size_x_)) * static_cast<size_t>(std::max(0, size_y_)) *
                           static_cast<size_t>(std::max(0, size_z_));
        for (auto &layer : layers_) {
            layer.assign(total, -1);
        }
        palette_.clear();
        palette_index_by_raw_.clear();
        block_entities_.clear();
        block_entity_positions_.clear();
        entities_.clear();
        interned_tags_.clear();
        owned_tags_.clear();
    }

    mcstructure_builder::layer_type &mcstructure_builder::layer_at(int layer) {
        static layer_type empty;
        return layer >= 0 && layer < static_cast<int>(std::size(layers_)) ? layers_[layer] : empty;
    }

    const mcstructure_builder::layer_type &mcstructure_builder::layer_at(int layer) const {
        static const layer_type empty;
        return layer >= 0 && layer < static_cast<int>(std::size(layers_)) ? layers_[layer] : empty;
    }

    size_t mcstructure_builder::flat_index(int x, int y, int z, int size_y, int size_z) {
        return flat_index_from_xyz(x, y, z, size_y, size_z);
    }

    bl::nbt::compound_tag *mcstructure_builder::intern_tag(const bl::nbt::compound_tag *tag, bool strip_version) {
        if (!tag) return nullptr;
        auto clone = std::unique_ptr<bl::nbt::compound_tag>(static_cast<bl::nbt::compound_tag *>(tag->copy()));
        if (strip_version) clone->remove("version");
        const std::string raw = clone->to_raw();
        if (auto it = interned_tags_.find(raw); it != interned_tags_.end()) {
            return it->second;
        }
        auto *stored = clone.get();
        interned_tags_.emplace(raw, stored);
        owned_tags_.push_back(std::move(clone));
        return stored;
    }

    size_t mcstructure_builder::ensure_palette_index(bl::nbt::compound_tag *tag) {
        if (!tag) return std::numeric_limits<size_t>::max();
        const std::string raw = tag->to_raw();
        if (auto it = palette_index_by_raw_.find(raw); it != palette_index_by_raw_.end()) {
            return it->second;
        }
        const size_t index = palette_.size();
        palette_.push_back(make_palette_entry(tag));
        palette_index_by_raw_.emplace(raw, index);
        return index;
    }

    mcstructure_builder::mcstructure_builder(const block_pos &size, const block_pos &origin, int32_t version) {
        size_x_ = std::max(0, size.x);
        size_y_ = std::max(0, size.y);
        size_z_ = std::max(0, size.z);
        origin_ = origin;
        version_ = version == 2 ? 2 : 1;
        reset_layers();
    }

    mcstructure_builder &mcstructure_builder::set_block(const block_pos &pos, const bl::nbt::compound_tag *tag) {
        return set_block(0, pos, tag);
    }

    mcstructure_builder &mcstructure_builder::set_block(int layer, const block_pos &pos, const bl::nbt::compound_tag *tag) {
        if (!has_size() || layer < 0 || layer >= static_cast<int>(std::size(layers_))) return *this;
        const int x = pos.x;
        const int y = pos.y;
        const int z = pos.z;
        if (x < 0 || y < 0 || z < 0 || x >= size_x_ || y >= size_y_ || z >= size_z_) return *this;
        const auto index = flat_index(x, y, z, size_y_, size_z_);
        auto &values = layer_at(layer);
        if (values.empty() || index >= values.size()) return *this;
        if (!tag) {
            values[index] = -1;
            return *this;
        }
        auto *stored = intern_tag(tag, true);
        values[index] = static_cast<int32_t>(ensure_palette_index(stored));
        return *this;
    }

    mcstructure_builder &mcstructure_builder::fill_blocks(const block_box &box, const bl::nbt::compound_tag *tag) {
        return fill_blocks(0, box, tag);
    }

    mcstructure_builder &mcstructure_builder::fill_blocks(int layer, const block_box &box, const bl::nbt::compound_tag *tag) {
        if (!has_size() || layer < 0 || layer >= static_cast<int>(std::size(layers_))) return *this;
        const auto fillBox = box.normalized().intersected(block_box::from_min_and_size({0, 0, 0}, size_x_, size_y_, size_z_));
        if (!fillBox.is_valid()) return *this;

        auto &values = layer_at(layer);
        if (values.empty()) return *this;

        int32_t palette_index = -1;
        if (tag) {
            auto *stored = intern_tag(tag, true);
            palette_index = static_cast<int32_t>(ensure_palette_index(stored));
        }
        for (int x = fillBox.min_pos.x; x < fillBox.max_pos.x; ++x) {
            for (int y = fillBox.min_pos.y; y < fillBox.max_pos.y; ++y) {
                for (int z = fillBox.min_pos.z; z < fillBox.max_pos.z; ++z) {
                    values[flat_index(x, y, z, size_y_, size_z_)] = palette_index;
                }
            }
        }
        return *this;
    }

    mcstructure_builder &mcstructure_builder::set_block_entity(const block_pos &pos, const bl::nbt::compound_tag *tag) {
        const int x = pos.x;
        const int y = pos.y;
        const int z = pos.z;
        if (!has_size() || x < 0 || y < 0 || z < 0 || x >= size_x_ || y >= size_y_ || z >= size_z_) return *this;
        if (!tag) return *this;

        auto clone = std::unique_ptr<bl::nbt::compound_tag>(static_cast<bl::nbt::compound_tag *>(tag->copy()));
        set_block_entity_pos(clone.get(), to_world_pos(origin_, pos));

        auto *stored = clone.get();
        owned_tags_.push_back(std::move(clone));
        block_entities_.push_back(stored);
        block_entity_positions_.push_back(pos);
        return *this;
    }

    mcstructure_builder &mcstructure_builder::add_entity(const bl::nbt::compound_tag *tag) {
        if (!tag) return *this;

        auto clone = std::unique_ptr<bl::nbt::compound_tag>(static_cast<bl::nbt::compound_tag *>(tag->copy()));
        auto *stored = clone.get();
        owned_tags_.push_back(std::move(clone));
        entities_.push_back(stored);
        return *this;
    }

    void mcstructure_builder::release_ownership() {
        for (auto &tag : owned_tags_) {
            tag.release();
        }
        owned_tags_.clear();
        interned_tags_.clear();
    }

    mcstructure mcstructure_builder::build() {
        mcstructure result;
        result.version_ = version_;
        result.size_x_ = size_x_;
        result.size_y_ = size_y_;
        result.size_z_ = size_z_;
        result.origin_ = origin_;
        for (size_t i = 0; i < block_entities_.size(); ++i) {
            auto *block_entity = block_entities_[i];
            if (!block_entity) continue;
            const auto local_pos = i < block_entity_positions_.size() ? block_entity_positions_[i] : block_pos{0, 0, 0};
            set_block_entity_pos(block_entity, to_world_pos(result.origin(), local_pos));
        }
        result.palette_ = std::move(palette_);
        result.layers_[0] = std::move(layers_[0]);
        result.layers_[1] = std::move(layers_[1]);
        result.block_entities_ = std::move(block_entities_);
        result.block_entity_positions_ = std::move(block_entity_positions_);
        result.entities_ = std::move(entities_);
        release_ownership();
        return result;
    }

    mcstructure parse_mcstructure(const byte_t *data, size_t len) {
        mcstructure result;
        int read = 0;
        auto *root = bl::nbt::read_one_palette(data, len, read);
        if (!root) return result;

        auto *format_version_tag = root->get("format_version");
        auto *format_version = format_version_tag ? format_version_tag->as<bl::nbt::int_tag *>() : nullptr;
        if (!format_version) {
            BL_ERROR("Invalid mcstructure: missing format_version");
            delete root;
            return result;
        }
        result.version_ = format_version->value;
        if (result.version_ != 1 && result.version_ != 2) {
            BL_ERROR("Unsupported mcstructure format_version: %d (supported versions are 1 and 2)", result.version_);
            result.version_ = 0;
            delete root;
            return result;
        }

        auto read_vec3 = [](bl::nbt::list_tag *list) {
            block_pos pos;
            if (!list || list->value.size() < 3) return pos;
            if (auto *x = list->value[0]->as<bl::nbt::int_tag *>(); x) pos.x = x->value;
            if (auto *y = list->value[1]->as<bl::nbt::int_tag *>(); y) pos.y = y->value;
            if (auto *z = list->value[2]->as<bl::nbt::int_tag *>(); z) pos.z = z->value;
            return pos;
        };
        auto get_list = [&](const char *path) -> bl::nbt::list_tag * {
            auto *tag = root->getByPath(path);
            return tag ? tag->as<bl::nbt::list_tag *>() : nullptr;
        };
        auto get_compound = [&](const char *path) -> bl::nbt::compound_tag * {
            auto *tag = root->getByPath(path);
            return tag ? tag->as<bl::nbt::compound_tag *>() : nullptr;
        };

        const auto size = read_vec3(get_list("size"));
        result.size_x_ = size.x;
        result.size_y_ = size.y;
        result.size_z_ = size.z;
        result.origin_ = read_vec3(get_list("structure_world_origin"));

        const auto volume = static_cast<size_t>(std::max(0, result.size_x_)) * static_cast<size_t>(std::max(0, result.size_y_)) *
                            static_cast<size_t>(std::max(0, result.size_z_));
        result.layers_[0].assign(volume, -1);
        result.layers_[1].assign(volume, -1);

        // Version 1 uses ListTag<IntTag> layers; version 2 uses ListTag<IntArrayTag> layers.
        if (auto *bi = get_list("structure.block_indices")) {
            for (int layer = 0; layer < 2 && layer < static_cast<int>(bi->value.size()); layer++) {
                if (auto *layer_array = bi->value[layer]->as<bl::nbt::int_array_tag *>(); layer_array) {
                    result.layers_[layer] = layer_array->value;
                    continue;
                }
                if (auto *layer_list = bi->value[layer]->as<bl::nbt::list_tag *>(); layer_list) {
                    auto &indices = result.layers_[layer];
                    indices.clear();
                    indices.reserve(layer_list->value.size());
                    for (auto *item : layer_list->value) {
                        auto *it = item->as<bl::nbt::int_tag *>();
                        indices.push_back(it ? it->value : -1);
                    }
                }
            }
        }

        // entities: list of entity NBT compounds
        if (auto *e_list = get_list("structure.entities")) {
            result.entities_.reserve(e_list->value.size());
            for (auto *child : e_list->value) {
                if (auto *e = child->as<bl::nbt::compound_tag *>(); e) {
                    result.entities_.push_back(static_cast<bl::nbt::compound_tag *>(e->copy()));
                }
            }
        }

        // palette -> default -> block_palette
        if (auto *bp_list = get_list("structure.palette.default.block_palette")) {
            result.palette_.reserve(bp_list->value.size());
            for (auto *child : bp_list->value) {
                if (auto *comp = child->as<bl::nbt::compound_tag *>(); comp) {
                    result.palette_.push_back(make_palette_entry(static_cast<bl::nbt::compound_tag *>(comp->copy())));
                }
            }
        }

        // block_position_data: key = flat index, value -> block_entity_data
        if (auto *bpd = get_compound("structure.palette.default.block_position_data")) {
            for (auto &kv : bpd->value) {
                size_t flat = 0;
                try {
                    flat = static_cast<size_t>(std::stoull(kv.first));
                } catch (...) {
                    continue;
                }
                auto *entry = kv.second->as<bl::nbt::compound_tag *>();
                if (!entry) continue;
                if (auto *be = entry->get("block_entity_data"); be) {
                    if (auto *comp = be->as<bl::nbt::compound_tag *>(); comp) {
                        result.block_entities_.push_back(static_cast<bl::nbt::compound_tag *>(comp->copy()));
                        result.block_entity_positions_.push_back(block_pos_from_flat_index(flat, result.size_y_, result.size_z_));
                    }
                }
            }
        }

        delete root;
        return result;
    }

    mcstructure::~mcstructure() {
        std::unordered_set<bl::nbt::compound_tag *> deleted;
        deleted.reserve(palette_.size() + entities_.size() + block_entities_.size());
        for (auto &entry : palette_) {
            if (entry.tag && deleted.insert(entry.tag).second) delete entry.tag;
        }
        for (auto *e : entities_) {
            if (e && deleted.insert(e).second) delete e;
        }
        for (auto *be : block_entities_) {
            if (be && deleted.insert(be).second) delete be;
        }
    }

    std::string mcstructure::dump() const {
        std::ostringstream oss;
        oss << "mcstructure version=" << version_ << " size=(" << size_x_ << ", " << size_y_ << ", " << size_z_ << ")"
            << " origin=(" << origin_.x << ", " << origin_.y << ", " << origin_.z << ")\n";
        oss << "palette (" << palette_.size() << "):\n";
        for (size_t i = 0; i < palette_.size(); i++) {
            oss << "  [" << i << "] " << palette_[i].name << "\n";
        }
        for (int layer = 0; layer < 2; layer++) {
            int voids = 0;
            for (int idx : layers_[layer]) {
                if (idx == -1) voids++;
            }
            oss << "layer " << layer << ": " << layers_[layer].size() << " blocks (" << voids << " void)\n";
        }
        oss << "entities: " << entities_.size() << ", block_entities: " << block_entities_.size() << "\n";
        return oss.str();
    }
}  // namespace bl
