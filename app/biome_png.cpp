//
// Created by xhy on 2023/3/31.
//

#include <string>

#include "bedrock_level.h"
#include "color.h"

struct A {
    A() { this->x = new int[10]; }
    int *x;

    ~A() { delete[] this->x; }
};
std::vector<A *> get() {
    std::vector<A *> r;
    for (int i = 0; i < 10; i++) {
        r.emplace_back();
    }
    return r;
}

int main() {
    bl::init_biome_color_palette_from_file(
        R"(C:\Users\xhy\dev\bedrock-level\data\colors\biome.json)");

    const std::string path = R"(C:\Users\xhy\Desktop\t)";
    auto level = bl::bedrock_level();
    level.open(path);

    auto spawn_pos = level.dat().spawn_position();
    auto cp = spawn_pos.to_chunk_pos();

    cp.dim = 0;

    auto center_chunk_pos = spawn_pos.to_chunk_pos();

    const int DIM = 0;
    const int R = 40;
    auto minP = bl::chunk_pos{center_chunk_pos.x - R, center_chunk_pos.z - R, DIM};
    auto maxP = bl::chunk_pos{center_chunk_pos.x + R, center_chunk_pos.z + R, DIM};
    const int W = maxP.x - minP.x + 1;
    const int H = maxP.z - minP.z + 1;
    std::vector<std::vector<bl::color>> cm(H * 16, std::vector<bl::color>(W * 16, bl::color()));
    for (int x = minP.x; x <= maxP.x; x++) {
        for (int z = minP.z; z <= maxP.z; z++) {
            auto *chunk = level.get_chunk({x, z, DIM});
        }
    }

    //    bl::export_image(cm, 1, "biome.png");
    return 0;
}