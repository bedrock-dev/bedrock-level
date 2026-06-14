//
// Created by xhy on 2023/3/30.
//

#include "bedrock_level.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

#include "bedrock_key.h"
#include "chunk.h"
#include "include/utils.h"
#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/decompress_allocator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "leveldb/write_batch.h"
#include "leveldb/zlib_compressor.h"

class SlowEnv : public leveldb::Env {};

namespace bl {
    const std::string bedrock_level::LEVEL_DATA = "level.dat";
    const std::string bedrock_level::LEVEL_DB = "db";

    bedrock_level::bedrock_level() {
        options_.filter_policy = leveldb::NewBloomFilterPolicy(10);
        options_.block_cache = leveldb::NewLRUCache(20 * 1024 * 1024);
        options_.write_buffer_size = 4 * 1024 * 1024;
        options_.block_size = 163840;
        options_.compressors[0] = new leveldb::ZlibCompressorRaw(-1);
        options_.compressors[1] = new leveldb::ZlibCompressor();
        // read option
        read_option_.decompress_allocator = new leveldb::DecompressAllocator();
    };

    bedrock_level::~bedrock_level() {
        this->close();
        delete this->options_.compressors[0];
        delete this->options_.compressors[1];
        delete this->options_.block_cache;
        delete this->options_.filter_policy;
        delete this->read_option_.decompress_allocator;
    };

    bool bedrock_level::open(const std::string &root) {
        namespace fs = std::filesystem;
        this->root_name_ = root;
        fs::path path(this->root_name_);
        path /= LEVEL_DATA;
        this->is_open_ = this->dat_.load_from_file(path.string()) && this->load_db();
        return this->is_open_;
    }

    void bedrock_level::close() {
        for (auto &kv : this->chunk_data_cache_) {
            delete kv.second;
        }
        this->clear_cache();
        this->village_data_.clear_data();
        this->player_data_.clear_data();
        delete this->db_;
        this->db_ = nullptr;
        this->is_open_ = false;
    }

    chunk *bedrock_level::get_chunk(const chunk_pos &cp, bool fast_load) {
        if (!this->is_open()) {
            return nullptr;
        }

        if (!cp.valid()) {
            BL_ERROR("Invalid Chunk position %s.", cp.to_string().c_str());
            return nullptr;
        }
        if (this->enable_cache_) {
            auto it = this->chunk_data_cache_.find(cp);
            if (it != this->chunk_data_cache_.end()) {
                return it->second;
            } else {
                auto *ch = this->load_chunk(cp, fast_load);
                if (ch) {
                    this->chunk_data_cache_[cp] = ch;
                }
                return ch;
            }
        } else {
            return this->load_chunk(cp, fast_load);
        }
    }

    void bedrock_level::set_cache(bool enable) {
        this->enable_cache_ = enable;
        if (!this->enable_cache_) {
            this->clear_cache();
        }
    }

    bool bedrock_level::load_raw(const std::string &key, std::string &value) {
        if (!this->is_open() || !this->db_) return false;
        auto r = this->db_->Get(read_option_, key, &value);
        return r.ok();
    }

    actor *bedrock_level::load_actor(const std::string &raw_uid) {
        const auto key = "actorprefix" + raw_uid;
        std::string raw_data;
        if (!load_raw(key, raw_data)) return nullptr;
        auto ac = new actor;
        if (!ac->load(raw_data.data(), raw_data.size())) {
            delete ac;
            return nullptr;
        } else {
            return ac;
        }
    }

    void bedrock_level::load_global_data() {
        this->foreach_global_keys([this](const std::string &key, const std::string &value) {
            if (key.find("player") != std::string::npos) {
                this->player_data_.append_nbt(key, value);
            } else if (key.find("map") == 0) {
                this->map_item_data_.append_nbt(key, value);
            } else {
                bl::village_key vk = village_key::parse(key);
                if (vk.valid()) {
                    this->village_data_.append_village(vk, value);
                }
            }
        });
    }
    void bedrock_level::foreach_global_keys(const std::function<void(const std::string &, const std::string &)> &f) {
        auto *it = this->db_->NewIterator(this->read_option_);
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            auto ck = bl::chunk_key::parse(it->key().ToString());
            if (ck.valid()) continue;
            auto actor_key = bl::actor_key::parse(it->key().ToString());
            if (actor_key.valid()) continue;
            f(it->key().ToString(), it->value().ToString());
        }
        delete it;
    }

    void bedrock_level::foreach_key_with_prefix(const std::string &prefix,
                                                const std::function<void(const std::string &, const std::string &)> &f,
                                                std::atomic_bool &stop, int max) {
        auto *it = this->db_->NewIterator(this->read_option_);
        int count = 0;
        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
            f(it->key().ToString(), it->value().ToString());
            count++;
            if (count >= max || stop) {
                delete it;
                return;
            }
        }
        delete it;
    }

    uint64_t bedrock_level::generate_actor_uid() {
        auto wsc = static_cast<uint64_t>(dat_.world_start_count() - 1) & 0x00000000ffffffff;
        auto uid = (wsc << 32) | wsc_uid;
        wsc_uid++;
        return uid;
    }

    // private
    void bedrock_level::clear_cache() {
        for (auto &kv : this->chunk_data_cache_) delete kv.second;
        this->chunk_data_cache_.clear();
    }

    chunk *bedrock_level::load_chunk(const chunk_pos &cp, bool fast_load) {
        auto *chunk = new bl::chunk(cp);
        if (!chunk->load_data(*this, fast_load)) {
            delete chunk;
            return nullptr;
        } else {
            return chunk;
        }
    }

    bool bedrock_level::load_db() {  // NOLINT
        namespace fs = std::filesystem;
        fs::path path(this->root_name_);
        path /= bl::bedrock_level::LEVEL_DB;
        leveldb::Status status = leveldb::DB::Open(this->options_, bl::utils::UTF8ToGBEx(path.string().c_str()), &this->db_);
        if (!status.ok()) {
            BL_ERROR("Can not open level database: [%s].", status.ToString().c_str());
        }
        return status.ok();
    }
}  // namespace bl
