#include "level_dat.h"

#include <cctype>
#include <filesystem>
#include <string_view>

#include "magic-enum/magic_enum.hpp"
#include "nbt.h"
#include "utils.h"

namespace bl {

    void ClientVersion::read(nbt::list_tag* tag) {
        if (!tag) return;
        const auto& value = tag->value;
        const size_t count = std::min(value.size(), this->version.size());
        for (size_t i = 0; i < count; i++) {
            if (value[i] && value[i]->type() == nbt::tag_type::Int) {
                this->version[i] = dynamic_cast<nbt::int_tag*>(value[i])->value;
            }
        }
    }

    void ClientVersion::write(nbt::list_tag* tag) const {
        if (!tag) return;
        for (auto* item : tag->value) delete item;
        tag->value.clear();
        for (int v : this->version) {
            tag->value.push_back(new nbt::int_tag("", v));
        }
    }

    std::string ClientVersion::to_string() const {
        return std::to_string(version[0]) + "." + std::to_string(version[1]) + "." + std::to_string(version[2]) + "." +
               std::to_string(version[3]) + "." + std::to_string(version[4]);
    }

    LevelChunkFormat client_version_to_chunk_format(const ClientVersion& version) {
        const std::array<int, 3> client{version.version[0], version.version[1], version.version[2]};
        const auto formats = magic_enum::enum_values<LevelChunkFormat>();
        // Select the newest format no newer than the client.
        for (auto it = formats.rbegin(); it != formats.rend(); ++it) {
            if (*it == LevelChunkFormat::Count)
                continue;  // upper bound of the enum, not a format
                           // Read only the first three numeric groups; later digits are revision labels.
            std::array<int, 3> format{0, 0, 0};
            const std::string_view name = magic_enum::enum_name(*it);
            size_t groups = 0;
            for (size_t i = 0; i < name.size() && groups < format.size();) {
                if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
                    ++i;
                    continue;
                }
                int value = 0;
                while (i < name.size() && std::isdigit(static_cast<unsigned char>(name[i]))) {
                    value = value * 10 + (name[i] - '0');
                    ++i;
                }
                format[groups++] = value;
            }
            // Two numeric groups denote a pre-1.0 version: V17_0 means 0.17.0.
            if (groups == 2) format = {0, format[0], format[1]};
            if (format <= client) return *it;
        }
        return formats.front();  // client older than every known format
    }

    bool level_dat::load_from_file(const std::string& path) {
        using namespace bl::nbt;
        namespace fs = std::filesystem;
        if (!fs::exists(path)) {
            LOG_F(ERROR, "No such level.dat file: %s", path.c_str());
            return false;
        }

        auto data = utils::read_file(path);
        if (this->load_from_raw_data(data)) {
            return true;
        } else {
            LOG_F(ERROR, "Invalid level.dat file format: %s", path.c_str());
            return false;
        }
    }

    bool level_dat::preload_data() {
        using namespace bl::nbt;
        auto name_tag = root_->get("LevelName");
        if (name_tag && name_tag->type() == tag_type::String) {
            this->level_name_ = dynamic_cast<string_tag*>(name_tag)->value;
        }

        auto x_tag = root_->get("SpawnX");
        auto y_tag = root_->get("SpawnY");
        auto z_tag = root_->get("SpawnZ");
        if (x_tag && x_tag->type() == tag_type::Int) {
            this->spawn_position_.x = dynamic_cast<int_tag*>(x_tag)->value;
        }
        if (y_tag && y_tag->type() == tag_type::Int) {
            this->spawn_position_.y = dynamic_cast<int_tag*>(y_tag)->value;
        }

        if (z_tag && z_tag->type() == tag_type::Int) {
            this->spawn_position_.z = dynamic_cast<int_tag*>(z_tag)->value;
        }

        auto* ver_tag = root_->get("MinimumCompatibleClientVersion");
        if (ver_tag && ver_tag->type() == tag_type::List) {
            this->min_compat_version_.read(dynamic_cast<list_tag*>(ver_tag));
        }
        return true;
    }

    bool level_dat::load_from_raw_data(const std::vector<byte_t>& data) {
        using namespace bl::nbt;
        if (data.size() <= 8) return false;
        int read = 0;
        this->header_ = std::string(data.data(), 8);
        this->root_ = read_one_palette(data.data() + 8, read);
        if (!root_ || read != static_cast<int>(data.size()) - 8) {
            return false;
        }
        return this->preload_data();
    }
    void level_dat::set_nbt(bl::nbt::compound_tag* root) {
        if (!root) return;
        delete this->root_;
        this->root_ = root;
        this->preload_data();
    }
    std::string level_dat::to_raw() const { return this->header_ + this->root_->to_raw(); }
    level_dat::~level_dat() { delete this->root_; }
}  // namespace bl
