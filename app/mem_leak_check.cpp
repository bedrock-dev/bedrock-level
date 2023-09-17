//
// Created by xhy on 2023/8/18.
//
#include <iostream>

#include "bedrock_level.h"
#include "palette.h"
#include "utils.h"

void demo_check() { int *a = new int; }

void nbt_leak_check() {
    const auto path =
        "/mnt/c/Users/xhy/dev/bedrock-level/data/dumps/actors/144115188092633088.palette";

    auto file = bl::utils::read_file(path);
    auto p = bl::palette::read_palette_to_end(file.data(), file.size());
    for (auto i : p) delete i;
}

void level_read_leak_check() {
    const auto path = "/mnt/d/MC/saves/2g";
    bl::bedrock_level l;

    assert(l.open(path));
    for (int i = -10; i <= 10; i++) {
        for (int j = -10; j <= 10; j++) {
            auto *ch = l.get_chunk({i, j, 0}, true);
            if (ch) {
                std::cout << ch->get_block(0, 32, 0).name << std::endl;
                printf("%d\n", ch->get_pos().x);
                delete ch;
            }
        }
    }

    l.close();
}

int main() {
    level_read_leak_check();
    return 0;
}
