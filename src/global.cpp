#include "global.h"

#include <algorithm>
#include <array>
namespace bl {
    namespace global_key {
        namespace {
            constexpr key_list OTHER_KEYS{"scoreboard", "AutonomousEntities", "BiomeData", "Nether",      "Overworld",
                                          "TheEnd",     "schedulerWT",        "mobevents", "WorldClocks", "LevelChunkMetaDataDictionary"};
        }

        const key_list& other_keys() noexcept { return OTHER_KEYS; }

        bool is_other_key(std::string_view key) noexcept {
            return std::find(OTHER_KEYS.begin(), OTHER_KEYS.end(), key) != OTHER_KEYS.end();
        }
    }  // namespace global_key

    void village_data::reset(const village_table_type& data) {
        this->clear_data();
        this->data_ = data;
    }
    void village_data::append_village(const village_key& key, std::string_view value) {
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
    void general_kv_nbts::append_nbt(std::string_view key, std::string_view value) {
        int read = 0;
        auto* nbt = bl::nbt::read_one_palette(value.data(), read);
        if (static_cast<size_t>(read) == value.size() && nbt) {
            this->data_[std::string(key)] = nbt;
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
