//
// Created by xhy on 2026/8/5.
//
// Dump raw chunk data (BCHK format) from a world save for benchmark/testing.
// Usage: dump_chunks <level path> <output dir>
// Dumps chunks of dims {0=Overworld, 1=Nether, 2=TheEnd}, x/z in [-5, 5]
// (11x11 = 121 slots per dim, 363 slots total).
// Missing chunks are written as empty (0-byte) files so the whole grid is present.

#include <cstdio>
#include <filesystem>
#include <string>

#include "bedrock_level.h"
#include "chunk.h"
#include "utils.h"

namespace fs = std::filesystem;

int main(int argc, const char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: dump_chunks <level path> <output dir>\n");
        return 1;
    }

    const std::string level_path = argv[1];
    const std::string out_dir = argv[2];

    bl::bedrock_level level;
    if (!level.open(level_path)) {
        fprintf(stderr, "Can not open level %s\n", level_path.c_str());
        return 1;
    }
    fs::create_directories(out_dir);

    static constexpr int DIMS[] = {0, 1, 2};
    static constexpr int RANGE = 5;
    const int slots_per_dim = (2 * RANGE + 1) * (2 * RANGE + 1);

    int total = 0, existing = 0;
    size_t total_bytes = 0;
    for (int dim : DIMS) {
        int dim_existing = 0;
        for (int x = -RANGE; x <= RANGE; x++) {
            for (int z = -RANGE; z <= RANGE; z++) {
                bl::raw_chunk rc(bl::chunk_pos{x, z, dim});
                bool exists = rc.read(level) && (!rc.get_sub_chunks().empty() || !rc.get_entities().empty());
                auto raw = rc.to_raw();
                char filename[128];
                snprintf(filename, sizeof(filename), "dim%d_cx%d_cz%d.chunk", dim, x, z);
                auto path = (fs::path(out_dir) / filename).string();
                bl::utils::write_file(path, raw.data(), raw.size());
                if (exists) {
                    dim_existing++;
                    total_bytes += raw.size();
                }
                total++;
            }
        }
        existing += dim_existing;
        printf("dim %d: %d/%d chunks present\n", dim, dim_existing, slots_per_dim);
    }

    printf("total: %d/%d chunks present, %zu bytes written to %s\n", existing, total, total_bytes, out_dir.c_str());
    level.close();
    return 0;
}
