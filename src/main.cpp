//
// Created by xhy on 2023/3/28.
//
#include <iostream>
#include "leveldb/db.h"
#include "leveldb/comparator.h"
#include "leveldb/slice.h"
#include "leveldb/compressor.h"
#include "leveldb/zlib_compressor.h"
#include "leveldb/filter_policy.h"
#include "leveldb/cache.h"

int main() {
    leveldb::Options options;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);
    options.write_buffer_size = 4 * 1024 * 1024;
    options.compressors[0] = new leveldb::ZlibCompressorRaw(-1);
    options.compressors[1] = new leveldb::ZlibCompressor();
    leveldb::DB *db;
    leveldb::Status status = leveldb::DB::Open(options, "./mc-world/db", &db);
    assert(status.ok());
    leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::cout << it->key().ToString() << std::endl;
    }

    return 0;

}