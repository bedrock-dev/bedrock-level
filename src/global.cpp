//
// Created by xhy on 2023/3/31.
//

#include "global.h"

#include <array>
namespace bl {
    void village_data::reset(const village_table_type& data) {
        this->clear_data();
        this->data_ = data;
    }
    void village_data::append_village(const village_key& key, const std::string& value) {
        int read = 0;
        auto* nbt = bl::nbt::read_one_palette(value.data(), read);
        if (static_cast<size_t>(read) == value.size() && nbt && key.dim >= 0 && key.dim <= 2) {
            this->data_[key.dim][key.uuid][static_cast<size_t>(key.type)] = nbt;
        }
    }
    village_data::~village_data() { this->clear_data(); }
    void village_data::clear_data() {
        for (auto& dim : this->data_) {
            for (auto& vill : dim) {
                for (auto v : vill.second) {
                    delete v;
                }
            }
            dim.clear();
        }
    }

    void general_kv_nbts::reset(const std::unordered_map<std::string, bl::nbt::compound_tag*>& data) {
        this->clear_data();
        this->data_ = data;
    }
    void general_kv_nbts::append_nbt(const std::string& key, const std::string& value) {
        int read = 0;
        auto* nbt = bl::nbt::read_one_palette(value.data(), read);
        if (static_cast<size_t>(read) == value.size() && nbt) {
            this->data_[key] = nbt;
        }
    }
    general_kv_nbts::~general_kv_nbts() {
        for (auto& kv : this->data_) {
            delete kv.second;
        }
    }
    void general_kv_nbts::clear_data() {
        for (auto& kv : this->data_) {
            delete kv.second;
        }
        this->data_.clear();
    }

}  // namespace bl
