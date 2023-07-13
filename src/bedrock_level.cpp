//
// Created by xhy on 2023/3/30.
//

#include "bedrock_level.h"

#include <fstream>
#include <iostream>

#include "bedrock_key.h"
#include "bit_tools.h"
#include "chunk.h"
#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/write_batch.h"
#include "leveldb/zlib_compressor.h"

namespace bl {
    namespace {
        bool load_raw(leveldb::DB *&db, const std::string &raw_key, std::string &raw) {
            auto r = db->Get(leveldb::ReadOptions(), raw_key, &raw);
            return r.ok();
        }
    }  // namespace

    const std::string bedrock_level::LEVEL_DATA = "level.dat";
    const std::string bedrock_level::LEVEL_DB = "db";

    bool bedrock_level::open(const std::string &root) {
        this->root_name_ = root;
        this->is_open_ =
            this->dat_.load_from_file(this->root_name_ + "/" + LEVEL_DATA) && this->read_db();
        return this->is_open_;
    }

    bool bedrock_level::read_db() {  // NOLINT
        leveldb::Options options;
        options.filter_policy = leveldb::NewBloomFilterPolicy(10);
        options.block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);
        options.write_buffer_size = 4 * 1024 * 1024;
        options.compressors[0] = new leveldb::ZlibCompressorRaw(-1);
        options.compressors[1] = new leveldb::ZlibCompressor();
        leveldb::Status status = leveldb::DB::Open(
            options, this->root_name_ + "/" + bl::bedrock_level::LEVEL_DB, &this->db_);
        if (!status.ok()) {
            BL_ERROR("Can not open level database: %s.", status.ToString().c_str());
        }
        return status.ok();
    }

    bedrock_level::~bedrock_level() { this->close(); };

    chunk *bedrock_level::get_chunk(const chunk_pos &cp) {
        if (!this->is_open()) {
            BL_ERROR("Level is not opened.");
            return nullptr;
        }

        if (!cp.valid()) {
            BL_ERROR("Invalid Chunk position %s", cp.to_string().c_str());
            return nullptr;
        }
        if (this->enable_cache_) {
            auto it = this->chunk_data_cache_.find(cp);
            if (it != this->chunk_data_cache_.end()) {
                return it->second;
            } else {
                auto *ch = this->read_chunk_from_db(cp);
                if (ch) {
                    this->chunk_data_cache_[cp] = ch;
                }
                return ch;
            }
        } else {
            return this->read_chunk_from_db(cp);
        }
    }

    void bedrock_level::close() {
        for (auto &kv : this->chunk_data_cache_) {
            delete kv.second;
        }
        delete this->db_;
        this->db_ = nullptr;
        this->is_open_ = false;
    }

    void bedrock_level::set_cache(bool enable) {
        this->enable_cache_ = enable;
        if (!this->enable_cache_) {
            this->clear_cache();
        }
    }

    chunk *bedrock_level::read_chunk_from_db(const chunk_pos &cp) {
        auto *chunk = new bl::chunk(cp);
        if (!chunk->load_data(*this)) {
            delete chunk;
            return nullptr;
        } else {
            return chunk;
        }
    }

    void bedrock_level::clear_cache() {
        for (auto &kv : this->chunk_data_cache_) delete kv.second;
        this->chunk_data_cache_.clear();
    }

    actor *bedrock_level::load_actor(const std::string &raw_uid) {
        const auto key = "actorprefix" + raw_uid;
        std::string raw_data;
        if (!load_raw(this->db_, key, raw_data)) return nullptr;
        auto ac = new actor;
        if (!ac->load(raw_data.data(), raw_data.size())) {
            delete ac;
            return nullptr;
        } else {
            return ac;
        }
    }
    bool bedrock_level::remove_chunk(const chunk_pos &cp) {
        // remove new chunk  actors
        leveldb::WriteBatch batch;
        bl::actor_digest_key key{cp};
        std::string raw;
        // 没啥要解析的，不用管错误
        if (load_raw(this->db_, key.to_raw(), raw)) {
            bl::actor_digest_list ads;
            ads.load(raw);
            for (auto &uid : ads.actor_digests_) {
                batch.Delete("actorprefix" + uid);
            }
        }

        // terrain
        for (int8_t i = -4; i <= 20; i++) {
            bl::chunk_key terrain_key{chunk_key::SubChunkTerrain, cp, i};
            batch.Delete(terrain_key.to_raw());
        }
        // others
        for (int i = 43; i <= 65; i++) {
            auto t = static_cast<chunk_key::key_type>(i);
            if (t != chunk_key::SubChunkTerrain) {
                auto dk = bl::chunk_key{t, cp};
                batch.Delete(dk.to_raw());
            }
        }

        // version
        bl::chunk_key version_key{chunk_key::VersionOld, cp};
        batch.Delete(version_key.to_raw());
        auto s = this->db_->Write(leveldb::WriteOptions(), &batch);
        return s.ok();
    }
    void bedrock_level::foreach_global_keys(
        const std::function<void(const std::string &, const std::string &)> &f) {
        auto *it = this->db_->NewIterator(leveldb::ReadOptions());
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            auto ck = bl::chunk_key::parse(it->key().ToString());
            if (ck.valid()) continue;
            auto actor_key = bl::actor_key::parse(it->key().ToString());
            if (actor_key.valid()) continue;
            f(it->key().ToString(), it->value().ToString());
        }
        delete it;
    }

    void bedrock_level::load_global_data() {
        this->foreach_global_keys([this](const std::string &key, const std::string &value) {
            if (key.find("player") != std::string::npos) {
                this->player_list_.append_player(key, value);
            } else {
                bl::village_key vk = village_key::parse(key);
                if (vk.valid()) {
                    this->village_list_.append_village(vk, value);
                }
            }
        });
    }
}  // namespace bl