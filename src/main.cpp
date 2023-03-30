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
#include <fstream>
#include "bit_tools.h"
#include "nbt.hpp"
#include "sub_chunk.h"
#include "bedrock_level.h"
//
//void printBinaries(const leveldb::Slice &slice) {
//    for (int i = 0; i < slice.size(); i++) {
//        printf("%02d ", slice[i]);
//    }
//
//}
//
//
//void saveSubChunk(int x, int z, int y, const leveldb::Slice &slice) {
//    auto name = "./subchunks/" + std::to_string(x) + "_" + std::to_string(z) + "_" + std::to_string(y) + ".subdata";
//    FILE *fp = fopen(name.c_str(), "wb+");
//    fwrite(slice.data(), 1, slice.size(), fp);
//    fclose(fp);
//}


int main() {
//    leveldb::Options options;
//    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
//    options.block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);
//    options.write_buffer_size = 4 * 1024 * 1024;
//    options.compressors[0] = new leveldb::ZlibCompressorRaw(-1);
//    options.compressors[1] = new leveldb::ZlibCompressor();
//    leveldb::DB *db;
//    leveldb::Status status = leveldb::DB::Open(options, "./mc-world/db", &db);
//    assert(status.ok());
//    leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
//    for (it->SeekToFirst(); it->Valid(); it->Next()) {
//
//        if (it->key().size() >= 9 && it->key()[8] == 47) {
//            auto x = *reinterpret_cast<const int *>(it->key().data());
//            auto z = *reinterpret_cast<const int *>(it->key().data() + 4);
//            // printBinaries(it->key());
//            saveSubChunk(x, z, it->key()[9], it->value());
//            printf("find sub chunk (%d,%d) with y = %d\n", x, z, it->key()[9]);
//        }
//
////        if (it->key().size() == 10 || it->key().size() == 9) {
//
////            int dim = 0;
////            if (it->key().size() == 10) {
////                dim = *reinterpret_cast<const int8_t *>(it->key().data() + 8);
////            }
////            std::cout << "Chunk: " << x << "  " << z << std::endl;
////
////        }
//
//
//    }

//    uint8_t bytes[3]{0b10111101, 0b11110000, 0b11100011};
//    auto vec = bl::bits::restructureBytes<uint8_t>(3, bytes, 3);
//    for (auto v: vec) {
//        printf("%d ", v);
    // }

//
//    auto data = bl::utils::read_file("./subchunks/0_0_0.subdata");
//    bl::sub_chunk sub_chunk;
//    sub_chunk.load(data.data(), data.size());
//    sub_chunk.dump_to_file(stdout);
//    system("chcp 65001");
    bl::bedrock_level level;
    level.open("./2");
    level.parse_keys();

    return 0;

}