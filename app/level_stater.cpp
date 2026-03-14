#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

#include "bedrock_key.h"
#include "bedrock_level.h"
#include "global.h"
#include "utils.h"

namespace fs = std::filesystem;

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

uintmax_t getTotalLdbSize(const fs::path& path) {
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
        for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
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

void PrintDatabaseStats(leveldb::DB* db) {
    if (db == nullptr) return;
    std::string stats;
    if (db->GetProperty("leveldb.stats", &stats)) {
        printf("stats:\n %s\n", stats.c_str());
    } else {
        BL_ERROR("Can not get leveldb.stats properties");
    }
    uint64_t approximate_size;
    leveldb::Range range(leveldb::Slice(""), leveldb::Slice("\xff\xff\xff\xff"));
    db->GetApproximateSizes(&range, 1, &approximate_size);
    printf("Approximate Total Size: %zu bytes (%.2f MiB)", approximate_size, (approximate_size / 1024.0 / 1024.0));
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        BL_ERROR("Use ./level_stater <level path>");
        return 1;
    }

    auto path = std::string(argv[1]);
    bl::bedrock_level level;
    if (!level.open(path)) {
        BL_ERROR("Can not open level %s", path.c_str());
    }
    auto fileSz = getTotalLdbSize(path);
    size_t chunk_key_num{0}, chunk_data_size{0};
    std::set<bl::chunk_pos> chunk_poses;
    size_t other_key_num{0}, other_data_size{0};
    printf("Data Stats for level %s:\n", level.dat().level_name().c_str());
    printf("RealDB size %zu bytes (%.2f MiB)\n", fileSz, (fileSz / (1024.0 * 1024.0)));
    PrintDatabaseStats(level.db());
    printf("Chunks: %zu (%zu), Total %zu bytes (%.2f MiB)\n", chunk_poses.size(), chunk_key_num, chunk_data_size,
           (chunk_data_size / (1024.0 * 1024.0)));
    printf("Other Data: %zu, Total %zu bytes (%.2f MiB)\n", other_key_num, other_data_size, (other_data_size / (1024.0 * 1024.0)));
    level.close();
    return 0;
}
