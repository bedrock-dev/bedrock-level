//
// Created by xhy on 2026/8/8.
//
// Benchmark NBT (de)serialization on a large file (tests/data/nbts/large.nbt).
// Skipped when the file is absent, so machines without the fixture still pass.

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "nbt.h"
#include "utils.h"

namespace fs = std::filesystem;

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

using steady_clock_t = std::chrono::steady_clock;

namespace {
    const fs::path kLargeNbt = fs::path(TEST_DATA_DIR) / "nbts" / "large.nbt";
    constexpr int kRounds = 3;

    // in-memory footprint of the parsed tag tree (64-bit, libstdc++)
    struct TreeStats {
        size_t tags = 0;
        size_t payload_bytes = 0;  // raw value bytes held by the tree
        size_t est_bytes = 0;      // estimated object bytes
    };

    void accumulateTree(const bl::nbt::abstract_tag *t, TreeStats &s) {
        s.tags++;
        switch (t->type()) {
            case bl::nbt::String: {
                const auto *st = static_cast<const bl::nbt::string_tag *>(t);
                s.payload_bytes += st->value.size();
                s.est_bytes += 72 + (st->value.size() > 15 ? st->value.size() : 0);
                break;
            }
            case bl::nbt::ByteArray: {
                const auto *a = static_cast<const bl::nbt::byte_array_tag *>(t);
                s.payload_bytes += a->value.size();
                s.est_bytes += 64 + a->value.size();
                break;
            }
            case bl::nbt::IntArray: {
                const auto *a = static_cast<const bl::nbt::int_array_tag *>(t);
                s.payload_bytes += a->value.size() * 4;
                s.est_bytes += 64 + a->value.size() * 4;
                break;
            }
            case bl::nbt::LongArray: {
                const auto *a = static_cast<const bl::nbt::long_array_tag *>(t);
                s.payload_bytes += a->value.size() * 8;
                s.est_bytes += 64 + a->value.size() * 8;
                break;
            }
            case bl::nbt::List: {
                const auto *l = static_cast<const bl::nbt::list_tag *>(t);
                s.payload_bytes += l->value.size() * 8;
                s.est_bytes += 64 + l->value.size() * 8;
                for (const auto *c : l->value) accumulateTree(c, s);
                break;
            }
            case bl::nbt::Compound: {
                const auto *c = static_cast<const bl::nbt::compound_tag *>(t);
                s.payload_bytes += c->value.size() * 40;
                s.est_bytes += 64 + c->value.size() * 40;
                for (const auto &kv : c->value) {
                    s.payload_bytes += kv.first.size();
                    s.est_bytes += (kv.first.size() > 15 ? kv.first.size() : 0);
                    accumulateTree(kv.second, s);
                }
                break;
            }
            default:
                s.est_bytes += 48;  // scalars
                break;
        }
    }
}  // namespace

TEST(LargeNbt, DeserializeTime) {
    if (!fs::exists(kLargeNbt)) GTEST_SKIP() << "large.nbt not present: " << kLargeNbt.string();
    auto data = bl::utils::read_file(kLargeNbt.string());
    ASSERT_FALSE(data.empty());

    size_t tags = 0;
    auto start = steady_clock_t::now();
    for (int r = 0; r < kRounds; r++) {
        auto palettes = bl::nbt::read_palette_to_end(data.data(), data.size());
        tags = palettes.size();
        for (auto *p : palettes) delete p;
    }
    auto ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "deserialize " << data.size() << " bytes -> " << tags << " tag(s) in " << ms << " ms (" << (ms / kRounds)
              << " ms/round)\n";
    EXPECT_GT(tags, 0u);

    // one extra parse to estimate the in-memory footprint of the tag tree
    auto palettes = bl::nbt::read_palette_to_end(data.data(), data.size());
    TreeStats st;
    for (auto *p : palettes) accumulateTree(p, st);
    for (auto *p : palettes) delete p;
    const double raw_mb = data.size() / (1024.0 * 1024.0);
    const double tree_mb = st.est_bytes / (1024.0 * 1024.0);
    std::cout << "estimate: " << st.tags << " tag(s), " << st.payload_bytes / (1024.0 * 1024.0) << " MiB payload, ~" << tree_mb
              << " MiB tree + " << raw_mb << " MiB raw buffer -> ~" << (tree_mb + raw_mb) << " MiB peak (deserialize)\n";
}

TEST(LargeNbt, SerializeTime) {
    if (!fs::exists(kLargeNbt)) GTEST_SKIP() << "large.nbt not present: " << kLargeNbt.string();
    auto data = bl::utils::read_file(kLargeNbt.string());
    ASSERT_FALSE(data.empty());

    auto palettes = bl::nbt::read_palette_to_end(data.data(), data.size());
    ASSERT_FALSE(palettes.empty());

    size_t total_bytes = 0;
    auto start = steady_clock_t::now();
    for (int r = 0; r < kRounds; r++) {
        total_bytes = 0;
        for (auto *p : palettes) total_bytes += p->to_raw().size();
    }
    auto ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "serialize " << palettes.size() << " tag(s) -> " << total_bytes << " bytes in " << ms << " ms (" << (ms / kRounds)
              << " ms/round)\n";
    EXPECT_GT(total_bytes, 0u);
    for (auto *p : palettes) delete p;
}
