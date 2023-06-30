//
// Created by xhy on 2023/6/28.
//

#include <iostream>

#include "bedrock_level.h"
int main() {
    const std::string path = R"(C:\Users\xhy\Desktop\t)";
    auto level = bl::bedrock_level();
    level.open(path);

    auto spawn_pos = level.dat().spawn_position();
    auto cp = spawn_pos.to_chunk_pos();

    cp.dim = 0;
    auto *chunk = level.get_chunk(cp);

    if (!chunk) {
        BL_LOGGER("can not read chunk");
        return 0;
    }

    for (int i = 0; i < 64; i++) {
        chunk->get_block_raw(0, i, 0)->write(std::cout, 0);
    }

    delete chunk;

    return 0;
}