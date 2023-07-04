//
// Created by xhy on 2023/3/31.
//

#include "global.h"
namespace bl {
    void village_data::reset(
        const std::unordered_map<std::string, std::array<bl::palette::compound_tag*, 4>>& data) {
        for (auto& village : this->data_) {
            for (auto& nbt : village.second) delete nbt;
        }
        this->data_ = data;
    }
    void village_data::append_village(const village_key& key, const std::string& value) {
        int read = 0;
        auto* nbt = bl::palette::read_one_palette(value.data(), read);
        if (read == value.size() && nbt) {
            this->data_[key.uuid][static_cast<int>(key.type)] = nbt;
        }
    }

    void player_data::reset(
        const std::unordered_map<std::string, bl::palette::compound_tag*>& data) {
        for (auto& kv : this->data_) {
            delete kv.second;
        }
        this->data_ = data;
    }
    void player_data::append_player(const std::string& key, const std::string& value) {
        int read = 0;
        auto* nbt = bl::palette::read_one_palette(value.data(), read);
        if (read == value.size() && nbt) {
            this->data_[key] = nbt;
        }
    }
}  // namespace bl
