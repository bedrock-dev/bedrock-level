//
// Created by xhy on 2023/3/31.
// Updated: benchmark tests based on dumped raw chunks (tests/data).
//

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "bedrock_key.h"
#include "chunk.h"
#include "nbt.h"
#include "utils.h"

namespace fs = std::filesystem;

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

namespace {
    // allocation counting (this TU only) for leak regression checks
    std::atomic<long> g_alloc_net{0};
}  // namespace

// custom global new/delete trip GCC's conservative -Wmismatched-new-delete (malloc/free pair is correct)
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void *operator new(std::size_t n) {
    g_alloc_net.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(n);
}
void operator delete(void *p) noexcept {
    if (p) g_alloc_net.fetch_sub(1, std::memory_order_relaxed);
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept { operator delete(p); }

namespace {

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

    // collect raw nbt payloads of actors + pending ticks from a raw chunk
    void collect_palette_raws(const bl::raw_chunk &rc, std::vector<std::string> &out) {
        // actors (new version): one compound per entity
        for (auto &[uid, raw] : rc.get_entities()) {
            if (!raw.empty()) out.push_back(raw);
        }
        // actors (old version): concatenated compounds in Entity key
        auto entity_raw = rc.get_normal_key(bl::chunk_key::Entity);
        if (!entity_raw.empty()) out.push_back(entity_raw);
        // pending ticks
        auto tick_raw = rc.get_normal_key(bl::chunk_key::PendingTicks);
        if (!tick_raw.empty()) out.push_back(tick_raw);
    }

    using steady_clock_t = std::chrono::steady_clock;

}  // namespace

// Loads all dumped chunks once, then benchmarks palette parse/serialize.
class PaletteBenchmark : public ::testing::Test {
   protected:
    void SetUp() override {
        files_ = list_chunk_files();
        ASSERT_FALSE(files_.empty());
        for (auto &f : files_) {
            auto raw = bl::utils::read_file(f);
            if (raw.empty()) continue;
            bl::raw_chunk rc;
            if (rc.from_raw(raw)) {
                collect_palette_raws(rc, raw_palettes_);
            }
        }
        ASSERT_FALSE(raw_palettes_.empty());
    }

    std::vector<std::string> files_;
    std::vector<std::string> raw_palettes_;
};

// parse all actor/pending-tick palettes into tag trees
TEST_F(PaletteBenchmark, ParseAllPalettes) {
    constexpr int kRounds = 5;
    auto start = steady_clock_t::now();
    size_t tags = 0;
    for (int round = 0; round < kRounds; round++) {
        tags = 0;
        for (auto &raw : raw_palettes_) {
            auto palettes = bl::nbt::read_palette_to_end(reinterpret_cast<const byte_t *>(raw.data()), raw.size());
            tags += palettes.size();
            for (auto *p : palettes) delete p;
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();

    std::cout << "parse " << raw_palettes_.size() << " palettes -> " << tags << " tags in " << elapsed_ms << " ms (" << elapsed_ms / kRounds
              << " ms/round)\n";
    EXPECT_GT(tags, 0u);
}

// serialize all parsed palettes back to raw bytes
TEST_F(PaletteBenchmark, SerializeAllPalettes) {
    constexpr int kRounds = 5;
    auto start = steady_clock_t::now();
    size_t total_bytes = 0;
    for (int round = 0; round < kRounds; round++) {
        total_bytes = 0;
        for (auto &raw : raw_palettes_) {
            auto palettes = bl::nbt::read_palette_to_end(reinterpret_cast<const byte_t *>(raw.data()), raw.size());
            for (auto *p : palettes) {
                total_bytes += p->to_raw().size();
            }
            for (auto *p : palettes) delete p;
        }
    }
    auto elapsed_ms = std::chrono::duration<double, std::milli>(steady_clock_t::now() - start).count();
    std::cout << "serialize " << raw_palettes_.size() << " palettes -> " << total_bytes << " bytes in " << elapsed_ms << " ms ("
              << elapsed_ms / kRounds << " ms/round)\n";
    EXPECT_GT(total_bytes, 0u);
}

// parse -> to_raw must reproduce the input bytes exactly (parse/serialize are inverses)
TEST_F(PaletteBenchmark, RoundTrip) {
    size_t ok = 0;
    for (auto &raw : raw_palettes_) {
        auto palettes = bl::nbt::read_palette_to_end(reinterpret_cast<const byte_t *>(raw.data()), raw.size());
        std::string reencoded;
        for (auto *p : palettes) reencoded += p->to_raw();
        if (reencoded == raw) ok++;
        for (auto *p : palettes) delete p;
    }
    std::cout << "round trip ok: " << ok << "/" << raw_palettes_.size() << "\n";
    EXPECT_EQ(ok, raw_palettes_.size());
}

// serialize -> parse -> serialize must be stable (idempotent, byte-identical on 2nd round)
TEST_F(PaletteBenchmark, ReSerializeStable) {
    for (auto &raw : raw_palettes_) {
        auto first = bl::nbt::read_palette_to_end(reinterpret_cast<const byte_t *>(raw.data()), raw.size());
        std::string first_raw;
        for (auto *p : first) first_raw += p->to_raw();
        for (auto *p : first) delete p;

        auto second = bl::nbt::read_palette_to_end(reinterpret_cast<const byte_t *>(first_raw.data()), first_raw.size());
        std::string second_raw;
        for (auto *p : second) second_raw += p->to_raw();
        for (auto *p : second) delete p;

        EXPECT_EQ(first_raw, second_raw) << "2nd serialization differs for palette of " << first_raw.size() << " bytes";
    }
}

// assignment must release the destination's previous children (net alloc returns to baseline)
TEST(PaletteLeak, AssignmentReleasesOld) {
    auto base = g_alloc_net.load(std::memory_order_relaxed);
    {
        bl::nbt::compound_tag c1("a");
        c1.put(new bl::nbt::int_tag("x", 1));
        c1.put(new bl::nbt::string_tag("y", "hello"));
        bl::nbt::compound_tag c2("b");
        c2.put(new bl::nbt::int_tag("x", 2));
        c2.put(new bl::nbt::float_tag("z", 1.5f));
        c2 = c1;  // must free c2's old children
        EXPECT_EQ(c2.value.size(), 2u);
    }
    EXPECT_EQ(g_alloc_net.load(std::memory_order_relaxed), base) << "operator= leaked children";
}

// parsing a compound with a duplicate key must drop the older tag without leaking
TEST(PaletteLeak, DuplicateKeyParse) {
    auto base = g_alloc_net.load(std::memory_order_relaxed);
    {
        std::string raw;
        raw.push_back(static_cast<char>(10));  // Compound
        uint16_t klen = 0;
        raw.append(reinterpret_cast<const char *>(&klen), 2);
        // child 1: Int "k"
        raw.push_back(static_cast<char>(3));
        uint16_t l1 = 1;
        raw.append(reinterpret_cast<const char *>(&l1), 2);
        raw += 'k';
        int32_t v1 = 1;
        raw.append(reinterpret_cast<const char *>(&v1), 4);
        // child 2: String "k" (duplicate key)
        raw.push_back(static_cast<char>(8));
        uint16_t l2 = 1;
        raw.append(reinterpret_cast<const char *>(&l2), 2);
        raw += 'k';
        uint16_t sl = 2;
        raw.append(reinterpret_cast<const char *>(&sl), 2);
        raw += "hi";
        raw.push_back(0);  // End

        int read = 0;
        auto *nbt = bl::nbt::read_one_palette(raw.data(), raw.size(), read);
        ASSERT_NE(nbt, nullptr);
        EXPECT_EQ(nbt->value.size(), 1u);  // older Int "k" dropped
        EXPECT_NE(nbt->get("k"), nullptr);
        delete nbt;
    }
    EXPECT_EQ(g_alloc_net.load(std::memory_order_relaxed), base) << "duplicate-key parse leaked a tag";
}