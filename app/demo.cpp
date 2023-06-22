//
// Created by xhy on 2023/6/22.
//
#include "bedrock_key.h"
#include "bedrock_level.h"
int main() {
    const std::string path = R"(C:\Users\xhy\Desktop\t)";
    bl::bedrock_level level;
    level.open(path);

    return 0;
}