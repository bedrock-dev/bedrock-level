//
// Created by xhy on 2023/6/28.
//

#include <iostream>

#include "bedrock_level.h"
#include "utils.h"
int main() {
    const std::string path = R"(C:\Users\xhy\Desktop\worlds\231)";
    auto level = bl::bedrock_level();
    level.open(path);
    if (!level.is_open()) {
        BL_LOGGER("Can not open %s", path.c_str());
    }
    auto key = std::string("player_server_1cac9061-77c2-4922-8a56-c6ec4c0b7cce");

    std::string value;

    level.db()->Get(leveldb::ReadOptions(), key, &value);

    bl::utils::write_file("a.mcstructure", value.data(), value.size());
    int read = 0;
    BL_LOGGER("%d", value.size());
    auto* b = bl::palette::read_one_palette(value.data(), read);
    b->write(std::cout, 0);

    //    level.load_global_data();
    return 0;
}