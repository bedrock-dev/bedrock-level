//
// Created by xhy on 2023/6/21.
//

#include "level_dat.h"

#include <filesystem>

#include "include/palette.h"
#include "utils.h"

namespace bl {

    void ClientVersion::read(palette::list_tag *tag) {
        if (!tag) return;
        const auto &value = tag->value;
        const size_t count = std::min(value.size(), this->version.size());
        for (size_t i = 0; i < count; i++) {
            if (value[i] && value[i]->type() == palette::tag_type::Int) {
                this->version[i] = dynamic_cast<palette::int_tag *>(value[i])->value;
            }
        }
    }

    void ClientVersion::write(palette::list_tag *tag) const {
        if (!tag) return;
        for (auto *item : tag->value) delete item;
        tag->value.clear();
        for (int v : this->version) {
            tag->value.push_back(new palette::int_tag("", v));
        }
    }

    std::string ClientVersion::to_string() const {
        return std::to_string(version[0]) + "." + std::to_string(version[1]) + "." + std::to_string(version[2]) + "." +
               std::to_string(version[3]) + "." + std::to_string(version[4]);
    }

    bool level_dat::load_from_file(const std::string &path) {
        using namespace bl::palette;
        namespace fs = std::filesystem;
        if (!fs::exists(path)) {
            return false;
        }

        auto data = utils::read_file(path);
        return this->load_from_raw_data(data);
    }

    bool level_dat::preload_data() {
        using namespace bl::palette;
        auto name_tag = root_->get("LevelName");
        if (name_tag && name_tag->type() == tag_type::String) {
            this->level_name_ = dynamic_cast<string_tag *>(name_tag)->value;
        }

        auto x_tag = root_->get("SpawnX");
        auto y_tag = root_->get("SpawnY");
        auto z_tag = root_->get("SpawnZ");
        if (x_tag && x_tag->type() == tag_type::Int) {
            this->spawn_position_.x = dynamic_cast<int_tag *>(x_tag)->value;
        }
        if (y_tag && y_tag->type() == tag_type::Int) {
            this->spawn_position_.y = dynamic_cast<int_tag *>(y_tag)->value;
        }

        if (z_tag && z_tag->type() == tag_type::Int) {
            this->spawn_position_.z = dynamic_cast<int_tag *>(z_tag)->value;
        }

        auto *ver_tag = root_->get("MinimumCompatibleClientVersion");
        if (ver_tag && ver_tag->type() == tag_type::List) {
            this->min_compat_version_.read(dynamic_cast<list_tag *>(ver_tag));
        }

        auto *world_start_count_tag = root_->get("worldStartCount");
        if (world_start_count_tag && world_start_count_tag->type() == tag_type::Long) {
            world_start_count_ = dynamic_cast<long_tag *>(world_start_count_tag)->value;
        }
        return true;
    }

    bool level_dat::load_from_raw_data(const std::vector<byte_t> &data) {
        using namespace bl::palette;
        if (data.size() <= 8) return false;
        int read = 0;
        this->header_ = std::string(data.data(), 8);
        this->root_ = read_one_palette(data.data() + 8, read);
        if (!root_ || read != static_cast<int>(data.size()) - 8) {
            BL_ERROR("can not read level.dat");
            return false;
        }
        return this->preload_data();
    }
    void level_dat::set_nbt(bl::palette::compound_tag *root) {
        if (!root) return;
        delete this->root_;
        this->root_ = root;
        this->preload_data();
    }
    std::string level_dat::to_raw() const { return this->header_ + this->root_->to_raw(); }
    level_dat::~level_dat() { delete this->root_; }
}  // namespace bl
