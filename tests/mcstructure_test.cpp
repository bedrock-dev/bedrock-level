//
// Created by xhy on 2023/3/29.
//

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>

#include "palette.h"
#include "utils.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

namespace {
    std::vector<byte_t> load_mcstructure() { return bl::utils::read_file(TEST_DATA_DIR "/mcstructures/test.mcstructure"); }

    bool is_visible_block_name(const std::string &name) {
        return name != "minecraft:air" && name != "minecraft:cave_air" && name != "minecraft:void_air" && name != "minecraft:unknown";
    }

    std::unique_ptr<bl::nbt::compound_tag> make_block_state_tag(const std::string &name) {
        auto tag = std::make_unique<bl::nbt::compound_tag>("block");
        tag->put(new bl::nbt::string_tag("name", name));
        tag->put(new bl::nbt::compound_tag("states"));
        tag->put(new bl::nbt::int_tag("version", 17959425));
        return tag;
    }

    std::unique_ptr<bl::nbt::compound_tag> make_block_entity_tag(const std::string &id) {
        auto tag = std::make_unique<bl::nbt::compound_tag>("block_entity");
        tag->put(new bl::nbt::string_tag("id", id));
        return tag;
    }

    size_t flat_index(int x, int y, int z, int size_y, int size_z) {
        return static_cast<size_t>(size_z) * static_cast<size_t>(size_y) * static_cast<size_t>(x) +
               static_cast<size_t>(size_z) * static_cast<size_t>(y) + static_cast<size_t>(z);
    }

    bl::nbt::list_tag *make_int_list_tag(const std::string &key, int a, int b, int c) {
        auto *list = new bl::nbt::list_tag(key);
        list->append(new bl::nbt::int_tag("", a));
        list->append(new bl::nbt::int_tag("", b));
        list->append(new bl::nbt::int_tag("", c));
        return list;
    }

    std::vector<byte_t> make_mcstructure_with_block_entity_position_key() {
        auto root = std::make_unique<bl::nbt::compound_tag>("");
        root->put(new bl::nbt::int_tag("format_version", 1));
        root->put(make_int_list_tag("size", 3, 3, 3));

        auto *structure = new bl::nbt::compound_tag("structure");
        auto *block_indices = new bl::nbt::list_tag("block_indices");
        for (int layer = 0; layer < 2; ++layer) {
            auto *layer_list = new bl::nbt::list_tag("");
            for (int i = 0; i < 27; ++i) {
                layer_list->append(new bl::nbt::int_tag("", layer == 0 ? 0 : -1));
            }
            block_indices->append(layer_list);
        }
        structure->put(block_indices);
        structure->put(new bl::nbt::list_tag("entities"));

        auto *palette = new bl::nbt::compound_tag("palette");
        auto *def = new bl::nbt::compound_tag("default");
        auto *block_palette = new bl::nbt::list_tag("block_palette");
        block_palette->append(make_block_state_tag("minecraft:stone").release());
        def->put(block_palette);

        auto *block_position_data = new bl::nbt::compound_tag("block_position_data");
        auto *position_entry = new bl::nbt::compound_tag(std::to_string(flat_index(1, 1, 1, 3, 3)));
        auto block_entity = make_block_entity_tag("minecraft:chest");
        block_entity->set_key("block_entity_data");
        block_entity->put(new bl::nbt::int_tag("x", 99));
        block_entity->put(new bl::nbt::int_tag("y", 88));
        block_entity->put(new bl::nbt::int_tag("z", 77));
        position_entry->put(block_entity.release());
        block_position_data->put(position_entry);
        def->put(block_position_data);
        palette->put(def);
        structure->put(palette);

        root->put(structure);
        root->put(make_int_list_tag("structure_world_origin", 10, 20, 30));

        const auto raw = root->to_raw();
        return {raw.begin(), raw.end()};
    }
}  // namespace

TEST(McStructure, ParseTestFile) {
    auto raw = load_mcstructure();
    ASSERT_FALSE(raw.empty());

    auto s = bl::parse_mcstructure(raw.data(), raw.size());

    EXPECT_GT(s.size_x(), 0);
    EXPECT_GT(s.size_y(), 0);
    EXPECT_GT(s.size_z(), 0);
    EXPECT_EQ(s.size().x, s.size_x());
    EXPECT_EQ(s.size().y, s.size_y());
    EXPECT_EQ(s.size().z, s.size_z());

    const auto expected = static_cast<size_t>(s.size_x()) * static_cast<size_t>(s.size_y()) * static_cast<size_t>(s.size_z());
    EXPECT_EQ(expected, s.layer_size(0));
    EXPECT_EQ(expected, s.layer_size(1));
    EXPECT_EQ(2u, s.layer_count());
    EXPECT_FALSE(s.palette().empty());
    EXPECT_EQ(s.palette_size(), s.palette().size());
    EXPECT_EQ(s.entity_count(), s.entities().size());
    EXPECT_EQ(s.block_entity_count(), s.block_entities().size());

    // every index must be valid (-1 = void) or point into the palette
    for (int layer = 0; layer < 2; ++layer) {
        const auto &indices = s.layer(layer);
        ASSERT_EQ(expected, indices.size());
        for (int idx : indices) {
            EXPECT_GE(idx, -1);
            EXPECT_LT(idx, static_cast<int>(s.palette_size()));
        }
    }

    for (int x = 0; x < s.size_x(); ++x) {
        for (int y = 0; y < s.size_y(); ++y) {
            for (int z = 0; z < s.size_z(); ++z) {
                const auto flat = flat_index(x, y, z, s.size_y(), s.size_z());
                const int layer0 = s.layer(0)[flat];
                const int layer1 = s.layer(1)[flat];
                EXPECT_EQ(layer0, s.block_index(0, x, y, z));
                EXPECT_EQ(layer1, s.block_index(1, x, y, z));
                int effective = -1;
                for (int layer = 0; layer < 2; ++layer) {
                    const int idx = s.block_index(layer, x, y, z);
                    if (idx < 0) continue;
                    const auto *entry = s.palette_entry_at(static_cast<size_t>(idx));
                    if (entry && is_visible_block_name(entry->name)) {
                        effective = idx;
                        break;
                    }
                }
                EXPECT_EQ(effective, s.block_index(x, y, z));

                const auto *by_coord = s.block_at(x, y, z);
                if (effective < 0) {
                    EXPECT_EQ(by_coord, nullptr);
                } else {
                    ASSERT_NE(by_coord, nullptr);
                    ASSERT_NE(s.palette_entry_at(static_cast<size_t>(effective)), nullptr);
                    EXPECT_EQ(by_coord->name, s.palette_entry_at(static_cast<size_t>(effective))->name);
                }
            }
        }
    }
}

TEST(McStructure, HandlesInvalidData) {
    const byte_t garbage[] = {0x01, 0x02, 0x03};
    auto s = bl::parse_mcstructure(garbage, sizeof(garbage));
    EXPECT_EQ(s.size_x(), 0);
    EXPECT_EQ(s.size_y(), 0);
    EXPECT_EQ(s.size_z(), 0);
    EXPECT_TRUE(s.palette().empty());
    EXPECT_TRUE(s.layer(0).empty());
    EXPECT_TRUE(s.layer(1).empty());
    EXPECT_EQ(s.palette_entry_at(0), nullptr);
    EXPECT_EQ(s.block_at(0, 0, 0), nullptr);
    EXPECT_EQ(s.block_at(0, 0, 0, 0), nullptr);
}

TEST(McStructure, DumpSummary) {
    auto raw = load_mcstructure();
    ASSERT_FALSE(raw.empty());
    auto s = bl::parse_mcstructure(raw.data(), raw.size());

    auto text = s.dump();
    EXPECT_NE(text.find("size="), std::string::npos);
    EXPECT_NE(text.find("palette ("), std::string::npos);
    EXPECT_NE(text.find("layer 0:"), std::string::npos);
    EXPECT_NE(text.find("entities:"), std::string::npos);
}

TEST(McStructureBuilder, BuildAndDeduplicate) {
    auto stone = make_block_state_tag("minecraft:stone");
    auto chest = make_block_entity_tag("minecraft:chest");

    auto structure = bl::mcstructure_builder({3, 2, 2}, {0, 0, 0})
                         .set_block({0, 0, 0}, stone.get())
                         .fill_blocks(1, bl::block_box{{0, 0, 0}, {3, 2, 2}}, stone.get())
                         .set_block_entity({1, 1, 1}, chest.get())
                         .build();

    EXPECT_EQ(structure.size_x(), 3);
    EXPECT_EQ(structure.size_y(), 2);
    EXPECT_EQ(structure.size_z(), 2);
    EXPECT_EQ(structure.palette_size(), 1u);
    EXPECT_NE(structure.block_at(0, 0, 0), nullptr);
    EXPECT_NE(structure.block_at(2, 1, 1), nullptr);
    EXPECT_EQ(structure.block_entity_count(), 1u);
    ASSERT_EQ(structure.block_entities().size(), 1u);

    const auto *entity = structure.block_entities().front();
    ASSERT_NE(entity, nullptr);
    auto *x = entity->get("x");
    auto *y = entity->get("y");
    auto *z = entity->get("z");
    ASSERT_NE(x, nullptr);
    ASSERT_NE(y, nullptr);
    ASSERT_NE(z, nullptr);
    EXPECT_EQ(x->as<bl::nbt::int_tag *>()->value, 1);
    EXPECT_EQ(y->as<bl::nbt::int_tag *>()->value, 1);
    EXPECT_EQ(z->as<bl::nbt::int_tag *>()->value, 1);
}

TEST(McStructureBuilder, SerializeBlockEntities) {
    auto stone = make_block_state_tag("minecraft:stone");
    auto chest = make_block_entity_tag("minecraft:chest");

    auto structure = bl::mcstructure_builder({2, 2, 2}, {0, 0, 0})
                         .set_block({0, 0, 0}, stone.get())
                         .set_block_entity({1, 1, 1}, chest.get())
                         .build();

    auto dumped = structure.to_raw();
    EXPECT_NE(dumped.find("block_position_data"), std::string::npos);
    EXPECT_NE(dumped.find("block_entity_data"), std::string::npos);

    auto roundtrip = bl::parse_mcstructure(reinterpret_cast<const byte_t *>(dumped.data()), dumped.size());
    ASSERT_EQ(roundtrip.block_entity_count(), 1u);
    ASSERT_EQ(roundtrip.block_entities().size(), 1u);

    const auto *entity = roundtrip.block_entities().front();
    ASSERT_NE(entity, nullptr);
    auto *x = entity->get("x");
    auto *y = entity->get("y");
    auto *z = entity->get("z");
    ASSERT_NE(x, nullptr);
    ASSERT_NE(y, nullptr);
    ASSERT_NE(z, nullptr);
    EXPECT_EQ(x->as<bl::nbt::int_tag *>()->value, 1);
    EXPECT_EQ(y->as<bl::nbt::int_tag *>()->value, 1);
    EXPECT_EQ(z->as<bl::nbt::int_tag *>()->value, 1);
}

TEST(McStructureBuilder, WritesBlockEntityWorldPositionFromOrigin) {
    auto chest = make_block_entity_tag("minecraft:chest");

    auto structure = bl::mcstructure_builder({4, 4, 4}, {10, 20, 30})
                         .set_block_entity({1, 2, 3}, chest.get())
                         .build();

    ASSERT_EQ(structure.block_entity_count(), 1u);
    EXPECT_EQ(structure.block_entity_position(0), (bl::block_pos{1, 2, 3}));

    const auto *entity = structure.block_entities().front();
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ(entity->get("x")->as<bl::nbt::int_tag *>()->value, 11);
    EXPECT_EQ(entity->get("y")->as<bl::nbt::int_tag *>()->value, 22);
    EXPECT_EQ(entity->get("z")->as<bl::nbt::int_tag *>()->value, 33);

    const auto dumped = structure.to_raw();
    auto roundtrip = bl::parse_mcstructure(reinterpret_cast<const byte_t *>(dumped.data()), dumped.size());
    ASSERT_EQ(roundtrip.block_entity_count(), 1u);
    EXPECT_EQ(roundtrip.block_entity_position(0), (bl::block_pos{1, 2, 3}));

    const auto *roundtrip_entity = roundtrip.block_entities().front();
    ASSERT_NE(roundtrip_entity, nullptr);
    EXPECT_EQ(roundtrip_entity->get("x")->as<bl::nbt::int_tag *>()->value, 11);
    EXPECT_EQ(roundtrip_entity->get("y")->as<bl::nbt::int_tag *>()->value, 22);
    EXPECT_EQ(roundtrip_entity->get("z")->as<bl::nbt::int_tag *>()->value, 33);
}

TEST(McStructureBuilder, KeepsDuplicateBlockEntitiesAtDifferentPositions) {
    auto chest = make_block_entity_tag("minecraft:chest");

    auto structure = bl::mcstructure_builder({3, 3, 3}, {0, 0, 0})
                         .set_block_entity({0, 0, 0}, chest.get())
                         .set_block_entity({2, 1, 1}, chest.get())
                         .build();

    EXPECT_EQ(structure.block_entity_count(), 2u);
    ASSERT_EQ(structure.block_entities().size(), 2u);

    const auto *first = structure.block_entities()[0];
    const auto *second = structure.block_entities()[1];
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_EQ(first->get("x")->as<bl::nbt::int_tag *>()->value, 0);
    EXPECT_EQ(first->get("y")->as<bl::nbt::int_tag *>()->value, 0);
    EXPECT_EQ(first->get("z")->as<bl::nbt::int_tag *>()->value, 0);
    EXPECT_EQ(second->get("x")->as<bl::nbt::int_tag *>()->value, 2);
    EXPECT_EQ(second->get("y")->as<bl::nbt::int_tag *>()->value, 1);
    EXPECT_EQ(second->get("z")->as<bl::nbt::int_tag *>()->value, 1);

    auto dumped = structure.to_raw();
    EXPECT_NE(dumped.find("block_position_data"), std::string::npos);
    EXPECT_NE(dumped.find("block_entity_data"), std::string::npos);
    auto roundtrip = bl::parse_mcstructure(reinterpret_cast<const byte_t *>(dumped.data()), dumped.size());
    EXPECT_EQ(roundtrip.block_entity_count(), 2u);
}

TEST(McStructure, UsesBlockPositionDataKeyAsBlockEntityPosition) {
    auto raw = make_mcstructure_with_block_entity_position_key();
    auto structure = bl::parse_mcstructure(raw.data(), raw.size());

    ASSERT_EQ(structure.block_entity_count(), 1u);
    const auto pos = structure.block_entity_position(0);
    EXPECT_EQ(pos.x, 1);
    EXPECT_EQ(pos.y, 1);
    EXPECT_EQ(pos.z, 1);

    const auto *entity = structure.block_entities().front();
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ(entity->get("x")->as<bl::nbt::int_tag *>()->value, 99);
    EXPECT_EQ(entity->get("y")->as<bl::nbt::int_tag *>()->value, 88);
    EXPECT_EQ(entity->get("z")->as<bl::nbt::int_tag *>()->value, 77);

    auto dumped = structure.to_raw();
    auto roundtrip = bl::parse_mcstructure(reinterpret_cast<const byte_t *>(dumped.data()), dumped.size());
    ASSERT_EQ(roundtrip.block_entity_count(), 1u);
    const auto roundtrip_pos = roundtrip.block_entity_position(0);
    EXPECT_EQ(roundtrip_pos.x, 1);
    EXPECT_EQ(roundtrip_pos.y, 1);
    EXPECT_EQ(roundtrip_pos.z, 1);
}

TEST(McStructure, SerializeRoundTrip) {
    auto raw = load_mcstructure();
    ASSERT_FALSE(raw.empty());

    auto s = bl::parse_mcstructure(raw.data(), raw.size());
    auto dumped = s.to_raw();
    ASSERT_FALSE(dumped.empty());
    EXPECT_NE(dumped.find("structure_world_origin"), std::string::npos);

    auto roundtrip = bl::parse_mcstructure(reinterpret_cast<const byte_t *>(dumped.data()), dumped.size());
    EXPECT_EQ(roundtrip.size(), s.size());
    EXPECT_EQ(roundtrip.origin(), s.origin());
    EXPECT_EQ(roundtrip.palette_size(), s.palette_size());
    EXPECT_EQ(roundtrip.layer_size(0), s.layer_size(0));
    EXPECT_EQ(roundtrip.layer_size(1), s.layer_size(1));
    EXPECT_EQ(roundtrip.entity_count(), s.entity_count());
    EXPECT_EQ(roundtrip.block_entity_count(), s.block_entity_count());

    for (int x = 0; x < s.size_x(); ++x) {
        for (int y = 0; y < s.size_y(); ++y) {
            for (int z = 0; z < s.size_z(); ++z) {
                EXPECT_EQ(roundtrip.block_index(x, y, z), s.block_index(x, y, z));
            }
        }
    }

    auto temp = std::filesystem::temp_directory_path() /
                ("bedrockmap_mcstructure_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()) + ".mcstructure");
    ASSERT_TRUE(s.save_to_file(temp.string()));
    auto written = bl::utils::read_file(temp.string());
    EXPECT_EQ(std::vector<byte_t>(dumped.begin(), dumped.end()), written);
    std::filesystem::remove(temp);
}
