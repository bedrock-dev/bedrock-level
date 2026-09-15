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
        for (auto& entry : fs::directory_iterator(TEST_DATA_DIR "/chunks")) {
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
        for (auto& f : list_chunk_files()) {
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
        for (auto& raw : raw_bytes_) {
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
        for (auto& rc : chunks_) {
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
    for (auto& raw : raw_bytes_) {
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
        for (auto& rc : chunks_) {
            auto* c = new bl::chunk(rc.pos());
            if (c->load_from_raw_chunk(rc)) loaded++;
            delete c;
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "load_from_raw_chunk " << chunks_.size() << " chunks -> " << loaded << " loaded in " << elapsed_ms << " ms ("
              << elapsed_ms / kRounds << " ms/round)\n";
    EXPECT_GT(loaded, 0u);
}

// render hot path: per-column top-down block scan (get_top_y + get_block_name)
TEST_F(ChunkBenchmark, ScanBlocksFast) {
    std::vector<bl::chunk*> loaded;
    for (auto& rc : chunks_) {
        auto* c = new bl::chunk(rc.pos());
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
        for (auto* c : loaded) {
            for (int cx = 0; cx < 16; cx++) {
                for (int cz = 0; cz < 16; cz++) {
                    auto [top, solid] = c->get_top_y(cx, cz, 320);
                    for (int y = top; y >= 0; y--) {
                        const auto& name = c->get_block_name(cx, y, cz);
                        total += name.size();
                    }
                }
            }
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "scan blocks " << loaded.size() << " chunks in " << elapsed_ms << " ms (" << elapsed_ms / kRounds << " ms/round, ~"
              << total / 16u << " blocks scanned)\n";
    EXPECT_GT(total, 0u);
    for (auto* c : loaded) delete c;
}

// get_block_name must match get_block_with_color, and miss outside the world must return "minecraft:unknown"
TEST_F(ChunkBenchmark, BlockNameConsistent) {
    for (auto& rc : chunks_) {
        auto* c = new bl::chunk(rc.pos());
        if (!c->load_from_raw_chunk(rc)) {
            delete c;
            continue;
        }
        for (int cx = 0; cx < 16; cx++) {
            for (int cz = 0; cz < 16; cz++) {
                for (int y = -64; y < 320; y++) {
                    EXPECT_EQ(c->get_block_name(cx, y, cz), c->get_block_with_color(cx, y, cz).name);
                }
            }
        }
        // empty subchunk slot (e.g. y below world) -> unknown name, no crash
        EXPECT_EQ(c->get_block_name(0, -500, 0), "minecraft:unknown");
        delete c;
    }
}

namespace {
    bl::nbt::compound_tag* make_block_tag(const std::string& name) {
        auto* tag = new bl::nbt::compound_tag("");
        tag->put(new bl::nbt::string_tag("name", name));
        return tag;
    }

    void set_named_block(bl::chunk& c, int cx, int y, int cz, const char* name, int layer = 0) {
        auto* tag = make_block_tag(name);
        c.set_block(cx, y, cz, tag, layer);
        delete tag;
    }
}  // namespace

// set_block must create the sub-chunk that holds the Y when the chunk has none there.
TEST(ChunkBlockEdit, SetBlockCreatesMissingSubChunk) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    const auto version_before = c.get_version();

    set_named_block(c, 3, 5, 7, "minecraft:stone");

    EXPECT_EQ(c.get_block_name(3, 5, 7), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(0, 0, 0), "minecraft:air") << "untouched positions read back as air";
    EXPECT_EQ(c.get_version(), version_before) << "creating a sub-chunk must not shift the chunk version";
}

// Y maps onto different sub-chunks, including negative indexes on 1.18+ worlds.
TEST(ChunkBlockEdit, SetBlockAcrossYIndexes) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    set_named_block(c, 1, -1, 1, "minecraft:stone");       // sub-chunk -1
    set_named_block(c, 1, 0, 1, "minecraft:dirt");         // sub-chunk 0
    set_named_block(c, 1, 16, 1, "minecraft:gold_block");  // sub-chunk 1

    EXPECT_EQ(c.get_block_name(1, -1, 1), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(1, 0, 1), "minecraft:dirt");
    EXPECT_EQ(c.get_block_name(1, 16, 1), "minecraft:gold_block");
}

// The layer argument reaches the right layer.
TEST(ChunkBlockEdit, SetBlockLayer) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    set_named_block(c, 2, 2, 2, "minecraft:water", 1);

    EXPECT_EQ(c.get_block_name(2, 2, 2, 1), "minecraft:water");
    EXPECT_EQ(c.get_block_name(2, 2, 2, 0), "minecraft:air");
}

// chunk::compact() must reach every sub-chunk, so terrain built by repeated set_block
// writes compactly instead of carrying thousands of unreferenced palette entries.
TEST(ChunkBlockEdit, CompactForwardsToEverySubChunk) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    // Three different sub-chunks (-1, 0, 1) all filled with the same block through set_block.
    for (int y = -1; y <= 16; y++) {
        for (int x = 0; x < 2; x++) {
            for (int z = 0; z < 2; z++) {
                set_named_block(c, x, y, z, "minecraft:stone");
            }
        }
    }

    c.compact();

    EXPECT_EQ(c.get_block_name(0, -1, 0), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(0, 0, 0), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(0, 16, 0), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(1, 16, 1), "minecraft:stone");
}

// get_y_range comes from the sub-chunks actually present, so a chunk is free to hold any
// number of them and blocks written outside the usual world height still report correctly.
TEST(ChunkBlockEdit, YRangeFollowsActualSubChunks) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    EXPECT_EQ(c.get_y_range(), std::make_pair(0, -1)) << "a chunk with no terrain has an empty range";

    set_named_block(c, 0, 0, 0, "minecraft:stone");
    EXPECT_EQ(c.get_y_range(), std::make_pair(0, 15)) << "one sub-chunk covers y 0..15";

    set_named_block(c, 0, -1, 0, "minecraft:stone");
    EXPECT_EQ(c.get_y_range(), std::make_pair(-16, 15)) << "sub-chunk -1 extends the range down";

    // well past the vanilla overworld ceiling: nothing limits how many sub-chunks a chunk holds
    set_named_block(c, 0, 480, 0, "minecraft:stone");
    EXPECT_EQ(c.get_y_range(), std::make_pair(-16, 495));
}

// raw_chunk::get_y_range must ignore the empty payloads that the read window inserts.
TEST(ChunkBlockEdit, RawChunkYRangeIgnoresEmptyPayloads) {
    bl::raw_chunk raw(bl::chunk_pos(0, 0, 0));
    EXPECT_EQ(raw.get_y_range(), std::make_pair(0, -1));

    bl::chunk c(bl::chunk_pos(0, 0, 0));
    set_named_block(c, 0, 20, 0, "minecraft:stone");  // sub-chunk 1 only
    c.to_raw_chunk(raw);

    // to_raw_chunk writes only the sub-chunks that exist, so the range is exactly that one.
    EXPECT_EQ(raw.get_y_range(), std::make_pair(16, 31));
}

// compact() on a chunk loaded from a real save must be idempotent and preserve the terrain.
TEST_F(ChunkBenchmark, CompactLoadedChunkIsStable) {
    ASSERT_FALSE(chunks_.empty());
    size_t checked = 0;
    for (auto& rc : chunks_) {
        if (checked >= 5) break;
        auto* c = new bl::chunk(rc.pos());
        if (!c->load_from_raw_chunk(rc)) {
            delete c;
            continue;
        }

        // sample before, compact twice, sample after
        std::vector<std::string> before;
        for (int y = -64; y < 320; y += 37) before.push_back(c->get_block_name(3, y, 5));

        c->compact();
        c->compact();

        size_t i = 0;
        for (int y = -64; y < 320; y += 37) {
            EXPECT_EQ(c->get_block_name(3, y, 5), before[i]) << "y=" << y;
            i++;
        }
        checked++;
        delete c;
    }
}

// to_raw_chunk must write terrain a fresh chunk can read back, and must not disturb the
// non-terrain keys that were already in the raw_chunk.
TEST(ChunkBlockEdit, ToRawChunkRoundTripsAndKeepsOtherKeys) {
    const bl::chunk_pos pos(0, 0, 0);
    bl::raw_chunk raw(pos);
    raw.set_normal(bl::chunk_key::PendingTicks, "sentinel-pending-ticks");

    bl::chunk c(pos);
    set_named_block(c, 3, 4, 5, "minecraft:stone");
    c.to_raw_chunk(raw);

    EXPECT_FALSE(raw.get_sub_chunk(0).empty()) << "terrain must have been written";
    EXPECT_EQ(raw.get_normal_key(bl::chunk_key::PendingTicks), "sentinel-pending-ticks") << "non-terrain keys must survive";

    bl::chunk reloaded(pos);
    ASSERT_TRUE(reloaded.load_from_raw_chunk(raw, bl::chunk_load_policy::Terrain));
    EXPECT_EQ(reloaded.get_block_name(3, 4, 5), "minecraft:stone");
    EXPECT_EQ(reloaded.get_block_name(0, 0, 0), "minecraft:air");
}

// to_raw_chunk compacts, so terrain built by per-block set_block writes in compact form.
TEST(ChunkBlockEdit, ToRawChunkCompacts) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            for (int y = 0; y < 16; y++) {
                set_named_block(c, x, y, z, "minecraft:stone");
            }
        }
    }

    bl::raw_chunk raw(bl::chunk_pos(0, 0, 0));
    c.to_raw_chunk(raw);

    // 4096 un-deduplicated appends would be ~100 KB; compacted this is a couple hundred bytes.
    EXPECT_GT(raw.get_sub_chunk(0).size(), 0u);
    EXPECT_LT(raw.get_sub_chunk(0).size(), 200u) << "to_raw_chunk did not compact";
}

// Bytes produced by to_raw_chunk must be a valid SubChunkTerrain payload on their own.
TEST(ChunkBlockEdit, ToRawChunkOutputIsLoadable) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    set_named_block(c, 1, -1, 2, "minecraft:gold_block");

    bl::raw_chunk raw(bl::chunk_pos(0, 0, 0));
    c.to_raw_chunk(raw);

    const auto payload = raw.get_sub_chunk(-1);
    ASSERT_FALSE(payload.empty());

    bl::sub_chunk sub;
    sub.set_y_index(-1);
    ASSERT_TRUE(sub.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()));
    EXPECT_EQ(sub.version(), static_cast<uint8_t>(bl::SubChunkVersion::V9));
    EXPECT_EQ(sub.y_index(), -1);
    EXPECT_EQ(sub.get_block_name(1, 15, 2, 0), "minecraft:gold_block") << "y=-1 is offset 15 of sub-chunk -1";
}

// chunk::fill_blocks maps world-Y onto sub-chunks, so a box that spans a sub-chunk
// boundary has to land on both sides.
TEST(ChunkBlockEdit, FillBlocksCrossesSubChunkBoundary) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));

    // sub-chunk -1 covers y -16..-1, so this box straddles the -1 / 0 boundary.
    const bl::block_box box{{0, -2, 0}, {2, 2, 2}};
    c.fill_blocks(box, make_block_tag("minecraft:stone"), 0);

    EXPECT_EQ(c.get_block_name(0, -2, 0), "minecraft:stone") << "last layer of sub-chunk -1";
    EXPECT_EQ(c.get_block_name(0, -1, 0), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(0, 0, 0), "minecraft:stone") << "first layer of sub-chunk 0";
    EXPECT_EQ(c.get_block_name(0, 1, 0), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(0, 2, 0), "minecraft:air") << "max y is excluded";
    EXPECT_EQ(c.get_block_name(2, 0, 0), "minecraft:air") << "max x is excluded";
}

// chunk::fill_blocks clips x/z to the chunk and ignores a zero-size box.
TEST(ChunkBlockEdit, FillBlocksClipsHorizontally) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    c.fill_blocks(bl::block_box{{-5, 0, -5}, {3, 1, 3}}, make_block_tag("minecraft:stone"), 0);

    EXPECT_EQ(c.get_block_name(0, 0, 0), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(2, 0, 2), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(3, 0, 0), "minecraft:air") << "max x is excluded";

    // A zero-size box has nothing to fill. (An inverted box is not a no-op: block_box is
    // normalized first, so min/max are swapped and it fills the swapped range.)
    c.fill_blocks(bl::block_box{{4, 0, 4}, {4, 0, 4}}, make_block_tag("minecraft:dirt"), 0);
    EXPECT_EQ(c.get_block_name(4, 0, 4), "minecraft:air");
}

// layer < 0 never creates terrain: a box over Y with no sub-chunk does nothing.
// An explicit layer index does create what is missing so the fill lands.
TEST(ChunkBlockEdit, FillBlocksLayerControlsCreation) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    const bl::block_box box{{0, 0, 0}, {2, 2, 2}};

    c.fill_blocks(box, make_block_tag("minecraft:stone"));  // default layer -1
    EXPECT_EQ(c.get_block_name(0, 0, 0), "minecraft:unknown") << "layer < 0 must not create a sub-chunk";

    c.fill_blocks(box, make_block_tag("minecraft:stone"), 0);
    EXPECT_EQ(c.get_block_name(0, 0, 0), "minecraft:stone");
}

// A box fill must not disturb blocks outside it, including in a loaded save.
TEST_F(ChunkBenchmark, FillBlocksIntoLoadedChunk) {
    ASSERT_FALSE(chunks_.empty());

    size_t checked = 0;
    for (auto& rc : chunks_) {
        if (checked >= 5) break;
        auto* c = new bl::chunk(rc.pos());
        if (!c->load_from_raw_chunk(rc)) {
            delete c;
            continue;
        }

        // find a column with a solid block, then fill a 1x1x1 box on its x/z neighbours
        bool filled = false;
        for (int y = 0; y < 320 && !filled; y++) {
            const auto& name = c->get_block_name(0, y, 0);
            if (name == "minecraft:unknown" || name == "minecraft:air") continue;
            const std::string neighbour_before = c->get_block_name(5, y, 5);

            c->fill_blocks(bl::block_box{{0, y, 0}, {1, y + 1, 1}}, make_block_tag("minecraft:diamond_block"), 0);

            EXPECT_EQ(c->get_block_name(0, y, 0), "minecraft:diamond_block");
            EXPECT_EQ(c->get_block_name(5, y, 5), neighbour_before) << "outside the box must not change";
            filled = true;
        }
        EXPECT_TRUE(filled) << "no solid block found in " << rc.pos().to_string();
        checked++;
        delete c;
    }
}

// Writing into a chunk that already has terrain must reuse its sub-chunks rather than
// shadow them, and leave the neighbouring blocks alone.
TEST_F(ChunkBenchmark, SetBlockIntoLoadedChunk) {
    ASSERT_FALSE(chunks_.empty());

    size_t checked = 0;
    for (auto& rc : chunks_) {
        if (checked >= 5) break;
        auto* c = new bl::chunk(rc.pos());
        if (!c->load_from_raw_chunk(rc)) {
            delete c;
            continue;
        }

        // find a solid block and overwrite exactly that one
        bool overwritten = false;
        for (int y = 0; y < 320 && !overwritten; y++) {
            const std::string neighbour_before = c->get_block_name(1, y, 0);
            const auto& name = c->get_block_name(0, y, 0);
            if (name == "minecraft:unknown" || name == "minecraft:air") continue;

            set_named_block(*c, 0, y, 0, "minecraft:diamond_block");

            EXPECT_EQ(c->get_block_name(0, y, 0), "minecraft:diamond_block");
            EXPECT_EQ(c->get_block_name(1, y, 0), neighbour_before) << "the neighbouring block must not change";
            overwritten = true;
        }
        EXPECT_TRUE(overwritten) << "no solid block found in " << rc.pos().to_string();
        checked++;
        delete c;
    }
}
