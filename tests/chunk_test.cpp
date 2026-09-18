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

#include "bedrock_level.h"
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

    // smallest NBT that actor::preload accepts: Pos + identifier + UniqueID
    bl::nbt::compound_tag* make_actor_tag(const char* identifier, float x, float y, float z, int64_t uid) {
        auto* tag = new bl::nbt::compound_tag("");
        tag->put(new bl::nbt::string_tag("identifier", identifier));
        tag->put(new bl::nbt::long_tag("UniqueID", uid));
        auto* pos = new bl::nbt::list_tag("Pos");
        pos->append(new bl::nbt::float_tag("", x));
        pos->append(new bl::nbt::float_tag("", y));
        pos->append(new bl::nbt::float_tag("", z));
        tag->put(pos);
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
    const auto format_before = c.chunk_format();

    set_named_block(c, 3, 5, 7, "minecraft:stone");

    EXPECT_EQ(c.get_block_name(3, 5, 7), "minecraft:stone");
    EXPECT_EQ(c.get_block_name(0, 0, 0), "minecraft:air") << "untouched positions read back as air";
    EXPECT_EQ(c.chunk_format(), format_before) << "creating a sub-chunk must not shift the chunk format";
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
    const auto out = c.to_raw_chunk();

    // to_raw_chunk writes only the sub-chunks that exist, so the range is exactly that one.
    EXPECT_EQ(out.get_y_range(), std::make_pair(16, 31));
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

// to_raw_chunk must produce a self-contained chunk: the version marker the game gates the chunk
// on, a finalized state, and terrain a fresh chunk can read back.
TEST(ChunkBlockEdit, ToRawChunkRoundTrips) {
    const bl::chunk_pos pos(0, 0, 0);
    bl::chunk c(pos);
    set_named_block(c, 3, 4, 5, "minecraft:stone");
    const auto raw = c.to_raw_chunk();

    EXPECT_FALSE(raw.get_sub_chunk(0).empty()) << "terrain must have been written";
    ASSERT_EQ(raw.get_normal_key(bl::chunk_key::VersionNew).size(), 1u) << "the version marker gates the chunk";
    EXPECT_EQ(static_cast<unsigned char>(raw.get_normal_key(bl::chunk_key::VersionNew)[0]), static_cast<unsigned char>(c.chunk_format()));
    EXPECT_EQ(raw.chunk_format(), c.chunk_format());
    ASSERT_EQ(raw.get_normal_key(bl::chunk_key::FinalizedState).size(), 4u);
    EXPECT_EQ(raw.get_normal_key(bl::chunk_key::FinalizedState)[0], 2) << "FinalizedState is an int32 fixed at 2";

    bl::chunk reloaded(pos);
    ASSERT_TRUE(reloaded.load_from_raw_chunk(raw, bl::chunk_load_policy::Terrain));
    EXPECT_EQ(reloaded.get_block_name(3, 4, 5), "minecraft:stone");
    EXPECT_EQ(reloaded.get_block_name(0, 0, 0), "minecraft:air");
}

// An old-format chunk keeps its layout: the version marker is the legacy key and the sub-chunks
// it builds inherit the v8 header.
TEST(ChunkBlockEdit, ToRawChunkKeepsOldFormat) {
    bl::raw_chunk source(bl::chunk_pos(0, 0, 0));
    source.set_chunk_format(bl::LevelChunkFormat::V1_12_0);

    bl::chunk c(bl::chunk_pos(0, 0, 0));
    ASSERT_TRUE(c.load_from_raw_chunk(source, bl::chunk_load_policy::Terrain));
    set_named_block(c, 1, 5, 1, "minecraft:stone");
    const auto raw = c.to_raw_chunk();

    EXPECT_TRUE(raw.get_normal_key(bl::chunk_key::VersionNew).empty()) << "V1_12_0 is not a new-format chunk";
    ASSERT_EQ(raw.get_normal_key(bl::chunk_key::VersionOld).size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(raw.get_normal_key(bl::chunk_key::VersionOld)[0]),
              static_cast<unsigned char>(bl::LevelChunkFormat::V1_12_0));
    EXPECT_EQ(raw.chunk_format(), bl::LevelChunkFormat::V1_12_0);
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

    const auto raw = c.to_raw_chunk();

    // 4096 un-deduplicated appends would be ~100 KB; compacted this is a couple hundred bytes.
    EXPECT_GT(raw.get_sub_chunk(0).size(), 0u);
    EXPECT_LT(raw.get_sub_chunk(0).size(), 200u) << "to_raw_chunk did not compact";
}

// A block entity written through set_block_entity has to reach the raw_chunk payload, which
// is how an imported structure gets its chests and signs into the level. Positions in that
// payload are world-space.
TEST(ChunkBlockEdit, ToRawChunkWritesBlockEntities) {
    const bl::chunk_pos pos(2, -3, 0);
    bl::raw_chunk raw(pos);

    bl::chunk c(pos);
    ASSERT_TRUE(c.load_from_raw_chunk(raw));  // block entities must be part of the load
    auto* chest = make_block_tag("minecraft:chest");
    c.set_block_entity(3, 70, 4, chest);
    delete chest;
    const auto out = c.to_raw_chunk();

    const auto payload = out.get_normal_key(bl::chunk_key::BlockEntity);
    ASSERT_FALSE(payload.empty()) << "block entities must be written back";
    auto stored = bl::nbt::read_palette_to_end(payload.data(), payload.size());
    ASSERT_EQ(stored.size(), 1u);
    EXPECT_EQ(stored[0]->get("x")->as<bl::nbt::int_tag*>()->value, 2 * 16 + 3);
    EXPECT_EQ(stored[0]->get("y")->as<bl::nbt::int_tag*>()->value, 70);
    EXPECT_EQ(stored[0]->get("z")->as<bl::nbt::int_tag*>()->value, -3 * 16 + 4);
    for (auto* tag : stored) delete tag;
}

// A chunk loaded without chunk_load_policy::BlockActor never saw the payload, so it must not
// appear in the output: raw_chunk::write only touches the keys a chunk holds, which is what
// keeps the stored payload in the level alive.
TEST(ChunkBlockEdit, ToRawChunkSkipsUnloadedBlockEntities) {
    const bl::chunk_pos pos(0, 0, 0);
    bl::raw_chunk raw(pos);
    raw.set_normal(bl::chunk_key::BlockEntity, "existing-payload");

    bl::chunk c(pos);
    ASSERT_TRUE(c.load_from_raw_chunk(raw, bl::chunk_load_policy::Terrain));
    const auto out = c.to_raw_chunk();

    EXPECT_TRUE(out.get_normal_key(bl::chunk_key::BlockEntity).empty());
}

// Rewriting one position leaves the last write as the live one, so importing over an existing
// block entity replaces it instead of stacking a second one next to it.
TEST(ChunkBlockEdit, SetBlockEntityReplacesSamePosition) {
    const bl::chunk_pos pos(0, 0, 0);
    bl::chunk c(pos);
    auto* first = make_block_tag("minecraft:chest");
    first->put(new bl::nbt::string_tag("custom", "old"));
    auto* second = make_block_tag("minecraft:chest");
    second->put(new bl::nbt::string_tag("custom", "new"));

    c.set_block_entity(1, 60, 1, first);
    c.set_block_entity(1, 60, 1, second);
    delete first;
    delete second;

    c.compact();
    ASSERT_EQ(c.block_entities().size(), 1u);
    EXPECT_EQ(c.block_entities()[0]->get("custom")->as<bl::nbt::string_tag*>()->value, "new");
}

// A copied-in entity must not keep the id of the actor it was exported from, or the level would
// hold two actors sharing one storage key.
TEST(ChunkBlockEdit, AddActorAssignsNewUniqueId) {
    bl::bedrock_level level;
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    auto* source = make_actor_tag("minecraft:cow", 10.0f, 64.0f, -20.0f, 12345);

    ASSERT_TRUE(c.add_actor(level, source, {42.0f, 69.0f, -36.0f}));
    ASSERT_EQ(c.entities().size(), 1u);
    auto* added = c.entities()[0];
    EXPECT_NE(added->uid(), 12345);
    EXPECT_EQ(added->identifier(), "minecraft:cow");
    EXPECT_EQ(added->root()->get("UniqueID")->as<bl::nbt::long_tag*>()->value, added->uid())
        << "the NBT UniqueID must match the uid the actor reports";

    // the requested position wins, whatever the tag claimed
    EXPECT_FLOAT_EQ(added->pos().x, 42.0f);
    EXPECT_FLOAT_EQ(added->pos().y, 69.0f);
    EXPECT_FLOAT_EQ(added->pos().z, -36.0f);
    auto* added_pos = added->root()->get("Pos")->as<bl::nbt::list_tag*>();
    EXPECT_FLOAT_EQ(added_pos->value[0]->as<bl::nbt::float_tag*>()->value, 42.0f);
    EXPECT_FLOAT_EQ(added_pos->value[1]->as<bl::nbt::float_tag*>()->value, 69.0f);
    EXPECT_FLOAT_EQ(added_pos->value[2]->as<bl::nbt::float_tag*>()->value, -36.0f);

    // the source tag is the caller's and must come back untouched
    EXPECT_EQ(source->get("UniqueID")->as<bl::nbt::long_tag*>()->value, 12345);
    auto* source_pos = source->get("Pos")->as<bl::nbt::list_tag*>();
    EXPECT_FLOAT_EQ(source_pos->value[0]->as<bl::nbt::float_tag*>()->value, 10.0f);
    EXPECT_FLOAT_EQ(source_pos->value[1]->as<bl::nbt::float_tag*>()->value, 64.0f);
    EXPECT_FLOAT_EQ(source_pos->value[2]->as<bl::nbt::float_tag*>()->value, -20.0f);

    // a second copy of the same tag is a second, independent actor
    ASSERT_TRUE(c.add_actor(level, source, {10.0f, 64.0f, -20.0f}));
    ASSERT_EQ(c.entities().size(), 2u);
    EXPECT_NE(c.entities()[1]->uid(), 12345);
    EXPECT_NE(c.entities()[1]->uid(), added->uid());
    EXPECT_FLOAT_EQ(c.entities()[1]->pos().x, 10.0f);

    delete source;
}

// A tag that actor::preload cannot accept leaves the chunk alone instead of adding half an actor.
TEST(ChunkBlockEdit, AddActorRejectsIncompleteTag) {
    bl::bedrock_level level;
    bl::chunk c(bl::chunk_pos(0, 0, 0));

    auto* no_uid = make_actor_tag("minecraft:cow", 0.0f, 64.0f, 0.0f, 1);
    no_uid->remove("UniqueID");
    EXPECT_FALSE(c.add_actor(level, no_uid, {0.0f, 64.0f, 0.0f}));

    auto* no_pos = make_actor_tag("minecraft:cow", 0.0f, 64.0f, 0.0f, 1);
    no_pos->remove("Pos");
    EXPECT_FALSE(c.add_actor(level, no_pos, {0.0f, 64.0f, 0.0f}));

    EXPECT_FALSE(c.add_actor(level, nullptr, {0.0f, 64.0f, 0.0f}));
    EXPECT_TRUE(c.entities().empty());

    delete no_uid;
    delete no_pos;
}

// An actor added through add_actor has to reach the raw_chunk, which is how the level indexes
// entities: a new-version chunk stores one "actorprefix<key>" entry per actor plus a digest.
TEST(ChunkBlockEdit, ToRawChunkWritesActors) {
    const bl::chunk_pos pos(0, 0, 0);
    bl::raw_chunk source(pos);
    // built from scratch, so nothing told it which entity layout to write
    source.set_chunk_format(bl::LevelChunkFormat::V1_18_3IndividualActorStorage);

    bl::bedrock_level level;
    bl::chunk c(pos);
    ASSERT_TRUE(c.load_from_raw_chunk(source, bl::chunk_load_policy::Terrain | bl::chunk_load_policy::Actor));
    ASSERT_EQ(c.chunk_format(), bl::LevelChunkFormat::V1_18_3IndividualActorStorage);

    auto* pig = make_actor_tag("minecraft:pig", 8.0f, 70.0f, 8.0f, 777);
    ASSERT_TRUE(c.add_actor(level, pig, {8.5f, 70.0f, 9.5f}));
    delete pig;
    const auto raw = c.to_raw_chunk();

    ASSERT_EQ(raw.get_entities().size(), 1u) << "the actor must be indexed by its storage key";
    ASSERT_EQ(raw.get_actor_digest().size(), 8u) << "one actor means one 8-byte digest entry";
    EXPECT_EQ(raw.get_actor_digest(), raw.get_entities().begin()->first);

    const auto& stored = raw.get_entities().begin()->second;
    auto tags = bl::nbt::read_palette_to_end(stored.data(), stored.size());
    ASSERT_EQ(tags.size(), 1u);
    EXPECT_EQ(tags[0]->get("identifier")->as<bl::nbt::string_tag*>()->value, "minecraft:pig");
    EXPECT_NE(tags[0]->get("UniqueID")->as<bl::nbt::long_tag*>()->value, 777);
    auto* stored_pos = tags[0]->get("Pos")->as<bl::nbt::list_tag*>();
    EXPECT_FLOAT_EQ(stored_pos->value[0]->as<bl::nbt::float_tag*>()->value, 8.5f);
    EXPECT_FLOAT_EQ(stored_pos->value[2]->as<bl::nbt::float_tag*>()->value, 9.5f);
    // the digest must describe this actor, or the level will not find it
    EXPECT_EQ(raw.get_entities().begin()->first, c.entities()[0]->storage_key_raw());

    // ... and the level must be able to read it back as a chunk entity
    bl::chunk reloaded(pos);
    ASSERT_TRUE(reloaded.load_from_raw_chunk(raw));
    ASSERT_EQ(reloaded.entities().size(), 1u);
    EXPECT_EQ(reloaded.entities()[0]->identifier(), "minecraft:pig");
    for (auto* tag : tags) delete tag;
}

// Same guard as block entities: a chunk loaded without chunk_load_policy::Actor never saw the
// entity payload, so it must not appear in the output and end up overwriting the stored one.
TEST(ChunkBlockEdit, ToRawChunkSkipsUnloadedActors) {
    const bl::chunk_pos pos(0, 0, 0);
    bl::raw_chunk source(pos);

    bl::bedrock_level level;
    bl::chunk writer(pos);
    ASSERT_TRUE(writer.load_from_raw_chunk(source, bl::chunk_load_policy::Terrain | bl::chunk_load_policy::Actor));
    auto* pig = make_actor_tag("minecraft:pig", 8.0f, 70.0f, 8.0f, 777);
    ASSERT_TRUE(writer.add_actor(level, pig, {8.0f, 70.0f, 8.0f}));
    delete pig;
    const auto loaded = writer.to_raw_chunk();
    ASSERT_EQ(loaded.get_entities().size(), 1u);

    bl::chunk terrain_only(pos);
    ASSERT_TRUE(terrain_only.load_from_raw_chunk(loaded, bl::chunk_load_policy::Terrain));
    set_named_block(terrain_only, 0, 0, 0, "minecraft:stone");
    const auto edited = terrain_only.to_raw_chunk();

    EXPECT_TRUE(edited.get_entities().empty()) << "an edit that never read the entities must not write them";
    EXPECT_TRUE(edited.get_actor_digest().empty());
}

// Bytes produced by to_raw_chunk must be a valid SubChunkTerrain payload on their own.
TEST(ChunkBlockEdit, ToRawChunkOutputIsLoadable) {
    bl::chunk c(bl::chunk_pos(0, 0, 0));
    set_named_block(c, 1, -1, 2, "minecraft:gold_block");

    const auto raw = c.to_raw_chunk();

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
