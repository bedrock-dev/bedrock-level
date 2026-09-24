//
// Created by xhy on 2026/9/24.
//
// db_bench: measures how fast the level's LevelDB can be traversed key by key.
//
// Usage: db_bench --path <level> [--codec zlib|deflate|both] [--runs N] [--warmup N] [--prefix <str>]
//
// Values are never read: it->value() is what drags the data blocks through
// decompression, and the point of this benchmark is the key path alone.
//
// The same scan range is timed twice per round:
//   raw   - a bare leveldb iterator walking Slice views, no allocation per entry.
//           This is the ceiling of the storage layer.
//   parse - the app's key handling on top of it: every key copied into a
//           std::string and classified with chunk_key/actor_key::parse(), which
//           is what the map renderer does before it looks at any value.
//
// The whole scan runs once per codec in --codec, on a level opened for it, so the
// zlib and libdeflate numbers are directly comparable. Blocks are decompressed
// either way, so this is the knob that decides how fast a full traversal goes.
//
// The first round of each phase is a warmup (cold page cache) unless --warmup 0.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"

namespace fs = std::filesystem;

namespace {

    constexpr double MIB = 1024.0 * 1024.0;

    enum class Codec { Zlib, Libdeflate };

    const char* codecName(Codec c) { return c == Codec::Zlib ? "zlib" : "libdeflate"; }

    struct Options {
        fs::path path;
        int runs{5};
        int warmup{1};
        std::string prefix;
        bool both_codecs{true};
        bool use_libdeflate{false};
    };

    struct PassResult {
        uint64_t keys{0};
        uint64_t key_bytes{0};
        uint64_t chunk_keys{0};
        uint64_t actor_keys{0};
        double seconds{0.0};

        [[nodiscard]] double key_mib() const { return static_cast<double>(key_bytes) / MIB; }
        [[nodiscard]] double keys_per_second() const { return seconds > 0.0 ? static_cast<double>(keys) / seconds : 0.0; }
        [[nodiscard]] double mib_per_second() const { return seconds > 0.0 ? key_mib() / seconds : 0.0; }
    };

    struct PhaseSummary {
        uint64_t keys{0};
        double key_mib{0.0};
        double best_keys_per_s{0.0};
        double mean_keys_per_s{0.0};
    };

    struct CodecResult {
        Codec codec{Codec::Zlib};
        PhaseSummary raw;
        PhaseSummary parse;
    };

    void printUsage(const char* prog) {
        fprintf(stderr,
                "Usage: %s --path <level> [options]\n"
                "\n"
                "Benchmarks LevelDB key traversal over a world save (keys only, values\n"
                "are never read).\n"
                "\n"
                "Options:\n"
                "  -p, --path <level>        path to the level folder (contains db/)\n"
                "  --codec <zlib|deflate|both>\n"
                "                            block decompressor to scan with (default both)\n"
                "  --runs <n>                timed rounds per phase (default 5)\n"
                "  --warmup <n>              untimed rounds per phase (default 1)\n"
                "  --prefix <str>            scan only keys starting with this string\n"
                "                            (default: every key in the database)\n"
                "  -h, --help                show this help\n",
                prog);
    }

    bool parseInt(const std::string& s, int& out) {
        if (s.empty()) return false;
        char* end = nullptr;
        long v = std::strtol(s.c_str(), &end, 10);
        if (end == s.c_str() || *end != '\0') return false;
        out = static_cast<int>(v);
        return true;
    }

    bool parseArgs(int argc, const char* argv[], Options& opt) {
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            auto takeValue = [&](std::string& out) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Missing value for %s\n", a.c_str());
                    return false;
                }
                out = argv[++i];
                return true;
            };

            if (a == "--path" || a == "-p") {
                std::string v;
                if (!takeValue(v)) return false;
                opt.path = v;
            } else if (a == "--prefix") {
                if (!takeValue(opt.prefix)) return false;
            } else if (a == "--runs" || a == "--warmup") {
                std::string v;
                if (!takeValue(v)) return false;
                int n = 0;
                if (!parseInt(v, n) || n < 0) {
                    fprintf(stderr, "Invalid value for %s: %s\n", a.c_str(), v.c_str());
                    return false;
                }
                if (a == "--runs")
                    opt.runs = n;
                else
                    opt.warmup = n;
            } else if (a == "--codec") {
                std::string v;
                if (!takeValue(v)) return false;
                if (v == "zlib") {
                    opt.both_codecs = false;
                    opt.use_libdeflate = false;
                } else if (v == "deflate" || v == "libdeflate") {
                    opt.both_codecs = false;
                    opt.use_libdeflate = true;
                } else if (v == "both") {
                    opt.both_codecs = true;
                } else {
                    fprintf(stderr, "Invalid --codec value: %s (expected zlib, deflate or both)\n", v.c_str());
                    return false;
                }
            } else {
                fprintf(stderr, "Unknown option: %s\n", a.c_str());
                return false;
            }
        }
        return true;
    }

    // Bare iterator over key Slice views: what the storage layer can do on its own.
    PassResult scanRaw(leveldb::DB* db, const Options& opt) {
        leveldb::ReadOptions read_options{};
        leveldb::Iterator* it = db->NewIterator(read_options);
        PassResult r;
        const auto begin = std::chrono::steady_clock::now();
        for (it->Seek(opt.prefix); it->Valid() && it->key().starts_with(opt.prefix); it->Next()) {
            r.keys++;
            r.key_bytes += it->key().size();
        }
        r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        if (!it->status().ok()) fprintf(stderr, "  [warn] iterator error: %s\n", it->status().ToString().c_str());
        delete it;
        return r;
    }

    // Adds the key handling the app does on top: string copy plus classification.
    PassResult scanParse(leveldb::DB* db, const Options& opt) {
        leveldb::ReadOptions read_options{};
        leveldb::Iterator* it = db->NewIterator(read_options);
        PassResult r;
        const auto begin = std::chrono::steady_clock::now();
        for (it->Seek(opt.prefix); it->Valid() && it->key().starts_with(opt.prefix); it->Next()) {
            const std::string key = it->key().ToString();
            r.keys++;
            r.key_bytes += key.size();
            if (bl::chunk_key::parse(key).valid())
                r.chunk_keys++;
            else if (bl::actor_key::parse(key).valid())
                r.actor_keys++;
        }
        r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        if (!it->status().ok()) fprintf(stderr, "  [warn] iterator error: %s\n", it->status().ToString().c_str());
        delete it;
        return r;
    }

    const char* phaseName(int phase) { return phase == 0 ? "raw" : "parse"; }

    void printPass(int phase, int run, const PassResult& r) {
        printf("  %-5s run %2d: %9llu keys  %8.2f MiB  %8.1f ms  %7.2f M keys/s  %8.1f MiB/s\n", phaseName(phase), run,
               static_cast<unsigned long long>(r.keys), r.key_mib(), r.seconds * 1000.0, r.keys_per_second() / 1e6, r.mib_per_second());
    }

    void printSummary(int phase, const std::vector<PassResult>& runs) {
        double best_keys = 0.0, best_mib = 0.0, sum_keys = 0.0, sum_mib = 0.0;
        for (const auto& r : runs) {
            best_keys = std::max(best_keys, r.keys_per_second());
            best_mib = std::max(best_mib, r.mib_per_second());
            sum_keys += r.keys_per_second();
            sum_mib += r.mib_per_second();
        }
        const auto n = static_cast<double>(runs.size());
        printf("  %-5s best/mean: %.2f / %.2f M keys/s, %.1f / %.1f MiB/s\n\n", phaseName(phase), best_keys / 1e6, sum_keys / n / 1e6,
               best_mib, sum_mib / n);
    }

    PhaseSummary runPhase(int phase, const Options& opt, leveldb::DB* db, PassResult& last_pass) {
        std::vector<PassResult> runs;
        runs.reserve(static_cast<size_t>(opt.runs));
        for (int i = 0; i < opt.warmup; i++) {
            auto r = phase == 0 ? scanRaw(db, opt) : scanParse(db, opt);
            printf("  %-5s warmup: %9llu keys  %8.2f MiB  %8.1f ms\n", phaseName(phase), static_cast<unsigned long long>(r.keys),
                   r.key_mib(), r.seconds * 1000.0);
        }
        for (int i = 0; i < opt.runs; i++) {
            auto r = phase == 0 ? scanRaw(db, opt) : scanParse(db, opt);
            printPass(phase, i + 1, r);
            runs.push_back(r);
        }
        if (runs.empty()) return PhaseSummary{};

        printSummary(phase, runs);
        last_pass = runs.back();
        PhaseSummary s;
        s.keys = runs.front().keys;
        s.key_mib = runs.front().key_mib();
        double sum_keys = 0.0;
        for (const auto& r : runs) {
            s.best_keys_per_s = std::max(s.best_keys_per_s, r.keys_per_second());
            sum_keys += r.keys_per_second();
        }
        s.mean_keys_per_s = sum_keys / static_cast<double>(runs.size());
        return s;
    }

    void printComparison(const std::vector<CodecResult>& results) {
        if (results.size() < 2) return;
        printf("=== codec comparison (best round of each phase) ===\n");
        printf("  %-11s %-6s %10s %12s %12s\n", "codec", "phase", "time", "keys/s", "MiB/s");
        for (const auto& res : results) {
            for (int phase = 0; phase < 2; phase++) {
                const auto& s = phase == 0 ? res.raw : res.parse;
                if (s.best_keys_per_s <= 0.0) continue;
                const double seconds = static_cast<double>(s.keys) / s.best_keys_per_s;
                printf("  %-11s %-6s %9.1f s %9.2f M %11.1f\n", codecName(res.codec), phaseName(phase), seconds, s.best_keys_per_s / 1e6,
                       s.key_mib / seconds);
            }
        }
        for (int phase = 0; phase < 2; phase++) {
            const auto& zlib = phase == 0 ? results[0].raw : results[0].parse;
            const auto& deflate = phase == 0 ? results[1].raw : results[1].parse;
            if (zlib.best_keys_per_s <= 0.0 || deflate.best_keys_per_s <= 0.0) continue;
            printf("  %s: libdeflate is %.2fx zlib\n", phaseName(phase), deflate.best_keys_per_s / zlib.best_keys_per_s);
        }
        printf("\n");
    }

}  // namespace

int main(int argc, const char* argv[]) {
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    Options opt;
    if (!parseArgs(argc, argv, opt) || opt.path.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<Codec> codecs;
    if (opt.both_codecs || !opt.use_libdeflate) codecs.push_back(Codec::Zlib);
    if (opt.both_codecs || opt.use_libdeflate) codecs.push_back(Codec::Libdeflate);

    printf("=== db_bench: LevelDB key traversal (keys only) ===\n");
    printf("level      : %s\n", opt.path.string().c_str());
    printf("scan range : %s\n", opt.prefix.empty() ? "<all keys>" : ("prefix \"" + opt.prefix + "\"").c_str());
    printf("rounds     : %d timed + %d warmup per phase\n\n", opt.runs, opt.warmup);

    std::vector<CodecResult> results;
    for (Codec codec : codecs) {
        bl::bedrock_level level(codec == Codec::Libdeflate);
        if (!level.open(opt.path.string())) {
            fprintf(stderr, "Can not open level %s\n", opt.path.string().c_str());
            return 1;
        }

        printf("--- %s [%s] ---\n", codecName(codec), level.dat().level_name().c_str());
        CodecResult res;
        res.codec = codec;
        PassResult last_raw;
        PassResult last_parse;
        res.raw = runPhase(0, opt, level.db(), last_raw);
        res.parse = runPhase(1, opt, level.db(), last_parse);
        if (last_parse.keys > 0)
            printf("  key types: chunk %llu, actor %llu, other %llu\n\n", static_cast<unsigned long long>(last_parse.chunk_keys),
                   static_cast<unsigned long long>(last_parse.actor_keys),
                   static_cast<unsigned long long>(last_parse.keys - last_parse.chunk_keys - last_parse.actor_keys));
        results.push_back(res);
        level.close();
    }

    printComparison(results);
    return 0;
}
