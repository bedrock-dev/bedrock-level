//
// Created by xhy on 2023/6/21.
//

#include "level_dat.h"

#include <filesystem>
#include <fstream>
#include <memory>

#include "utils.h"
namespace bl {

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

        auto x_tag = root_->get("spawnZ");
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
