//
// Round-trip tests for sub_chunk serialization (write_layer + sub_chunk::to_raw).
//

#include "sub_chunk.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "chunk.h"
#include "palette.h"
#include "utils.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

namespace fs = std::filesystem;

namespace {

    // y varies fastest, then z, then x �?matches sub_chunk::palette_entry_at.
    int block_index(int x, int y, int z) { return y + z * 16 + x * 256; }

    bl::nbt::compound_tag* make_block_tag(const std::string& name, int state = -1) {
        auto* tag = new bl::nbt::compound_tag("");
        tag->put(new bl::nbt::string_tag("name", name));
        if (state >= 0) {
            auto* states = new bl::nbt::compound_tag("states");
            states->put(new bl::nbt::int_tag("value", state));
            tag->put(states);
        }
        return tag;
    }

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

    // Builds a single-layer payload whose blocks cycle through the palette.
    // version 8 has no Y index; version 9 stores it as the third header byte.
    std::string build_payload(const std::vector<std::string>& names, const std::vector<uint16_t>& blocks, int8_t y_index,
                              uint8_t version = static_cast<uint8_t>(bl::SubChunkVersion::V9)) {
        std::vector<bl::palette_entry> palette;
        palette.reserve(names.size());
        for (const auto& name : names) {
            palette.push_back(bl::make_palette_entry(make_block_tag(name)));
        }

        std::string payload;
        payload.push_back(static_cast<char>(version));
        payload.push_back(static_cast<char>(1));  // one layer
        if (version == static_cast<uint8_t>(bl::SubChunkVersion::V9)) {
            payload.push_back(static_cast<char>(y_index));
        }
        bl::write_layer(payload, blocks, palette);

        for (auto& entry : palette) delete entry.tag;
        return payload;
    }

}  // namespace

// Exercises the bits values seen in real saves, including the widths where the
// word-count padding in write_layer / read_block_indices can disagree.
TEST(SubChunkRoundTrip, PaletteWidths) {
    const int entry_counts[] = {1, 2, 3, 4, 5, 8, 9, 16, 17, 40, 158, 256};
    for (int count : entry_counts) {
        std::vector<std::string> names;
        for (int i = 0; i < count; i++) names.push_back("minecraft:test_block_" + std::to_string(i));

        std::vector<uint16_t> blocks(4096);
        for (int i = 0; i < 4096; i++) blocks[i] = static_cast<uint16_t>(i % count);

        const auto payload = build_payload(names, blocks, 3);

        bl::sub_chunk sc;
        sc.set_y_index(3);
        ASSERT_TRUE(sc.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()))
            << "count=" << count << " size=" << payload.size();

        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++) {
                for (int z = 0; z < 16; z++) {
                    const int idx = block_index(x, y, z);
                    const auto& expected = names[blocks[idx]];
                    ASSERT_EQ(sc.get_block_name(x, y, z, 0), expected) << "count=" << count << " pos=(" << x << "," << y << "," << z << ")";
                }
            }
        }
        EXPECT_EQ(sc.to_raw().size(), payload.size()) << "count=" << count;
    }
}

// set_block has no uniform switch: it only appends palette entries. The uniform form
// falls out of compact() ending up with a single entry, exactly like bits falls out of
// the palette size at write time.
TEST(SubChunkLayerEdit, CompactDerivesUniform) {
    bl::sub_chunk::layer layer;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            for (int z = 0; z < 16; z++) {
                layer.set_block(x, y, z, make_block_tag("minecraft:stone"));
            }
        }
    }
    // The first write seeds the air background, so this is 1 seed + 4096 separate appends.
    EXPECT_EQ(layer.palette.size(), 4097u);

    layer.compact();
    // Every position now holds stone, so the seeded air entry is unreferenced and pruned too.
    ASSERT_EQ(layer.palette.size(), 1u) << "identical blocks must collapse to one entry";
    EXPECT_EQ(layer.palette.front().name, "minecraft:stone");
    for (auto index : layer.blocks) EXPECT_EQ(index, 0);
}

// A brand new layer starts as air, so positions that are never written read back as air
// rather than as whichever block happened to be written first. Both entry points agree:
// layer::set_block seeds the layer, and ensure_layer seeds it when creating padding layers.
TEST(SubChunkLayerEdit, UntouchedPositionsAreAir) {
    bl::sub_chunk::layer layer;
    layer.set_block(0, 0, 0, make_block_tag("minecraft:stone"));

    ASSERT_FALSE(layer.palette.empty());
    EXPECT_EQ(layer.palette.front().name, "minecraft:air");
    EXPECT_EQ(layer.palette[layer.blocks[block_index(0, 0, 0)]].name, "minecraft:stone");
    EXPECT_EQ(layer.palette[layer.blocks[block_index(5, 5, 5)]].name, "minecraft:air");

    // seed_layer_with_air must not disturb a layer that already has content.
    layer.fill_blocks(make_block_tag("minecraft:dirt"));
    layer.set_block(1, 1, 1, make_block_tag("minecraft:stone"));
    ASSERT_EQ(layer.palette.size(), 2u) << "fill_blocks replaces the palette, so no extra air seed";
    EXPECT_EQ(layer.palette.front().name, "minecraft:dirt");
}

// compact() remaps indices and preserves distinct blocks.
TEST(SubChunkLayerEdit, CompactRemapsIndices) {
    bl::sub_chunk::layer layer;
    layer.set_block(0, 0, 0, make_block_tag("minecraft:stone"));
    layer.set_block(1, 0, 0, make_block_tag("minecraft:dirt"));
    layer.set_block(2, 0, 0, make_block_tag("minecraft:stone"));  // duplicate of the first
    layer.set_block(3, 0, 0, make_block_tag("minecraft:dirt"));   // duplicate of the second
    ASSERT_EQ(layer.palette.size(), 5u) << "the seeded air entry plus four appends";

    layer.compact();
    // Air survives here: every position other than the four written ones still points at it.
    ASSERT_EQ(layer.palette.size(), 3u);
    EXPECT_EQ(layer.palette.front().name, "minecraft:air");

    EXPECT_EQ(layer.palette[layer.blocks[block_index(0, 0, 0)]].name, "minecraft:stone");
    EXPECT_EQ(layer.palette[layer.blocks[block_index(1, 0, 0)]].name, "minecraft:dirt");
    EXPECT_EQ(layer.palette[layer.blocks[block_index(2, 0, 0)]].name, "minecraft:stone");
    EXPECT_EQ(layer.palette[layer.blocks[block_index(3, 0, 0)]].name, "minecraft:dirt");
    // duplicates must share one slot
    EXPECT_EQ(layer.blocks[block_index(0, 0, 0)], layer.blocks[block_index(2, 0, 0)]);
    EXPECT_EQ(layer.blocks[block_index(1, 0, 0)], layer.blocks[block_index(3, 0, 0)]);
}

// sub_chunk::set_block must reach the right block through a round trip, and must create
// the target layer when it does not exist yet.
TEST(SubChunkBlockEdit, SetBlockRoundTrips) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    sc.set_y_index(2);
    sc.set_block(3, 4, 5, make_block_tag("minecraft:stone"));
    sc.set_block(0, 0, 0, make_block_tag("minecraft:dirt"));

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));

    EXPECT_EQ(reloaded.get_block_name(3, 4, 5, 0), "minecraft:stone");
    EXPECT_EQ(reloaded.get_block_name(0, 0, 0, 0), "minecraft:dirt");
    EXPECT_EQ(reloaded.get_block_name(1, 1, 1, 0), "minecraft:air") << "untouched positions stay air";
}

// Writing layer 1 implies layer 0 exists, and both must serialize. Layer 0 is padded as
// uniform air because a layer with an empty palette has no valid on-disk form.
TEST(SubChunkBlockEdit, SetBlockCreatesPaddingLayers) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    sc.set_block(1, 1, 1, make_block_tag("minecraft:water"), 1);

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));

    EXPECT_EQ(reloaded.get_block_name(1, 1, 1, 0), "minecraft:air") << "padding layer must be air";
    EXPECT_EQ(reloaded.get_block_name(1, 1, 1, 1), "minecraft:water");
}

// fill_blocks replaces every layer and discards the old palettes, so a sub-chunk that had
// terrain plus a water layer comes back uniformly air on both layers.
TEST(SubChunkBlockEdit, FillLayerClearsEveryLayer) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    sc.set_block(2, 2, 2, make_block_tag("minecraft:stone"));
    sc.set_block(2, 2, 2, make_block_tag("minecraft:water"), 1);

    sc.fill_layer(make_block_tag("minecraft:air"));

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));

    for (int layer = 0; layer < 2; layer++) {
        for (int x = 0; x < 16; x += 7) {
            for (int y = 0; y < 16; y += 7) {
                for (int z = 0; z < 16; z += 7) {
                    EXPECT_EQ(reloaded.get_block_name(x, y, z, layer), "minecraft:air")
                        << "layer=" << layer << " pos=(" << x << "," << y << "," << z << ")";
                }
            }
        }
    }
}

// layer_index < 0 means "every layer that exists", so an empty sub_chunk stays empty.
// Callers that want a layer created pass an explicit index.
TEST(SubChunkBlockEdit, FillLayerAllIsNoOpOnEmptySubChunk) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);

    // Filling "all layers" of a sub_chunk that has none must not invent one.
    sc.fill_layer(make_block_tag("minecraft:stone"));
    auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));
    EXPECT_EQ(reloaded.get_block_name(8, 8, 8, 0), "minecraft:unknown") << "no layer was created";

    // An explicit index does create the layer.
    sc.fill_layer(make_block_tag("minecraft:stone"), 0);
    raw = sc.to_raw();
    bl::sub_chunk second;
    ASSERT_TRUE(second.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));
    EXPECT_EQ(second.get_block_name(8, 8, 8, 0), "minecraft:stone");
}

// A positive layer index fills only that layer, leaving the others alone.
TEST(SubChunkBlockEdit, FillLayerSingleLayer) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    sc.set_block(0, 0, 0, make_block_tag("minecraft:stone"));
    sc.set_block(0, 0, 0, make_block_tag("minecraft:water"), 1);

    sc.fill_layer(make_block_tag("minecraft:air"), 1);

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));
    EXPECT_EQ(reloaded.get_block_name(0, 0, 0, 0), "minecraft:stone") << "layer 0 must be untouched";
    EXPECT_EQ(reloaded.get_block_name(0, 0, 0, 1), "minecraft:air");
}

// The box overload only touches [min, max) and leaves the rest of the layer alone.
TEST(SubChunkBlockEdit, FillBlocksBoxIsExclusiveAtMax) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    sc.set_block(0, 0, 0, make_block_tag("minecraft:stone"));  // outside the box

    // [1,4) x [2,5) x [3,6)
    const bl::block_box box{{1, 2, 3}, {4, 5, 6}};
    sc.fill_blocks(box, make_block_tag("minecraft:gold_block"));

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));

    EXPECT_EQ(reloaded.get_block_name(0, 0, 0, 0), "minecraft:stone") << "outside the box";
    EXPECT_EQ(reloaded.get_block_name(1, 2, 3, 0), "minecraft:gold_block") << "min corner is inside";
    EXPECT_EQ(reloaded.get_block_name(3, 4, 5, 0), "minecraft:gold_block") << "just below max is inside";
    EXPECT_EQ(reloaded.get_block_name(4, 2, 3, 0), "minecraft:air") << "max x is excluded";
    EXPECT_EQ(reloaded.get_block_name(1, 5, 3, 0), "minecraft:air") << "max y is excluded";
    EXPECT_EQ(reloaded.get_block_name(1, 2, 6, 0), "minecraft:air") << "max z is excluded";
}

// A box sticking out of the sub-chunk is clipped rather than rejected.
TEST(SubChunkBlockEdit, FillBlocksBoxIsClipped) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    const bl::block_box box{{-8, 12, -8}, {40, 40, 40}};
    // Explicit layer: the default (-1) means "existing layers only" and this sub_chunk is empty.
    sc.fill_blocks(box, make_block_tag("minecraft:stone"), 0);

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));
    EXPECT_EQ(reloaded.get_block_name(0, 12, 0, 0), "minecraft:stone") << "clipped corner is filled";
    EXPECT_EQ(reloaded.get_block_name(15, 15, 15, 0), "minecraft:stone");
    EXPECT_EQ(reloaded.get_block_name(0, 11, 0, 0), "minecraft:air") << "below the clipped box";
}

// layer_index < 0 fills the box on every existing layer.
TEST(SubChunkBlockEdit, FillBlocksBoxAllLayers) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    sc.set_block(0, 0, 0, make_block_tag("minecraft:stone"));
    sc.set_block(0, 0, 0, make_block_tag("minecraft:water"), 1);

    const bl::block_box box{{2, 2, 2}, {4, 4, 4}};
    sc.fill_blocks(box, make_block_tag("minecraft:gold_block"));

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));
    for (int layer = 0; layer < 2; layer++) {
        EXPECT_EQ(reloaded.get_block_name(2, 2, 2, layer), "minecraft:gold_block") << "layer=" << layer;
        EXPECT_EQ(reloaded.get_block_name(7, 7, 7, layer), "minecraft:air") << "layer=" << layer;
    }
}

// compact() is the write-prep rebuild: entries nothing points at are dropped, which is what
// lets an edited layer collapse back to uniform (bits == 0).
TEST(SubChunkLayerEdit, CompactPrunesUnreferencedEntries) {
    bl::sub_chunk::layer layer;
    // Start from a layer that is entirely stone so the seeded air entry becomes unreferenced.
    layer.fill_blocks(make_block_tag("minecraft:stone"));
    ASSERT_EQ(layer.palette.size(), 1u);

    // Write two blocks we then overwrite, leaving dirt and gravel referenced by nothing.
    layer.set_block(0, 0, 0, make_block_tag("minecraft:dirt"));
    layer.set_block(1, 0, 0, make_block_tag("minecraft:gravel"));
    layer.set_block(0, 0, 0, make_block_tag("minecraft:stone"));
    layer.set_block(1, 0, 0, make_block_tag("minecraft:stone"));
    ASSERT_EQ(layer.palette.size(), 5u);

    layer.compact();

    ASSERT_EQ(layer.palette.size(), 1u) << "dirt and gravel are referenced by nothing";
    EXPECT_EQ(layer.palette.front().name, "minecraft:stone");

    std::string out;
    bl::write_layer(out, layer.blocks, layer.palette);
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(static_cast<uint8_t>(out[0]), 0) << "pruned single-entry palette must write bits == 0";
}

// sub_chunk::compact() must reach every layer, so a sub-chunk built by repeated set_block
// writes as a single uniform pair of layers rather than a huge palette.
TEST(SubChunkBlockEdit, CompactForwardsToEveryLayer) {
    bl::sub_chunk sc;
    sc.set_version(bl::SubChunkVersion::V9);
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            for (int z = 0; z < 16; z++) {
                sc.set_block(x, y, z, make_block_tag("minecraft:stone"));
                sc.set_block(x, y, z, make_block_tag("minecraft:water"), 1);
            }
        }
    }

    sc.compact();

    const auto raw = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));
    for (int layer = 0; layer < 2; layer++) {
        const char* expected = layer == 0 ? "minecraft:stone" : "minecraft:water";
        EXPECT_EQ(reloaded.get_block_name(0, 0, 0, layer), expected) << "layer=" << layer;
        EXPECT_EQ(reloaded.get_block_name(15, 15, 15, layer), expected) << "layer=" << layer;
    }
    // Every layer collapsed to one entry, so the whole payload is tiny.
    EXPECT_LT(raw.size(), 200u) << "compact() did not reach every layer";
}

// compact() must work on a sub-chunk that was loaded from disk, not just a built one.
TEST(SubChunkBlockEdit, CompactLoadedSubChunkIsStable) {
    auto names = std::vector<std::string>{"minecraft:stone", "minecraft:dirt"};
    std::vector<uint16_t> blocks(4096);
    for (int i = 0; i < 4096; i++) blocks[i] = static_cast<uint16_t>(i % 2);
    const auto payload = build_payload(names, blocks, 0);

    bl::sub_chunk sc;
    ASSERT_TRUE(sc.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()));
    sc.compact();

    const auto again = sc.to_raw();
    bl::sub_chunk reloaded;
    ASSERT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(again.data()), again.size()));
    // Walk x/y/z and derive the flat index the same way palette_entry_at does, rather than
    // inverting it by hand -- idx = y + z * 16 + x * 256.
    for (int x = 0; x < 16; x += 3) {
        for (int y = 0; y < 16; y += 5) {
            for (int z = 0; z < 16; z += 7) {
                const int idx = y + z * 16 + x * 256;
                EXPECT_EQ(reloaded.get_block_name(x, y, z, 0), names[blocks[idx]]) << "pos=(" << x << "," << y << "," << z << ")";
            }
        }
    }
    EXPECT_EQ(sc.to_raw(), again) << "a second compact/to_raw must be byte-stable";
}

// A layer filled with air and compacted is what deleteBlocks emits: one entry, bits == 0
// on the wire.
TEST(SubChunkLayerEdit, AirLayerSerializesAsUniform) {
    bl::sub_chunk::layer layer;
    layer.fill_blocks(make_block_tag("minecraft:air"));
    layer.compact();
    ASSERT_EQ(layer.palette.size(), 1u);
    EXPECT_EQ(layer.palette.front().name, "minecraft:air");

    std::string out;
    bl::write_layer(out, layer.blocks, layer.palette);
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(static_cast<uint8_t>(out[0]), 0) << "single-entry palette must serialize with bits == 0";
}

// A 1-entry palette is stored as a uniform layer: header only, no index array and
// no palette_len word. Serializing it back has to take the same branch.
TEST(SubChunkRoundTrip, UniformLayerRoundTrip) {
    const std::string name = "minecraft:stone";
    std::vector<uint16_t> blocks(4096, 0);
    const auto payload = build_payload({name}, blocks, 0);

    bl::sub_chunk sc;
    sc.set_y_index(0);
    ASSERT_TRUE(sc.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()));

    // header + version + layers_num + y_index + one NBT compound
    EXPECT_EQ(payload[3], 0) << "uniform layer header should be 0";

    for (int x = 0; x < 16; x += 5) {
        for (int y = 0; y < 16; y += 5) {
            for (int z = 0; z < 16; z += 5) {
                EXPECT_EQ(sc.get_block_name(x, y, z, 0), name);
            }
        }
    }
}

// "Uniform" is derived from the palette size, not from the block contents: a layer whose
// blocks all use index 0 still keeps bit=1 when its palette has 2 entries. Real saves do
// contain such layers, so the writer must NOT collapse them.
TEST(SubChunkSerialization, DoesNotCollapseSemanticallyUniformLayer) {
    // 2-entry palette but every block references entry 0
    std::vector<uint16_t> blocks(4096, 0);
    const auto payload = build_payload({"minecraft:stone", "minecraft:dirt"}, blocks, 0);

    // header byte sits after version, layers_num and y_index
    EXPECT_EQ(static_cast<uint8_t>(payload[3]), 2) << "2-entry palette must stay bit-packed even when all blocks are identical";

    bl::sub_chunk sc;
    ASSERT_TRUE(sc.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()));
    EXPECT_EQ(sc.get_block_name(0, 0, 0, 0), "minecraft:stone");

    // and a round trip must stay faithful rather than normalizing to uniform
    const auto reserialized = sc.to_raw();
    EXPECT_EQ(static_cast<uint8_t>(reserialized[3]), static_cast<uint8_t>(payload[3]));
    EXPECT_EQ(reserialized.size(), payload.size());
}

// A built (never loaded) sub_chunk has no version byte; to_raw has to fall back to v9
// rather than emit the unset marker, which no reader accepts.
TEST(SubChunkVersionTest, UnsetVersionFallsBackToV9) {
    bl::sub_chunk sc;
    EXPECT_EQ(sc.version(), bl::UNSET_SUB_CHUNK_VERSION);

    const auto raw = sc.to_raw();
    ASSERT_GE(raw.size(), 2u);
    EXPECT_EQ(static_cast<uint8_t>(raw[0]), static_cast<uint8_t>(bl::SubChunkVersion::V9));

    // The fallback must also emit the v9-only Y index byte, otherwise layers would
    // shift by one byte and land at the wrong height.
    bl::sub_chunk reloaded;
    EXPECT_TRUE(reloaded.load(reinterpret_cast<const byte_t*>(raw.data()), raw.size()));
    EXPECT_EQ(reloaded.version(), static_cast<uint8_t>(bl::SubChunkVersion::V9));
}

// Version 8 has no Y index in the header; a round trip must not invent one.
TEST(SubChunkVersionTest, V8KeepsHeaderLayout) {
    std::vector<uint16_t> blocks(4096, 0);
    const auto payload = build_payload({"minecraft:stone"}, blocks, 7, static_cast<uint8_t>(bl::SubChunkVersion::V8));

    bl::sub_chunk sc;
    ASSERT_TRUE(sc.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()));
    EXPECT_EQ(sc.version(), static_cast<uint8_t>(bl::SubChunkVersion::V8));

    const auto reserialized = sc.to_raw();
    EXPECT_EQ(static_cast<uint8_t>(reserialized[0]), static_cast<uint8_t>(bl::SubChunkVersion::V8));
    EXPECT_EQ(reserialized.size(), payload.size());
    EXPECT_EQ(reserialized[2], payload[2]) << "layer body must start right after layers_num for v8";
}

// read_header rejects anything outside {8, 9}.
TEST(SubChunkVersionTest, UnsupportedVersionIsRejected) {
    std::vector<uint16_t> blocks(4096, 0);
    for (uint8_t bogus : {0u, 7u, 10u, 255u}) {
        auto payload = build_payload({"minecraft:stone"}, blocks, 0);
        payload[0] = static_cast<char>(bogus);

        bl::sub_chunk sc;
        EXPECT_FALSE(sc.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()))
            << "version " << static_cast<int>(bogus) << " should be rejected";
    }
}

// Every sub-chunk in the dumped saves must survive load -> to_raw -> load with the
// same geometry and block contents.
TEST(SubChunkRoundTrip, DumpedChunksSurviveReserialization) {
    const auto files = list_chunk_files();
    ASSERT_FALSE(files.empty());

    size_t sub_chunks = 0;
    size_t byte_identical = 0;
    size_t original_bytes = 0;
    size_t reserialized_bytes = 0;

    for (const auto& file : files) {
        const auto raw_file = bl::utils::read_file(file);
        if (raw_file.empty()) continue;

        bl::raw_chunk rc;
        if (!rc.from_raw(raw_file)) continue;

        for (const auto& [index, payload] : rc.get_sub_chunks()) {
            if (payload.empty()) continue;

            bl::sub_chunk original;
            original.set_y_index(index);
            ASSERT_TRUE(original.load(reinterpret_cast<const byte_t*>(payload.data()), payload.size()))
                << file << " sub=" << static_cast<int>(index);

            const auto reserialized = original.to_raw();

            bl::sub_chunk restored;
            restored.set_y_index(index);
            ASSERT_TRUE(restored.load(reinterpret_cast<const byte_t*>(reserialized.data()), reserialized.size()))
                << file << " sub=" << static_cast<int>(index);

            EXPECT_EQ(restored.version(), original.version()) << file;
            EXPECT_EQ(restored.y_index(), original.y_index()) << file;

            for (int x = 0; x < 16; x++) {
                for (int y = 0; y < 16; y++) {
                    for (int z = 0; z < 16; z++) {
                        for (int layer = 0; layer < 2; layer++) {
                            ASSERT_EQ(restored.get_block_name(x, y, z, layer), original.get_block_name(x, y, z, layer))
                                << file << " sub=" << static_cast<int>(index) << " pos=(" << x << "," << y << "," << z
                                << ") layer=" << layer;
                        }
                    }
                }
            }

            sub_chunks++;
            original_bytes += payload.size();
            reserialized_bytes += reserialized.size();
            if (reserialized == payload) byte_identical++;

            // Second pass has no version tags left to strip, so it must be byte-stable.
            ASSERT_EQ(restored.to_raw(), reserialized) << file << " sub=" << static_cast<int>(index) << " is not idempotent";
        }
    }

    ASSERT_GT(sub_chunks, 0u);
    ASSERT_GE(original_bytes, reserialized_bytes);

    // make_palette_entry() drops the "version" tag from every palette entry, so a
    // byte-identical rewrite is not expected. Every entry in these saves has one, and a
    // stripped "version" TAG_Int costs 1 (type) + 2 + 7 (name) + 4 (payload) = 14 bytes,
    // so the whole size delta has to be an exact multiple of that. Anything else would
    // mean the bit packing itself changed.
    const size_t stripped = original_bytes - reserialized_bytes;
    EXPECT_EQ(stripped % 14, 0u) << "size delta " << stripped << " is not only stripped version tags";

    std::cout << "sub-chunks: " << sub_chunks << ", byte-identical after round trip: " << byte_identical
              << ", bytes dropped with the version tags: " << stripped << " (" << stripped / 14 << " entries)\n";
}
