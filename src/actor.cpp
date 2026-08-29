//
// Created by xhy on 2023/3/31.
//

#include "actor.h"

#include <cstdint>
#include <iostream>
#include <random>

#include "bedrock_level.h"
#include "nbt.h"

namespace bl {

    bool actor::load(const byte_t *data, size_t len) {
        auto p = bl::nbt::read_palette_to_end(data, len);
        if (p.size() != 1) {
            LOG_F(ERROR, "Invalid Actor!!");
            return false;
        }

        this->root_ = p[0];
        this->loaded_ = this->preload(root_);
        return this->loaded_;
    }

    void actor::dump() {
        printf("- type: %s\n", this->identifier_.c_str());
        printf("- pos: [%f %f %f]\n", pos_.x, pos_.y, pos_.z);
        printf("- NBT:\n\n");
        for (auto &kv : this->root_->value) {
            kv.second->write(std::cout, 0);
            printf("=============================\n");
        }
    }
    bool actor::load_from_nbt(bl::nbt::compound_tag *nbt) {
        if (!this->preload(nbt)) return false;
        this->root_ = dynamic_cast<bl::nbt::compound_tag *>(nbt->copy());
        return true;
    }

    bool actor::load_from_nbt_owned(bl::nbt::compound_tag *nbt) {
        if (!this->preload(nbt)) return false;
        this->root_ = nbt;
        this->loaded_ = true;
        return true;
    }

    bool actor::preload(bl::nbt::compound_tag *root) {
        if (!root) return false;
        bool read_pos = false;
        bool read_uid = false;
        bool read_identifier = false;
        auto it = root->value.find("Pos");
        if (it != root->value.end()) {
            auto *pos_tag = dynamic_cast<bl::nbt::list_tag *>(it->second);
            if (pos_tag && pos_tag->value.size() == 3) {
                auto *tag_x = dynamic_cast<bl::nbt::float_tag *>(pos_tag->value[0]);
                auto *tag_y = dynamic_cast<bl::nbt::float_tag *>(pos_tag->value[1]);
                auto *tag_z = dynamic_cast<bl::nbt::float_tag *>(pos_tag->value[2]);
                if (tag_x && tag_y && tag_z) {
                    this->pos_.x = tag_x->value;
                    this->pos_.y = tag_y->value;
                    this->pos_.z = tag_z->value;
                    read_pos = true;
                }
            }
        }

        auto it2 = root->value.find("identifier");
        if (it2 != root->value.end()) {
            auto *id_tag = dynamic_cast<bl::nbt::string_tag *>(it2->second);
            if (id_tag) {
                this->identifier_ = id_tag->value;
                read_identifier = true;
            }
        }
        auto it3 = root->value.find("UniqueID");
        if (it3 != root->value.end()) {
            auto lt = dynamic_cast<bl::nbt::long_tag *>(it3->second);
            if (lt) {
                this->uid_ = lt->value;
                read_uid = true;
            }
        }
        return read_pos && read_identifier && read_uid;
    }
    actor::~actor() { delete this->root_; }

    void actor::reassign_uid(int64_t uid) {
        this->uid_ = uid;
        if (!root_) return;

        // update UniqueID
        auto it = root_->value.find("UniqueID");
        if (it != root_->value.end()) {
            auto *lt = dynamic_cast<bl::nbt::long_tag *>(it->second);
            if (lt) {
                lt->value = uid;
            }
        }

        // update internalComponents -> EntityStorageKeyComponent -> StorageKey
        auto storage_key = this->storage_key_raw();
        auto *ic = dynamic_cast<bl::nbt::compound_tag *>(root_->get("internalComponents"));
        if (!ic) {
            ic = new bl::nbt::compound_tag("internalComponents");
            root_->put(ic);
        }
        auto *eskc = dynamic_cast<bl::nbt::compound_tag *>(ic->get("EntityStorageKeyComponent"));
        if (!eskc) {
            eskc = new bl::nbt::compound_tag("EntityStorageKeyComponent");
            ic->put(eskc);
        }
        eskc->put(new bl::nbt::string_tag("StorageKey", storage_key));
    }

    void actor::offset_pos(float dx, float dz) {
        if (!root_) return;
        auto it = root_->value.find("Pos");
        if (it == root_->value.end()) return;
        auto *pos_tag = dynamic_cast<bl::nbt::list_tag *>(it->second);
        if (!pos_tag || pos_tag->value.size() != 3) return;
        auto *tx = dynamic_cast<bl::nbt::float_tag *>(pos_tag->value[0]);
        auto *tz = dynamic_cast<bl::nbt::float_tag *>(pos_tag->value[2]);
        if (tx) {
            tx->value += dx;
            pos_.x = tx->value;
        }
        if (tz) {
            tz->value += dz;
            pos_.z = tz->value;
        }
    }
}  // namespace bl
