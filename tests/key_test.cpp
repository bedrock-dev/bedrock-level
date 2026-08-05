//
// Created by xhy on 2023/3/31.
// Rewritten: comprehensive round-trip + invalid-input coverage for all key types.
//

#include <gtest/gtest.h>

#include "bedrock_key.h"

TEST(ChunkPos, ValidCheck) {
    using namespace bl;
    EXPECT_FALSE((chunk_pos{1, 1, -1}.valid()));
    EXPECT_TRUE((chunk_pos{1, 1, 0}.valid()));
    EXPECT_TRUE((chunk_pos{1, 1, 1}.valid()));
    EXPECT_TRUE((chunk_pos{1, 1, 2}.valid()));
    EXPECT_TRUE((chunk_pos{1, 1, 3}.valid()));  // custom dims are legal
}

TEST(ChunkPos, Equality) {
    using namespace bl;
    EXPECT_TRUE((chunk_pos{1, 2, 0} == chunk_pos{1, 2, 0}));
    EXPECT_FALSE((chunk_pos{1, 2, 0} == chunk_pos{2, 2, 0}));
    EXPECT_FALSE((chunk_pos{1, 2, 0} == chunk_pos{1, 2, 1}));
    EXPECT_FALSE((chunk_pos{1, 2, 0} == chunk_pos{1, 3, 0}));
}

TEST(ChunkPos, YRange) {
    using namespace bl;
    EXPECT_EQ((chunk_pos{0, 0, 0}.get_y_range(New)), (std::tuple<int32_t, int32_t>{-64, 319}));
    EXPECT_EQ((chunk_pos{0, 0, 0}.get_y_range(Old)), (std::tuple<int32_t, int32_t>{0, 255}));
    EXPECT_EQ((chunk_pos{0, 0, 1}.get_y_range(New)), (std::tuple<int32_t, int32_t>{0, 127}));
    EXPECT_EQ((chunk_pos{0, 0, 2}.get_y_range(New)), (std::tuple<int32_t, int32_t>{0, 255}));
    // custom dimensions fall back to the 1.18+ world range (valid min <= max)
    EXPECT_EQ((chunk_pos{0, 0, 3}.get_y_range(New)), (std::tuple<int32_t, int32_t>{-64, 319}));
    EXPECT_EQ((chunk_pos{0, 0, -1}.get_y_range(New)), (std::tuple<int32_t, int32_t>{-64, 319}));
}

// every parse(to_raw(k)) must reproduce k for all dim / type / y_index combinations
TEST(ChunkKey, RoundTripAll) {
    using namespace bl;
    for (int dim : {0, 1, 2}) {
        for (auto type : {chunk_key::RandomTicks, chunk_key::Data3D, chunk_key::BlockEntity, chunk_key::VersionNew}) {
            chunk_key key{type, chunk_pos{10, -7, dim}, 0};
            auto parsed = chunk_key::parse(key.to_raw());
            EXPECT_TRUE(parsed.cp == key.cp) << "dim=" << dim << " type=" << (int)type;
            EXPECT_EQ(parsed.type, key.type);
            EXPECT_EQ(parsed.y_index, key.y_index);
        }
        // sub chunk terrain carries a y index
        chunk_key sub{chunk_key::SubChunkTerrain, chunk_pos{10, -7, dim}, 13};
        auto parsed_sub = chunk_key::parse(sub.to_raw());
        EXPECT_TRUE(parsed_sub.cp == sub.cp);
        EXPECT_EQ(parsed_sub.type, sub.type);
        EXPECT_EQ(parsed_sub.y_index, 13);
    }
}

TEST(ChunkKey, ParseInvalid) {
    using namespace bl;
    EXPECT_FALSE(chunk_key::parse("").valid());
    EXPECT_FALSE(chunk_key::parse("short").valid());
    EXPECT_FALSE(chunk_key::parse(std::string(9, '\0')).valid());   // type 0 is out of range
    EXPECT_FALSE(chunk_key::parse(std::string(12, '\0')).valid());  // wrong length
    // 10-byte key that is not sub-chunk terrain must be rejected
    chunk_key bad{chunk_key::Data3D, chunk_pos{1, 1, 0}, 0};
    auto raw = bad.to_raw();
    raw.push_back('\0');  // make it 10 bytes with a trailing y index
    EXPECT_FALSE(chunk_key::parse(raw).valid());
}

TEST(ActorKey, RoundTrip) {
    using namespace bl;
    actor_key k;
    k.actor_uid = 1234567890123LL;
    EXPECT_TRUE(k.valid());
    // raw layout: "actorprefix" (11) + 8-byte uid
    std::string raw = "actorprefix";
    raw.append(reinterpret_cast<const char *>(&k.actor_uid), 8);
    auto parsed = actor_key::parse(raw);
    EXPECT_TRUE(parsed.valid());
    EXPECT_EQ(parsed.actor_uid, k.actor_uid);
}

TEST(ActorKey, ParseInvalid) {
    using namespace bl;
    EXPECT_FALSE(actor_key::parse("").valid());
    EXPECT_FALSE(actor_key::parse("actorprefix").valid());                         // too short (11 bytes)
    EXPECT_FALSE(actor_key::parse("wrongprefix" + std::string(8, '\0')).valid());  // wrong prefix
    EXPECT_FALSE(actor_key::parse(std::string(19, 'x')).valid());                  // wrong prefix, right length
}

TEST(ActorDigestKey, RoundTrip) {
    using namespace bl;
    for (int dim : {0, 1, 2}) {
        actor_digest_key k;
        k.cp = chunk_pos{5, -3, dim};
        auto parsed = actor_digest_key::parse(k.to_raw());
        EXPECT_TRUE(parsed.valid());
        EXPECT_TRUE(parsed.cp == k.cp) << "dim=" << dim;
        EXPECT_EQ(parsed.to_raw(), k.to_raw());
    }
}

TEST(ActorDigestKey, ParseInvalid) {
    using namespace bl;
    EXPECT_FALSE(actor_digest_key::parse("").valid());
    EXPECT_FALSE(actor_digest_key::parse("nope").valid());
    EXPECT_FALSE(actor_digest_key::parse("digpxxxx").valid());
    EXPECT_FALSE(actor_digest_key::parse(std::string(16, 'A')).valid());  // wrong prefix
}

TEST(VillageKey, ThreeSegment) {
    const std::string raw_id = "VILLAGE_241c7732-221a-4266-9fe9-cdd40d9bdeb0_INFO";
    bl::village_key key = bl::village_key::parse(raw_id);
    EXPECT_TRUE(key.valid());
    EXPECT_TRUE(key.uuid == "241c7732-221a-4266-9fe9-cdd40d9bdeb0");
    EXPECT_TRUE(key.type == bl::village_key::INFO);
    EXPECT_EQ(key.dim, 0);
    EXPECT_EQ(key.to_raw(), raw_id);
}

// dimensioned village keys must survive parse -> to_raw -> parse
TEST(VillageKey, FourSegmentRoundTrip) {
    using namespace bl;
    // dim 1/2 keep their dimension segment on serialization
    for (const char *dim_str : {"Nether", "TheEnd"}) {
        std::string raw = std::string("VILLAGE_") + dim_str + "_241c7732-221a-4266-9fe9-cdd40d9bdeb0_POI";
        auto k = village_key::parse(raw);
        EXPECT_TRUE(k.valid()) << dim_str;
        EXPECT_EQ(k.to_raw(), raw) << dim_str;  // dimension must not be dropped
        EXPECT_EQ(village_key::parse(k.to_raw()).dim, k.dim);
    }
    // Overworld is canonicalized to dim 0 and serialized as the 3-segment form
    {
        auto k = village_key::parse("VILLAGE_Overworld_241c7732-221a-4266-9fe9-cdd40d9bdeb0_POI");
        EXPECT_TRUE(k.valid());
        EXPECT_EQ(k.dim, 0);
        EXPECT_EQ(k.to_raw(), "VILLAGE_241c7732-221a-4266-9fe9-cdd40d9bdeb0_POI");
    }
}

TEST(VillageKey, ParseInvalid) {
    using namespace bl;
    EXPECT_FALSE(village_key::parse("").valid());
    EXPECT_FALSE(village_key::parse("NOT_VILLAGE_uuid_INFO").valid());
    EXPECT_FALSE(village_key::parse("VILLAGE_short_INFO").valid());  // bad uuid length
    EXPECT_FALSE(village_key::parse("VILLAGE_241c7732-221a-4266-9fe9-cdd40d9bdeb0_BADTYPE").valid());
}

TEST(BlockPos, Conversions) {
    using namespace bl;
    // positive coords
    block_pos b1{20, 0, 35};
    EXPECT_EQ(b1.to_chunk_pos(), (chunk_pos{1, 2, -1}));
    EXPECT_EQ(b1.in_chunk_offset(), (chunk_pos{4, 3, -1}));
    // negative coords floor-divide toward -inf
    block_pos b2{-1, 0, -17};
    EXPECT_EQ(b2.to_chunk_pos(), (chunk_pos{-1, -2, -1}));
    EXPECT_EQ(b2.in_chunk_offset(), (chunk_pos{15, 15, -1}));
}
