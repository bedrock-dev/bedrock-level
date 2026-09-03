#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "chunk.h"
#include "utils.h"

namespace fs = std::filesystem;

uintmax_t getTotalLdbSize(const fs::path &path) {
    uintmax_t totalSize = 0;

    std::error_code ec;
    if (!fs::exists(path, ec) || ec) {
        throw fs::filesystem_error("路径不存在或无法访问", path, ec);
    }

    if (fs::is_regular_file(path, ec) && !ec) {
        if (path.extension() == ".ldb") {
            return fs::file_size(path, ec);
        }
        return 0;
    }

    if (fs::is_directory(path, ec) && !ec) {
        for (const auto &entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }

            if (entry.is_regular_file(ec) && !ec && !entry.is_symlink(ec) && entry.path().extension() == ".ldb") {
                uintmax_t fsize = entry.file_size(ec);
                if (!ec) {
                    totalSize += fsize;
                } else {
                    ec.clear();
                }
            }
        }
    }
    return totalSize;
}

void PrintDatabaseStats(leveldb::DB *db) {
    if (db == nullptr) return;
    std::string stats;
    if (db->GetProperty("leveldb.stats", &stats)) {
        printf("stats:\n %s\n", stats.c_str());
    } else {
        LOG_F(ERROR, "Can not get leveldb.stats properties");
    }
    uint64_t approximate_size;
    leveldb::Range range(leveldb::Slice(""), leveldb::Slice("\xff\xff\xff\xff"));
    db->GetApproximateSizes(&range, 1, &approximate_size);
    printf("Approximate Total Size: %zu bytes (%.2f MiB)", approximate_size, (approximate_size / 1024.0 / 1024.0));
}

// ----------------------------- arguments -----------------------------

struct Options {
    fs::path path;
    bool stat{false};
    bool export_chunks{false};
    bool unpack{false};
    fs::path output;
    bool has_pos{false};
    bool range{false};
    int x1{0}, z1{0}, x2{0}, z2{0};
    int dim{0};
};

void printUsage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --path <level> [mode] [options]\n"
            "\n"
            "Modes (default: --stat):\n"
            "  --stat                    print level database statistics\n"
            "  --export                  export chunks\n"
            "\n"
            "Export options:\n"
            "  --pos <x> <z>             single chunk at (x, z)\n"
            "  --pos <x1> <z1> <x2> <z2> export chunks in rectangle [x1..x2] x [z1..z2]\n"
            "  --dim <d>                 dimension id (default 0)\n"
            "  --unpack                  unpack every non-empty key into its own file\n"
            "                            (one folder per chunk); default writes one .bchk\n"
            "                            file per chunk\n"
            "  -o, --output <dir>        output directory (required for --export)\n"
            "  -h, --help                show this help\n",
            prog);
}

bool parseInt(const std::string &s, int &out) {
    if (s.empty()) return false;
    char *end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool parseArgs(int argc, const char *argv[], Options &opt) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto takeValue = [&](std::string &out) {
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
        } else if (a == "--stat") {
            opt.stat = true;
        } else if (a == "--export") {
            opt.export_chunks = true;
        } else if (a == "--unpack") {
            opt.unpack = true;
        } else if (a == "--output" || a == "-o") {
            std::string v;
            if (!takeValue(v)) return false;
            opt.output = v;
        } else if (a == "--dim") {
            std::string v;
            if (!takeValue(v)) return false;
            if (!parseInt(v, opt.dim)) {
                fprintf(stderr, "Invalid --dim value: %s\n", v.c_str());
                return false;
            }
        } else if (a == "--pos") {
            std::vector<int> nums;
            while (i + 1 < argc) {
                int v;
                if (!parseInt(argv[i + 1], v)) break;
                nums.push_back(v);
                i++;
            }
            if (nums.size() == 2) {
                opt.x1 = opt.x2 = nums[0];
                opt.z1 = opt.z2 = nums[1];
                opt.range = false;
            } else if (nums.size() == 4) {
                opt.x1 = nums[0];
                opt.z1 = nums[1];
                opt.x2 = nums[2];
                opt.z2 = nums[3];
                opt.range = true;
            } else {
                fprintf(stderr, "--pos expects 2 or 4 integers (got %zu)\n", nums.size());
                return false;
            }
            opt.has_pos = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

// ----------------------------- export -----------------------------

std::string hexEncode(const std::string &s) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s) {
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0xf]);
    }
    return out;
}

const char *keySuffix(bl::chunk_key::key_type kt) {
    switch (kt) {
        case bl::chunk_key::BlockEntity:
        case bl::chunk_key::Entity:
        case bl::chunk_key::PendingTicks:
            return ".nbt";
        default:
            return ".bin";
    }
}

std::string chunkDirName(const bl::chunk_pos &cp) {
    return "chunk_" + std::to_string(cp.x) + "_" + std::to_string(cp.z) + "_" + std::to_string(cp.dim);
}

bool chunkPresent(const bl::raw_chunk &rc) { return !rc.get_sub_chunks().empty() || !rc.get_entities().empty(); }

bool exportChunk(bl::bedrock_level &level, const bl::chunk_pos &cp, const fs::path &outDir, bool unpack) {
    bl::raw_chunk rc(cp);
    if (!rc.read(level) || !chunkPresent(rc)) {
        fprintf(stderr, "  [skip] chunk %s not present\n", cp.to_string().c_str());
        return false;
    }

    if (!unpack) {
        auto raw = rc.to_raw();
        auto path = outDir / (chunkDirName(cp) + ".bchk");
        bl::utils::write_file(path.string(), raw.data(), raw.size());
        printf("  wrote %s (%zu bytes)\n", path.filename().string().c_str(), raw.size());
        return true;
    }

    auto dir = outDir / chunkDirName(cp);
    fs::create_directories(dir);
    size_t files = 0, bytes = 0;
    for (const auto &[kt, data] : rc.get_normal_data()) {
        if (data.empty()) continue;
        bl::utils::write_file((dir / (bl::chunk_key::chunk_key_to_str(kt) + keySuffix(kt))).string(), data.data(), data.size());
        files++, bytes += data.size();
    }
    for (const auto &[idx, data] : rc.get_sub_chunks()) {
        if (data.empty()) continue;
        bl::utils::write_file((dir / ("SubChunkTerrain_" + std::to_string(idx) + ".bin")).string(), data.data(), data.size());
        files++, bytes += data.size();
    }
    if (!rc.get_actor_digest().empty()) {
        const auto &d = rc.get_actor_digest();
        bl::utils::write_file((dir / "ActorDigest.bin").string(), d.data(), d.size());
        files++, bytes += d.size();
    }
    for (const auto &[uid, data] : rc.get_entities()) {
        if (data.empty()) continue;
        bl::utils::write_file((dir / ("Entity_" + hexEncode(uid) + ".bin")).string(), data.data(), data.size());
        files++, bytes += data.size();
    }
    printf("  unpacked %s -> %s (%zu files, %zu bytes)\n", cp.to_string().c_str(), dir.string().c_str(), files, bytes);
    return true;
}

// ----------------------------- stats -----------------------------

int printStats(bl::bedrock_level &level, const std::string &path) {
    auto fileSz = getTotalLdbSize(path);
    printf("Data Stats for level %s:\n", level.dat().level_name().c_str());
    printf("RealDB size %zu bytes (%.2f MiB)\n", fileSz, (fileSz / (1024.0 * 1024.0)));
    PrintDatabaseStats(level.db());
    level.close();
    return 0;
}

int main(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
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
    if (opt.stat && opt.export_chunks) {
        fprintf(stderr, "--stat and --export are mutually exclusive\n");
        return 1;
    }
    if (!opt.stat && !opt.export_chunks) opt.stat = true;  // default mode

    bl::bedrock_level level;
    if (!level.open(opt.path.string())) {
        LOG_F(ERROR, "Can not open level %s", opt.path.string().c_str());
        return 1;
    }

    if (opt.stat) return printStats(level, opt.path.string());

    // export
    if (!opt.has_pos) {
        fprintf(stderr, "--export requires --pos\n");
        return 1;
    }
    if (opt.output.empty()) {
        fprintf(stderr, "--export requires --output <dir>\n");
        return 1;
    }
    fs::create_directories(opt.output);

    int x1 = opt.x1, z1 = opt.z1, x2 = opt.x2, z2 = opt.z2;
    if (x1 > x2) std::swap(x1, x2);
    if (z1 > z2) std::swap(z1, z2);

    int total = 0, present = 0;
    for (int x = x1; x <= x2; x++) {
        for (int z = z1; z <= z2; z++) {
            total++;
            if (exportChunk(level, bl::chunk_pos{x, z, opt.dim}, opt.output, opt.unpack)) present++;
        }
    }
    printf("%s %d chunk(s) to %s (%d present)\n", opt.unpack ? "Unpacked" : "Exported", total, opt.output.string().c_str(), present);
    level.close();
    return 0;
}
