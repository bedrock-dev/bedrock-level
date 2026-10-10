#include "bedrock_level.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>

#include "bedrock_key.h"
#include "chunk.h"
#include "include/utils.h"
#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/decompress_allocator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/mcne.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/write_batch.h"
#include "leveldb/zlib_compressor.h"
#include "nbt.h"

class SlowEnv : public leveldb::Env {};

namespace {
    /// A key never needs to be copied to be classified, so scans read it as a view.
    [[nodiscard]] inline std::string_view slice_view(const leveldb::Slice& slice) noexcept {
        return std::string_view(slice.data(), slice.size());
    }

}  // namespace

namespace bl {
    const std::string bedrock_level::LEVEL_DATA = "level.dat";
    const std::string bedrock_level::LEVEL_DB = "db";
    const std::string bedrock_level::CUSTOM_DIM_KEY_PREFIX = "custom_dim:";
    const std::string bedrock_level::CUSTOM_DIM_TABLE_KEY = "DimensionNameIdTable";

    bedrock_level::bedrock_level(bool libdeflate, std::string xor_key) {
        options_.filter_policy = leveldb::NewBloomFilterPolicy(10);
        options_.block_cache = leveldb::NewLRUCache(20 * 1024 * 1024);
        options_.write_buffer_size = 4 * 1024 * 1024;
        options_.block_size = 163840;
        // Slot 0 reads raw-deflate tables; slot 1 reads zlib-wrapped data.
        auto* compressor_raw = new leveldb::ZlibCompressorRaw(-1);
        compressor_raw->useLibdeflate = libdeflate;
        auto* compressor_zlib = new leveldb::ZlibCompressor();
        compressor_zlib->useLibdeflate = libdeflate;
        options_.compressors[0] = compressor_raw;
        options_.compressors[1] = compressor_zlib;
        env_wrapper_ = new leveldb::McneWrapper(leveldb::Env::Default(), xor_key);
        options_.env = env_wrapper_;
        read_option_.decompress_allocator = new leveldb::DecompressAllocator();
    };

    bedrock_level::~bedrock_level() {
        this->close();
        delete this->env_wrapper_;
        delete this->options_.compressors[0];
        delete this->options_.compressors[1];
        delete this->options_.block_cache;
        delete this->options_.filter_policy;
        delete this->read_option_.decompress_allocator;
    };

    void bedrock_level::set_xor_key(std::string xor_key) {
        if (this->is_open_) return;
        delete this->env_wrapper_;
        this->env_wrapper_ = new leveldb::McneWrapper(leveldb::Env::Default(), xor_key);
        this->options_.env = this->env_wrapper_;
    }

    bool bedrock_level::open(const std::string& root) {
        namespace fs = std::filesystem;
        this->root_name_ = root;
        fs::path path(this->root_name_);
        path /= LEVEL_DATA;
        const bool loaded = this->dat_.load_from_file(path.string());
        if (loaded) {
            this->chunk_format_ = client_version_to_chunk_format(this->dat_.min_compat_version());
            this->roll_actor_uid_tag();
        }
        this->is_open_ = loaded && this->load_db();
        return this->is_open_;
    }

    void bedrock_level::close() {
        this->village_data_.clear_data();
        this->player_data_.clear_data();
        this->db_.reset();
        this->is_open_ = false;
        this->actor_uid_tag_ = 0;
        this->actor_uid_index = 1;
    }

    chunk* bedrock_level::get_chunk(const chunk_pos& cp, chunk_load_policy policy) {
        if (!this->is_open()) {
            return nullptr;
        }
        return this->load_chunk(cp, policy);
    }

    bool bedrock_level::load_raw(std::string_view key, std::string& value) {
        if (!this->is_open() || !this->db_) return false;
        auto r = this->db_->Get(read_option_, leveldb::Slice(key.data(), key.size()), &value);
        return r.ok();
    }

    leveldb::ReadOptions bedrock_level::bulk_read_options() const {
        leveldb::ReadOptions options = this->read_option_;
        options.fill_cache = false;
        return options;
    }

    void bedrock_level::load_global_data() {
        this->foreach_global_keys([this](std::string_view key, std::string_view value) {
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
    void bedrock_level::foreach_global_keys(const std::function<void(std::string_view, std::string_view)>& f) {
        std::unique_ptr<leveldb::Iterator> it(this->db_->NewIterator(this->read_option_));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            const auto key = slice_view(it->key());
            auto ck = bl::chunk_key::parse(key);
            if (ck.valid()) continue;
            auto actor_key = bl::actor_key::parse(key);
            if (actor_key.valid()) continue;
            const auto db_key = it->key();
            const auto value = it->value();
            f(std::string_view(db_key.data(), db_key.size()), std::string_view(value.data(), value.size()));
        }
    }

    void bedrock_level::foreach_key_with_prefix(std::string_view prefix, const std::function<void(std::string_view, std::string_view)>& f,
                                                std::atomic_bool& stop, int max) {
        std::unique_ptr<leveldb::Iterator> it(this->db_->NewIterator(this->read_option_));
        int count = 0;
        const leveldb::Slice prefix_slice(prefix.data(), prefix.size());
        for (it->Seek(prefix_slice); it->Valid() && it->key().starts_with(prefix_slice); it->Next()) {
            const auto key = it->key();
            const auto value = it->value();
            f(std::string_view(key.data(), key.size()), std::string_view(value.data(), value.size()));
            count++;
            if ((count >= max && max > 0) || stop) {
                return;
            }
        }
    }

    void bedrock_level::roll_actor_uid_tag() {
        // Avoid the game's descending counter range and choose a fresh ID range per open.
        std::random_device device;
        std::uniform_int_distribution<uint32_t> distribution(1, 0x7FFFFFFFu);
        this->actor_uid_tag_ = distribution(device);
        this->actor_uid_index = 1;
    }

    uint64_t bedrock_level::generate_actor_uid() { return (static_cast<uint64_t>(this->actor_uid_tag_) << 32) | this->actor_uid_index++; }

    chunk* bedrock_level::load_chunk(const chunk_pos& cp, chunk_load_policy policy) {
        auto* chunk = new bl::chunk(cp);
        if (!chunk->load_data(*this, policy)) {
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
        leveldb::DB* opened_db = nullptr;
        leveldb::Status status = leveldb::DB::Open(this->options_, bl::utils::UTF8ToGBEx(path.string().c_str()), &opened_db);
        this->db_.reset(opened_db);
        if (!status.ok()) {
            LOG_F(ERROR, "Can not open level database: [%s].", status.ToString().c_str());
        } else {
            load_dimension_name_id_table();
        }
        return status.ok();
    }

    void bedrock_level::load_dimension_name_id_table() {
        if (!db_) return;
        std::string value;
        auto r = this->db_->Get(read_option_, CUSTOM_DIM_TABLE_KEY, &value);
        if (!r.ok()) return;
        int read;
        auto* nbt = bl::nbt::read_one_palette(value.c_str(), read);
        if (!nbt) return;

        auto* entries = nbt->get("entries");
        if (entries) {
            auto* entries_compound = dynamic_cast<bl::nbt::compound_tag*>(entries);
            if (entries_compound) {
                custom_dimension_table_.clear();
                for (auto& [dim_name, tag] : entries_compound->value) {
                    auto* int_tag = dynamic_cast<bl::nbt::int_tag*>(tag);
                    if (int_tag) {
                        custom_dimension_table_[dim_name] = int_tag->value;
                    }
                }
            }
        }
        delete nbt;
    }

}  // namespace bl
