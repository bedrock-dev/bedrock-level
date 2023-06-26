//
// Created by xhy on 2023/6/21.
//

#include "level_dat.h"

#include <fstream>

#include "utils.h"
namespace bl {

    bool level_dat::load(const std::string &path) {
        using namespace nbt;
        std::ifstream dat(path);
        if (!dat) {
            BL_ERROR("Can not read level dat file %s", path.c_str());
            return false;
        }

        try {
            char header[8];
            dat.read(header, 8);
            nbt::tags::compound_tag root;
            dat >> contexts::bedrock_disk >> this->root_;
            return this->preload_data();

        } catch (const std::exception &e) {
            BL_ERROR("Invalid level.dat Format: %s", e.what());
            return false;
        }
    }
    bool level_dat::preload_data() {
        try {
            auto *root = dynamic_cast<nbt::tags::compound_tag *>(this->root_.value.at("").get());
            if (!root) {
                BL_LOGGER("Can't load preload data in level dat file");
                return false;
            }
            this->spawn_position_.x = root->get<int>("SpawnX");
            this->spawn_position_.y = root->get<int>("SpawnY");
            this->spawn_position_.z = root->get<int>("SpawnZ");
            this->storage_version_ = root->get<int>("StorageVersion");
            this->level_name_ = root->get<std::string>("LevelName");
            return true;
        } catch (std::exception &e) {
            BL_LOGGER("Can't load preload data in level dat file %s", e.what());
            return false;
        }
    }
    level_dat::~level_dat() {}

}  // namespace bl
