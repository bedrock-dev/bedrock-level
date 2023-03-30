//
// Created by xhy on 2023/3/30.
//

#include "bedrock_level.h"
#include <iostream>
#include "leveldb/db.h"
#include "leveldb/comparator.h"
#include "leveldb/slice.h"
#include "leveldb/compressor.h"
#include "leveldb/zlib_compressor.h"
#include "leveldb/filter_policy.h"
#include "leveldb/cache.h"
#include <fstream>
#include "bit_tools.h"
#include "nbt.hpp"
#include "bedrock_key.h"
#include "sub_chunk.h"
#include <fstream>

namespace bl {

    const std::string bedrock_level::LEVEL_DATA = "level.dat";
    const std::string bedrock_level::LEVEL_DB = "db";


//    bool bedrock_level::open(const std::string &root) {
//
////    leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
////    for (it->SeekToFirst(); it->Valid(); it->Next()) {
////
////        if (it->key().size() >= 9 && it->key()[8] == 47) {
////            auto x = *reinterpret_cast<const int *>(it->key().data());
////            auto z = *reinterpret_cast<const int *>(it->key().data() + 4);
////            // printBinaries(it->key());
////            saveSubChunk(x, z, it->key()[9], it->value());
////            printf("find sub chunk (%d,%d) with y = %d\n", x, z, it->key()[9]);
////        }
//
//        return true;
//    }

    bool bedrock_level::open(const std::string &root) {
        this->root_name_ = root;
        return this->read_level_dat() && this->read_db();
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
            dat >> contexts::bedrock_disk >> this->level_dat_;
            // std::cout << contexts::mojangson << this->level_dat_;
        } catch (const std::exception &e) {
            BL_ERROR("Invalid level.dat Format: %s", e.what());
            return false;
        }
        return true;
    }


    bool bedrock_level::read_db() { //NOLINT
        leveldb::Options options;
        options.filter_policy = leveldb::NewBloomFilterPolicy(10);
        options.block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);
        options.write_buffer_size = 4 * 1024 * 1024;
        options.compressors[0] = new leveldb::ZlibCompressorRaw(-1);
        options.compressors[1] = new leveldb::ZlibCompressor();
        leveldb::Status status = leveldb::DB::Open(options, this->root_name_ + "/" + bl::bedrock_level::LEVEL_DB,
                                                   &this->db_);
        if (!status.ok()) {
            BL_ERROR("Can not open level database: %s", status.ToString().c_str());
        }
        return status.ok();
    }


    void bedrock_level::parse_keys() {
        leveldb::Iterator *it = db_->NewIterator(leveldb::ReadOptions());
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            auto k = bl::bedrock_key::parse_key(it->key().ToString());
            if (k.type == bedrock_key::Data3D) {
                utils::write_file("./data3d/" + std::to_string(k.x) + "_" + std::to_string(k.z) + ".data3d",
                                  (const uint8_t *) it->value().data(),
                                  it->value().size());
            }

        }
    }
}

