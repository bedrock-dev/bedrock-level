//
// Created by xhy on 2023/3/30.
//
#include <gtest/gtest.h>

#include <cerrno>
#include <iostream>
#include <string>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "utils.h"

const std::string TEST_WORLD_ROOT = R"(C:\Users\xhy\Desktop\t)";
const std::string DUMP_ROOT = R"(C:\Users\xhy\dev\bedrock-level\data\dumps\)";

TEST(BedrockLevel, SimpleOpen) {
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    //    level.close();
}

TEST(BedrockLevel, CheckChunkKeys) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        std::cout << k.to_string() << std::endl;
    }
    delete it;
    level.close();
}

// Tag 43
TEST(BedrockLevel, ExportData3d) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::Data3D) {
            utils::write_file(DUMP_ROOT + "data3d/" + std::to_string(k.cp.x) + "_" +
                                  std::to_string(k.cp.z) + ".data3d",
                              it->value().data(), it->value().size());
        }
    }
    delete it;
}

// Tag 44
TEST(BedrockLevel, CheckVersion) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::VersionNew) {
            ASSERT_EQ(it->value().size(), 1);
            printf("Chunk version is %d\n", (int)it->value()[0]);
        }
    }
    delete it;
}

// Tag 47
TEST(BedrockLevel, ExportSubChunkTerrain) {
    using namespace bl;
    bl::bedrock_level level;

    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));

    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::SubChunkTerrain) {
            utils::write_file(DUMP_ROOT + "sub_chunks/" + std::to_string(k.cp.x) + "_" +
                                  std::to_string(k.cp.z) + "_" + std::to_string(k.y_index) +
                                  ".subchunk",
                              it->value().data(), it->value().size());
        }
    }
    delete it;
    level.close();
}

// Tag 49
TEST(BedrockLevel, ExportBlockEntity) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        printf("%s\n", k.to_string().c_str());
        if (k.type == chunk_key::BlockEntity) {
            utils::write_file(DUMP_ROOT + "bes/" + std::to_string(k.cp.z) + "_" +
                                  std::to_string(k.cp.z) + ".blockentity.palette",
                              it->value().data(), it->value().size());
        }
    }
    delete it;
    level.close();
}

// Tag 51
TEST(BedrockLevel, ExportPts) {
    using namespace bl;
    bl::bedrock_level level;

    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));

    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::PendingTicks) {
            utils::write_file(DUMP_ROOT + "pts/" + std::to_string(k.cp.x) + "_" +
                                  std::to_string(k.cp.z) + ".pt.palette",
                              it->value().data(), it->value().size());
        }
    }
    delete it;
    level.close();
}

// Tag 54

TEST(BedrockLevel, CheckChunkState) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::FinalizedState) {
            ASSERT_EQ(it->value().size(), 4);
            printf("Chunk State is %d\n", *reinterpret_cast<const int32_t *>(it->value().data()));
        }
    }
    delete it;
    level.close();
}

// Tag 58
TEST(BedrockLevel, ExportRandomTick) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::RandomTicks) {
            utils::write_file(DUMP_ROOT + "rt/" + std::to_string(k.cp.x) + "_" +
                                  std::to_string(k.cp.z) + ".rt.palette",
                              it->value().data(), it->value().size());
        }
    }
    delete it;
    level.close();
}

TEST(BedrockLevel, ExportHSA) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::HardCodedSpawnAreas) {
            utils::write_file(DUMP_ROOT + "hsa/" + std::to_string(k.cp.x) + "_" +
                                  std::to_string(k.cp.z) + ".hsa.data",
                              it->value().data(), it->value().size());
        }
    }
    delete it;
    level.close();
}

TEST(BedrockLevel, DumpActorDigits) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto k = bl::chunk_key::parse(it->key().ToString());
        if (k.type == chunk_key::ActorDigestVersion && it->value().size() > 1) {
            utils::write_file(DUMP_ROOT + "actor_digits/" + std::to_string(k.cp.x) + "_" +
                                  std::to_string(k.cp.z) + ".actor_digits",
                              it->value().data(), it->value().size());
        }
    }
    delete it;
    level.close();
}

TEST(BedrockLevel, SaveInvalid) {
    using namespace bl;
    bl::bedrock_level level;

    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
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
            //            std::cout << "Village Key: " << village_key.to_string() << std::endl;
            continue;
        }

        utils::write_file(DUMP_ROOT + "invalid/" + std::to_string(idx) + ".key", it->key().data(),
                          it->key().size());
        utils::write_file(DUMP_ROOT + "invalid/" + std::to_string(idx) + ".nbt", it->value().data(),
                          it->value().size());
        ++idx;
    }
    delete it;
    level.close();
}

TEST(BedrockLevel, DumpActors) {
    using namespace bl;
    bl::bedrock_level level;
    EXPECT_TRUE(level.open(TEST_WORLD_ROOT));
    auto *db = level.db();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto key = bl::actor_key::parse(it->key().ToString());
        if (key.valid()) {
            auto path = DUMP_ROOT + "/actors/" + std::to_string(key.actor_uid) + ".palette";
            utils::write_file(path, it->value().data(), it->value().size());
        }
    }
    delete it;
    level.close();
}
