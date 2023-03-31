//
// Created by xhy on 2023/3/30.
//
#include "bedrock_level.h"

#include <gtest/gtest.h>

#include <iostream>

#include "bedrock_key.h"
#include "utils.h"

TEST(BedrockLevel, SimpleOpen) {
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
}


TEST(BedrockLevel, CheckChunkKeys) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        std::cout << k.to_string() << std::endl;
    }
}


//Tag 43
TEST(BedrockLevel, ExportData3d) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::Data3D) {
            utils::write_file(
                    "data3d/" + std::to_string(k.cp.x) + "_" + std::to_string(k.cp.z) + ".data3d",
                    (uint8_t *) it->value().data(), it->value().size());
        }
    }
}




//Tag 44
TEST(BedrockLevel, CheckVersion) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::VersionNew) {
            ASSERT_EQ(it->value().size(), 1);
            printf("Chunk version is %d\n", (int) it->value()[0]);
        }
    }
}


//Tag 47
TEST(BedrockLevel, ExportSubChunkTerrain) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::SubChunkTerrain) {
            utils::write_file(
                    "sub_chunks/" + std::to_string(k.cp.x) + "_" + std::to_string(k.cp.z) + std::to_string(k.y_index) +
                    ".subchunk",
                    (uint8_t *) it->value().data(), it->value().size());
        }
    }
}


//Tag 49
TEST(BedrockLevel, ExportBlockEntity) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        printf("%s\n", k.to_string().c_str());
        if (k.type == chunk_key::BlockEntity) {
            utils::write_file(
                    "bes/" + std::to_string(k.cp.z) + "_" + std::to_string(k.cp.z) + ".blockentity.palette",
                    (uint8_t *) it->value().data(), it->value().size());
        }
    }
}

//Tag 50
//TEST(BedrockLevel, ExprotEntity) {
//    using namespace bl;
//    bl::bedrock_level level;
//    EXPECT_TRUE(level.open("./2"));
//    auto *db = level.db();
//    auto *it = db->NewIterator(leveldb::ReadOptions());
//    for (it->SeekToFirst(); it->Valid(); it->Next()) {
//        auto k = bl::chunk_key::parse_chunk_ley(it->key().ToString());
//        if (k.type == chunk_key::Entity) {
//            utils::write_file(
//                    "entities/" + std::to_string(k.x) + "_" + std::to_string(k.z) + ".entity.palette",
//                    (uint8_t *) it->value().data(), it->value().size());
//        }
//    }
//}
//Tag 51
TEST(BedrockLevel, ExportPts) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::PendingTicks) {
            utils::write_file(
                    "pts/" + std::to_string(k.cp.x) + "_" + std::to_string(k.cp.z) + ".pt.palette",
                    (uint8_t *) it->value().data(), it->value().size());
        }
    }
}


//Tag 54

TEST(BedrockLevel, CheckChunkState) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::FinalizedState) {
            ASSERT_EQ(it->value().size(), 4);
            printf("Chunk State is %d\n", *reinterpret_cast<const int32_t *>(it->value().data()));
        }
    }
}



//Tag 58
TEST(BedrockLevel, ExportRandomTick) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./2"));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::RandomTicks) {
            utils::write_file(
                    "rt/" + std::to_string(k.cp.x) + "_" + std::to_string(k.cp.z) + ".rt.palette",
                    (uint8_t *) it->value().data(), it->value().size());
        }
    }
}


TEST(BedrockLevel, SaveInvalid) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open("./sample"));
    auto *db = level.db();
    size_t idx = 0;
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto ck = bl::chunk_key::parse(it->key().ToString());
        if (ck.valid()) {
//            std::cout << "Chunk key: " << ck.to_string() << std::endl;
            continue;
        }

        auto actor_key = bl::actor_key::parse(it->key().ToString());
        if (actor_key.valid()) {
//            std::cout << "Actor key: " << actor_key.to_string() << std::endl;
            continue;
        }

        auto digest_key = bl::actor_digest_key::parse(it->key().ToString());
        if (digest_key.valid()) {
//            std::cout << "Digest key: " << digest_key.to_string() << std::endl;
            continue;
        }

        auto village_key = bl::village_key::parse(it->key().ToString());
        if (village_key.valid()) {
            std::cout << "Village Key: " << village_key.to_string() << std::endl;
            continue;
        }

        utils::write_file(
                "invalid/" + std::to_string(idx) + ".key",
                (uint8_t *) it->key().data(), it->key().size());
        utils::write_file(
                "invalid/" + std::to_string(idx) + ".value",
                (uint8_t *) it->value().data(), it->value().size());
        ++idx;

    }
}




