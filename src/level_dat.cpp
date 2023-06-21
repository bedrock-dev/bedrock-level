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
            dat >> contexts::bedrock_disk >> root;
            this->root_ = dynamic_cast<tags::compound_tag *>(root.value.at("").get());

            if (!this->root_) {
                BL_ERROR("Invalid level.dat Format");
                return false;
            }
            return this->preload_data();
            for (auto &kv : this->root_->value) {
                BL_LOGGER("%s", kv.first.c_str());
            }

            return true;
        } catch (const std::exception &e) {
            BL_ERROR("Invalid level.dat Format: %s", e.what());
            return false;
        }
    }
    bool level_dat::preload_data() {
        try {
            this->spawn_position_.x = this->root_->get<int>("SpawnX");
            this->spawn_position_.y = this->root_->get<int>("SpawnY");
            this->spawn_position_.z = this->root_->get<int>("SpawnZ");
            this->storage_version_ = this->root_->get<int>("StorageVersion");
            this->level_name_ = this->root_->get<std::string>("LevelName");
            return true;
        } catch (std::exception &e) {
            BL_LOGGER("Can't load preload data in level dat file %s", e.what());
            return false;
        }
    }

}  // namespace bl
