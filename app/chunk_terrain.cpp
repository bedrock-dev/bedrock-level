//
// Created by xhy on 2023/6/28.
//

#include <iostream>

#include "bedrock_level.h"
int main() {
    const std::string path = R"(C:\Users\xhy\Desktop\SAC_survival)";
    auto level = bl::bedrock_level();
    level.open(path);
    level.load_global_data();
    auto &vs = level.village_list().data();
    for (auto &kv : vs) {
        std::cout << kv.first << "\n";
        int z = 0;
        for (auto &p : kv.second) {
            std::cout << z;
            p->write(std::cout, 0);
            z++;
        }
    }

    return 0;
}