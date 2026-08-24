//
// Created by xhy on 2023/3/31.
// Rewritten: comprehensive round-trip + invalid-input coverage for all key types.
//

#include <gtest/gtest.h>

#include <memory>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "chunk.h"
#include "chunk_data_position.h"

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

TEST(BlockBox, HalfOpenRegion) {
    using namespace bl;
    const block_box box{{1, 2, 3}, {4, 6, 8}};

    EXPECT_TRUE(box.is_valid());
    EXPECT_EQ(box.size_x(), 3);
    EXPECT_EQ(box.size_y(), 4);
    EXPECT_EQ(box.size_z(), 5);
    EXPECT_TRUE(box.contains({1, 2, 3}));
    EXPECT_TRUE(box.contains({3, 5, 7}));
    EXPECT_FALSE(box.contains({4, 5, 7}));
    EXPECT_FALSE(box.contains({3, 6, 7}));
    EXPECT_FALSE(box.contains({3, 5, 8}));
}

TEST(BlockBox, NormalizeIntersectAndTranslate) {
    using namespace bl;
    const auto normalized = block_box{{5, 6, 7}, {1, 2, 3}}.normalized();
    EXPECT_EQ(normalized.min_pos, (block_pos{1, 2, 3}));
    EXPECT_EQ(normalized.max_pos, (block_pos{5, 6, 7}));

    const auto intersection = normalized.intersected({{3, 0, 4}, {8, 4, 9}});
    EXPECT_EQ(intersection.min_pos, (block_pos{3, 2, 4}));
    EXPECT_EQ(intersection.max_pos, (block_pos{5, 4, 7}));

    const auto moved = intersection.translated(-1, 10, 2);
    EXPECT_EQ(moved.min_pos, (block_pos{2, 12, 6}));
    EXPECT_EQ(moved.max_pos, (block_pos{4, 14, 9}));
}

// hardcoded_spawn_area_list serialize -> parse must reproduce every field
TEST(HardcodedSpawnAreaList, RoundTrip) {
    using namespace bl;
    hardcoded_spawn_area_list list;
    list.add({HSAType::NetherFortress, block_pos{0, 4, 0}, block_pos{15, 127, 15}});
    list.add({HSAType::OceanMonument, block_pos{-5, 40, -3}, block_pos{20, 80, 12}});
    list.add({HSAType::SwampHut, block_pos{1, 63, 2}, block_pos{4, 66, 5}});
    list.add({HSAType::PillagerOutpost, block_pos{-10, 70, -8}, block_pos{25, 90, 30}});

    auto raw = list.to_raw();
    EXPECT_EQ(raw.size(), 4 + 4u * 25u);

    hardcoded_spawn_area_list parsed;
    EXPECT_TRUE(parsed.from_raw(raw));
    EXPECT_EQ(parsed.size(), 4u);
    for (size_t i = 0; i < parsed.size(); i++) {
        const auto &a = list.areas()[i];
        const auto &b = parsed.areas()[i];
        EXPECT_EQ(static_cast<int>(b.type), static_cast<int>(a.type));
        EXPECT_EQ(b.min_pos.x, a.min_pos.x);
        EXPECT_EQ(b.min_pos.y, a.min_pos.y);
        EXPECT_EQ(b.min_pos.z, a.min_pos.z);
        EXPECT_EQ(b.max_pos.x, a.max_pos.x);
        EXPECT_EQ(b.max_pos.y, a.max_pos.y);
        EXPECT_EQ(b.max_pos.z, a.max_pos.z);
    }
    EXPECT_EQ(parsed.to_raw(), raw);
}

TEST(HardcodedSpawnAreaList, ParseInvalid) {
    using namespace bl;
    hardcoded_spawn_area_list list;
    EXPECT_FALSE(list.from_raw(""));                         // too short (no count field)
    EXPECT_FALSE(list.from_raw(std::string(5, '\0')));       // count field present but payload truncated
    EXPECT_FALSE(list.from_raw(std::string(4 + 24, '\0')));  // wrong area size for count=0
    // count=0 is a valid empty list payload
    EXPECT_TRUE(list.from_raw(std::string(4, '\0')));
    EXPECT_TRUE(list.empty());
}

TEST(HardcodedSpawnAreaList, EditOps) {
    using namespace bl;
    hardcoded_spawn_area_list list;
    list.add({HSAType::NetherFortress, block_pos{0, 0, 0}, block_pos{1, 1, 1}});
    list.add({HSAType::SwampHut, block_pos{2, 2, 2}, block_pos{3, 3, 3}});
    EXPECT_EQ(list.size(), 2u);
    EXPECT_TRUE(list.remove(0));
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(list.areas()[0].type, HSAType::SwampHut);
    EXPECT_FALSE(list.remove(5));  // out of range
    list.clear();
    EXPECT_TRUE(list.empty());
}

TEST(BlockEntity, OffsetPos) {
    using namespace bl;

    auto block_entity_owner = std::make_unique<nbt::compound_tag>("block_entity");
    auto *block_entity = block_entity_owner.get();
    block_entity->put(new nbt::string_tag("id", "Chest"));
    block_entity->put(new nbt::int_tag("x", 12));
    block_entity->put(new nbt::int_tag("y", 64));
    block_entity->put(new nbt::int_tag("z", -9));
    block_entity->put(new nbt::int_tag("pairx", 20));
    block_entity->put(new nbt::int_tag("pairz", 3));

    offset_block_entity_pos(block_entity, 32, -16);

    EXPECT_EQ(block_entity->get("x")->as<nbt::int_tag *>()->value, 44);
    EXPECT_EQ(block_entity->get("y")->as<nbt::int_tag *>()->value, 64);
    EXPECT_EQ(block_entity->get("z")->as<nbt::int_tag *>()->value, -25);
    EXPECT_EQ(block_entity->get("pairx")->as<nbt::int_tag *>()->value, 52);
    EXPECT_EQ(block_entity->get("pairz")->as<nbt::int_tag *>()->value, -13);
}

TEST(BlockEntity, DoesNotOffsetPairPosForOtherTypes) {
    using namespace bl;

    auto block_entity_owner = std::make_unique<nbt::compound_tag>("block_entity");
    auto *block_entity = block_entity_owner.get();
    block_entity->put(new nbt::string_tag("id", "Barrel"));
    block_entity->put(new nbt::int_tag("x", 12));
    block_entity->put(new nbt::int_tag("z", -9));
    block_entity->put(new nbt::int_tag("pairx", 20));
    block_entity->put(new nbt::int_tag("pairz", 3));

    offset_block_entity_pos(block_entity, 32, -16);

    EXPECT_EQ(block_entity->get("x")->as<nbt::int_tag *>()->value, 44);
    EXPECT_EQ(block_entity->get("z")->as<nbt::int_tag *>()->value, -25);
    EXPECT_EQ(block_entity->get("pairx")->as<nbt::int_tag *>()->value, 20);
    EXPECT_EQ(block_entity->get("pairz")->as<nbt::int_tag *>()->value, 3);
}

TEST(BlockEntity, SetPosUpdatesChestPairPos) {
    using namespace bl;

    auto block_entity = std::make_unique<nbt::compound_tag>("block_entity");
    block_entity->put(new nbt::string_tag("id", "Chest"));
    block_entity->put(new nbt::int_tag("x", 12));
    block_entity->put(new nbt::int_tag("y", 64));
    block_entity->put(new nbt::int_tag("z", -9));
    block_entity->put(new nbt::int_tag("pairx", 20));
    block_entity->put(new nbt::int_tag("pairz", 3));

    set_block_entity_pos(block_entity.get(), {44, 80, -25});

    EXPECT_EQ(block_entity->get("x")->as<nbt::int_tag *>()->value, 44);
    EXPECT_EQ(block_entity->get("y")->as<nbt::int_tag *>()->value, 80);
    EXPECT_EQ(block_entity->get("z")->as<nbt::int_tag *>()->value, -25);
    EXPECT_EQ(block_entity->get("pairx")->as<nbt::int_tag *>()->value, 52);
    EXPECT_EQ(block_entity->get("pairz")->as<nbt::int_tag *>()->value, -13);
}

TEST(PendingTicks, OffsetPos) {
    using namespace bl;

    auto pending_ticks_owner = std::make_unique<nbt::compound_tag>("pending_ticks");
    auto *pending_ticks = pending_ticks_owner.get();
    auto *tick_list = new nbt::list_tag("tickList");
    auto *tick = new nbt::compound_tag("");
    tick->put(new nbt::int_tag("x", 7));
    tick->put(new nbt::int_tag("y", 80));
    tick->put(new nbt::int_tag("z", -4));
    tick_list->append(tick);
    pending_ticks->put(tick_list);

    offset_pending_ticks_pos(pending_ticks, 32, -16);

    EXPECT_EQ(tick->get("x")->as<nbt::int_tag *>()->value, 39);
    EXPECT_EQ(tick->get("y")->as<nbt::int_tag *>()->value, 80);
    EXPECT_EQ(tick->get("z")->as<nbt::int_tag *>()->value, -20);
}

// raw_chunk::set_pos must offset HardCodedSpawnAreas coordinates with the chunk
// displacement, like it does for block entities / pending ticks / entities.
TEST(HardcodedSpawnAreaList, SetPosOffsets) {
    using namespace bl;
    bl::bedrock_level level;

    bl::raw_chunk rc(chunk_pos{0, 0, 0});
    hardcoded_spawn_area_list list;
    list.add({HSAType::NetherFortress, block_pos{0, 4, 0}, block_pos{15, 127, 15}});
    list.add({HSAType::OceanMonument, block_pos{-5, 40, -3}, block_pos{20, 80, 12}});
    rc.set_normal(chunk_key::HardCodedSpawnAreas, list.to_raw());

    // move chunk by (+2, +3) chunks -> +32/+48 blocks
    rc.set_pos(chunk_pos{2, 3, 0}, &level);

    hardcoded_spawn_area_list parsed;
    ASSERT_TRUE(parsed.from_raw(rc.get_normal_key(chunk_key::HardCodedSpawnAreas)));
    ASSERT_EQ(parsed.size(), 2u);
    EXPECT_EQ(parsed.areas()[0].min_pos.x, 32);
    EXPECT_EQ(parsed.areas()[0].min_pos.z, 48);
    EXPECT_EQ(parsed.areas()[0].max_pos.x, 47);
    EXPECT_EQ(parsed.areas()[0].max_pos.z, 63);
    EXPECT_EQ(parsed.areas()[0].min_pos.y, 4);  // y is untouched
    EXPECT_EQ(parsed.areas()[0].max_pos.y, 127);
    EXPECT_EQ(parsed.areas()[1].min_pos.x, 27);
    EXPECT_EQ(parsed.areas()[1].min_pos.z, 45);
    EXPECT_EQ(parsed.areas()[1].max_pos.x, 52);
    EXPECT_EQ(parsed.areas()[1].max_pos.z, 60);
}
