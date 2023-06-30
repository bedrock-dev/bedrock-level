//
// Created by xhy on 2023/6/29.
//

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "color.h"
#include "json/json.hpp"

int main() {
    const std::string filename = R"(C:\Users\xhy\dev\bedrock-level\data\colors\block.json)";
    bl::init_block_color_palette_from_file(filename);
    auto &table = bl::get_block_color_table();
    std::ofstream f(R"(C:\Users\xhy\dev\bedrock-level\data\colors\block_dump.txt)");
    for (auto &kv : table) {
        f << kv.first;
    }

    f.close();
}