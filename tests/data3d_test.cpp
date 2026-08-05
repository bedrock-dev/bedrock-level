//
// Created by xhy on 2023/3/30.
// Rewritten: tests based on Data3D payloads extracted from dumped chunks (tests/data).
//

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "chunk.h"
#include "data_3d.h"
#include "utils.h"

namespace fs = std::filesystem;

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

namespace {
    using steady_clock_t = std::chrono::steady_clock;

    std::vector<std::string> list_chunk_files() {
        std::vector<std::string> files;
        for (auto &entry : fs::directory_iterator(TEST_DATA_DIR)) {
            if (entry.path().extension() == ".chunk") {
                files.push_back(entry.path().string());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }
}  // namespace

// Extracts every Data3D payload from the dumped chunks, then benchmarks biome3d.
class Data3dBenchmark : public ::testing::Test {
   protected:
    void SetUp() override {
        for (auto &f : list_chunk_files()) {
            auto raw = bl::utils::read_file(f);
            if (raw.empty()) continue;
            bl::raw_chunk rc;
            if (rc.from_raw(raw)) {
                auto d3d = rc.get_normal_key(bl::chunk_key::Data3D);
                if (!d3d.empty()) {
                    payloads_.push_back(std::move(d3d));
                }
            }
        }
        ASSERT_FALSE(payloads_.empty());
    }

    std::vector<std::string> payloads_;
};

// parse every Data3D payload into biome3d
TEST_F(Data3dBenchmark, LoadD3dAll) {
    constexpr int kRounds = 5;
    auto start = steady_clock_t::now();
    size_t bytes = 0;
    for (int round = 0; round < kRounds; round++) {
        bytes = 0;
        for (auto &p : payloads_) {
            bl::biome3d d3d;
            d3d.set_chunk_pos(bl::chunk_pos{0, 0, 0});
            d3d.set_version(bl::ChunkVersion::New);
            ASSERT_TRUE(d3d.load_from_d3d(reinterpret_cast<const byte_t *>(p.data()), p.size()));
            bytes += p.size();
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "load_from_d3d " << payloads_.size() << " chunks -> " << bytes << " bytes in " << elapsed_ms << " ms ("
              << elapsed_ms / kRounds << " ms/round)\n";
    EXPECT_GT(bytes, 0u);
}

// serialize parsed biome3d back to bytes
TEST_F(Data3dBenchmark, ToRawAll) {
    constexpr int kRounds = 5;
    auto start = steady_clock_t::now();
    size_t bytes = 0;
    for (int round = 0; round < kRounds; round++) {
        bytes = 0;
        for (auto &p : payloads_) {
            bl::biome3d d3d;
            d3d.set_version(bl::ChunkVersion::New);
            ASSERT_TRUE(d3d.load_from_d3d(reinterpret_cast<const byte_t *>(p.data()), p.size()));
            bytes += d3d.to_raw().size();
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "to_raw " << payloads_.size() << " chunks -> " << bytes << " bytes in " << elapsed_ms << " ms (" << elapsed_ms / kRounds
              << " ms/round)\n";
    EXPECT_GT(bytes, 0u);
}

// height map must be copied from the payload, and biome lookups must stay in range
TEST_F(Data3dBenchmark, BasicCorrectness) {
    size_t nonzero_height = 0;
    for (auto &p : payloads_) {
        bl::biome3d d3d;
        d3d.set_chunk_pos(bl::chunk_pos{0, 0, 0});
        d3d.set_version(bl::ChunkVersion::New);
        ASSERT_TRUE(d3d.load_from_d3d(reinterpret_cast<const byte_t *>(p.data()), p.size()));
        auto hm = d3d.height_map();
        for (auto h : hm) {
            if (h != 0) nonzero_height++;
        }
        // every subchunk slot must be reachable through get_biome without OOB
        for (int y = -64; y < 320; y++) {
            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    (void)d3d.get_biome(x, y, z);
                }
            }
        }
    }
    EXPECT_GT(nonzero_height, 0u);
}

// load -> to_raw -> load must be stable (2nd to_raw identical)
TEST_F(Data3dBenchmark, ReSerializeStable) {
    for (auto &p : payloads_) {
        bl::biome3d d1;
        d1.set_version(bl::ChunkVersion::New);
        ASSERT_TRUE(d1.load_from_d3d(reinterpret_cast<const byte_t *>(p.data()), p.size()));
        auto r1 = d1.to_raw();

        bl::biome3d d2;
        d2.set_version(bl::ChunkVersion::New);
        ASSERT_TRUE(d2.load_from_d3d(reinterpret_cast<const byte_t *>(r1.data()), r1.size()));
        EXPECT_EQ(d2.to_raw(), r1);
    }
}
