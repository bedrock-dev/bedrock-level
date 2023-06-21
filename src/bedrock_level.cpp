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
#include "leveldb/zlib_compressor.h"
#include "nbt-cpp/nbt.hpp"

namespace bl {

    const std::string bedrock_level::LEVEL_DATA = "level.dat";
    const std::string bedrock_level::LEVEL_DB = "db";

    bool bedrock_level::open(const std::string &root) {
        this->root_name_ = root;
        auto res = this->read_level_dat() && this->read_db();
        if (!res) return false;
        this->cache_keys();
        this->is_open_ = true;
        return this->is_open_;
    }

    bool bedrock_level::read_level_dat() {
        using namespace nbt;
        const std::string level_data_path = this->root_name_ + "/" + LEVEL_DATA;
        std::ifstream dat(level_data_path);
        if (!dat) {
            BL_ERROR("Can not read dat file %s", level_data_path.c_str());
            return false;
        }
        try {
            char header[8];
            dat.read(header, 8);
            nbt::tags::compound_tag root;
            dat >> contexts::bedrock_disk >> root;
            this->level_dat_ = dynamic_cast<tags::compound_tag *>(root.value.at("").get());
            if (!this->level_dat_) {
                BL_ERROR("Invalid level.dat Format");
            }
        } catch (const std::exception &e) {
            BL_ERROR("Invalid level.dat Format: %s", e.what());
            return false;
        }
        return true;
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

    bedrock_level::~bedrock_level() { delete this->db_; };

    void bedrock_level::cache_keys() {
        leveldb::Iterator *it = db_->NewIterator(leveldb::ReadOptions());
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            auto raw_key = it->key().ToString();
            auto ck = bl::chunk_key::parse(it->key().ToString());
            if (ck.valid()) {
                auto r = this->chunk_data_cache_.find(ck.cp);
                if (r == this->chunk_data_cache_.end()) {
                    auto *ch = new bl::chunk(ck.cp);
                    if (ck.type == chunk_key::SubChunkTerrain) {
                        ch->sub_chunk_indexes_.insert(ck.y_index);
                    }
                    this->chunk_data_cache_.insert(std::make_pair(ck.cp, ch));
                } else {
                    if (ck.type == chunk_key::SubChunkTerrain) {
                        r->second->sub_chunk_indexes_.insert(ck.y_index);
                    }
                }
                continue;
            }

            //            auto actor_key = bl::actor_key::parse(it->key().ToString());
            //            if (actor_key.valid()) {
            //                continue;
            //            }
            //
            //            auto digest_key = bl::actor_digest_key::parse(it->key().ToString());
            //            if (digest_key.valid()) {
            //                continue;
            //            }
            //
            //            auto village_key = bl::village_key::parse(it->key().ToString());
            //            if (village_key.valid()) {
            //                continue;
            //
            //            }
        }
        delete it;
    }

    chunk *bedrock_level::get_chunk(const chunk_pos &cp) {
        if (!cp.valid()) {
            BL_ERROR("Invalid Chunk position %s", cp.to_string().c_str());
            return nullptr;
        }
        auto it = this->chunk_data_cache_.find(cp);
        if (it == this->chunk_data_cache_.end()) {
            return nullptr;
        } else {
            // lazy load
            it->second->load_data(*this);
            return it->second;
        }
    }

    void bedrock_level::for_each_chunk_pos(const std::function<void(const chunk_pos &)> &f) {
        for (auto &kv : this->chunk_data_cache_) {
            f(kv.first);
        }
    }

    block_info bedrock_level::get_block(const block_pos &pos, int dim) {
        auto cp = pos.to_chunk_pos();
        cp.dim = dim;
        if (!cp.valid()) return {};
        auto off = pos.in_chunk_offset();
        auto *ch = this->get_chunk(cp);
        if (!ch) return {};
        return ch->get_block(off.x, pos.y, off.z);
    }
    std::tuple<chunk_pos, chunk_pos> bedrock_level::get_range(int dim) const {
        int32_t minX{INT32_MAX};
        int32_t minZ{INT32_MAX};
        int32_t maxX{INT32_MIN};
        int32_t maxZ{INT32_MIN};
        for (auto &kv : this->chunk_data_cache_) {
            if (kv.first.dim != dim) continue;
            minX = std::min(minX, kv.first.x);
            minZ = std::min(minZ, kv.first.z);
            maxX = std::max(maxX, kv.first.x);
            maxZ = std::max(maxZ, kv.first.z);
        }
        return {{minX, minZ, dim}, {maxX, maxZ, dim}};
    }

    void bedrock_level::dump_level_dat() const { this->level_dat_->write(std::cout); }
    block_pos bedrock_level::get_spawn_position() {
        if (!this->level_dat_) {
            BL_ERROR("Invalid TAG");
            return {0, 0, 0};
        }
        using namespace nbt;

        return {0, 0, 0};
    }
}  // namespace bl
