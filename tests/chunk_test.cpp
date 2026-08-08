//
// Created by xhy on 2023/4/2.
//

#include "chunk.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "utils.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

namespace fs = std::filesystem;

void check_map(int y, int index, int offset) {
    int i, o;
    bl::chunk::map_y_to_subchunk(y, i, o);
    EXPECT_TRUE(i == index && o == offset);
}

TEST(Chunk, SubIndexMapping) {
    check_map(-64, -4, 0);
    check_map(-63, -4, 1);
    check_map(-49, -4, 15);
    check_map(-48, -3, 0);
    check_map(-47, -3, 1);
    check_map(-48, -3, 0);
    check_map(-16, -1, 0);
    check_map(-1, -1, 15);
    check_map(0, 0, 0);
    check_map(1, 0, 1);
    check_map(15, 0, 15);
    check_map(16, 1, 0);
}

namespace {
    using steady_clock_t = std::chrono::steady_clock;

    std::vector<std::string> list_chunk_files() {
        std::vector<std::string> files;
        for (auto &entry : fs::directory_iterator(TEST_DATA_DIR "/chunks")) {
            if (entry.path().extension() == ".chunk") {
                files.push_back(entry.path().string());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }
}  // namespace

// Loads all dumped BCHK chunks once, then benchmarks chunk serialize/deserialize.
class ChunkBenchmark : public ::testing::Test {
   protected:
    void SetUp() override {
        for (auto &f : list_chunk_files()) {
            auto raw = bl::utils::read_file(f);
            if (raw.empty()) continue;
            bl::raw_chunk rc;
            if (rc.from_raw(raw)) {
                raw_bytes_.push_back(std::move(raw));
                chunks_.push_back(std::move(rc));
            }
        }
        ASSERT_FALSE(chunks_.empty());
    }

    std::vector<std::vector<byte_t>> raw_bytes_;
    std::vector<bl::raw_chunk> chunks_;
};

// BCHK deserialize: bytes -> raw_chunk
TEST_F(ChunkBenchmark, FromRawAll) {
    constexpr int kRounds = 5;
    auto start = steady_clock_t::now();
    size_t total_bytes = 0;
    for (int round = 0; round < kRounds; round++) {
        total_bytes = 0;
        for (auto &raw : raw_bytes_) {
            bl::raw_chunk rc;
            ASSERT_TRUE(rc.from_raw(raw));
            total_bytes += raw.size();
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "from_raw " << raw_bytes_.size() << " chunks -> " << total_bytes << " bytes in " << elapsed_ms << " ms ("
              << elapsed_ms / kRounds << " ms/round)\n";
    EXPECT_GT(total_bytes, 0u);
}

// BCHK serialize: raw_chunk -> bytes
TEST_F(ChunkBenchmark, ToRawAll) {
    constexpr int kRounds = 5;
    auto start = steady_clock_t::now();
    size_t total_bytes = 0;
    for (int round = 0; round < kRounds; round++) {
        total_bytes = 0;
        for (auto &rc : chunks_) {
            total_bytes += rc.to_raw().size();
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "to_raw " << chunks_.size() << " chunks -> " << total_bytes << " bytes in " << elapsed_ms << " ms ("
              << elapsed_ms / kRounds << " ms/round)\n";
    EXPECT_GT(total_bytes, 0u);
}

// from_raw -> to_raw must reproduce the input bytes exactly
TEST_F(ChunkBenchmark, RoundTripBytes) {
    size_t ok = 0;
    for (auto &raw : raw_bytes_) {
        bl::raw_chunk rc;
        ASSERT_TRUE(rc.from_raw(raw));
        auto reencoded = rc.to_raw();
        if (reencoded == raw) ok++;
    }
    std::cout << "round trip ok: " << ok << "/" << raw_bytes_.size() << "\n";
    EXPECT_EQ(ok, raw_bytes_.size());
}

// deep parse: raw_chunk -> chunk (subchunks, biomes, entities, NBT palettes)
TEST_F(ChunkBenchmark, LoadFromRawAll) {
    constexpr int kRounds = 5;
    auto start = steady_clock_t::now();
    size_t loaded = 0;
    for (int round = 0; round < kRounds; round++) {
        loaded = 0;
        for (auto &rc : chunks_) {
            auto *c = new bl::chunk(rc.pos());
            if (c->load_from_raw_chunk(rc)) loaded++;
            delete c;
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "load_from_raw_chunk " << chunks_.size() << " chunks -> " << loaded << " loaded in " << elapsed_ms << " ms ("
              << elapsed_ms / kRounds << " ms/round)\n";
    EXPECT_GT(loaded, 0u);
}

// render hot path: per-column top-down block scan (get_top_y + get_block_fast)
TEST_F(ChunkBenchmark, ScanBlocksFast) {
    std::vector<bl::chunk *> loaded;
    for (auto &rc : chunks_) {
        auto *c = new bl::chunk(rc.pos());
        if (c->load_from_raw_chunk(rc)) {
            loaded.push_back(c);
        } else {
            delete c;
        }
    }
    ASSERT_FALSE(loaded.empty());

    constexpr int kRounds = 3;
    auto start = steady_clock_t::now();
    size_t total = 0;
    for (int round = 0; round < kRounds; round++) {
        total = 0;
        for (auto *c : loaded) {
            for (int cx = 0; cx < 16; cx++) {
                for (int cz = 0; cz < 16; cz++) {
                    auto [top, solid] = c->get_top_y(cx, cz, 320);
                    for (int y = top; y >= 0; y--) {
                        auto b = c->get_block_fast(cx, y, cz);
                        total += b.name.size();
                    }
                }
            }
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "scan blocks " << loaded.size() << " chunks in " << elapsed_ms << " ms (" << elapsed_ms / kRounds << " ms/round, ~"
              << total / 16u << " blocks scanned)\n";
    EXPECT_GT(total, 0u);
    for (auto *c : loaded) delete c;
}

// get_block_name must match get_block_fast, and miss outside the world must return "minecraft:unknown"
TEST_F(ChunkBenchmark, BlockNameConsistent) {
    for (auto &rc : chunks_) {
        auto *c = new bl::chunk(rc.pos());
        if (!c->load_from_raw_chunk(rc)) {
            delete c;
            continue;
        }
        for (int cx = 0; cx < 16; cx++) {
            for (int cz = 0; cz < 16; cz++) {
                for (int y = -64; y < 320; y++) {
                    EXPECT_EQ(c->get_block_name(cx, y, cz), c->get_block_fast(cx, y, cz).name);
                }
            }
        }
        // empty subchunk slot (e.g. y below world) -> unknown name, no crash
        EXPECT_EQ(c->get_block_name(0, -500, 0), "minecraft:unknown");
        delete c;
    }
}
