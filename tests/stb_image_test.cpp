//
// Created by xhy on 2023/6/18.
//

#include <gtest/gtest.h>
#include "stb/stb_image_write.h"
#include "utils.h"

TEST(StbImage, WritePNG) {
    const int c = 3;
    const int w = 60;
    const int h = 160;
    std::vector<unsigned char> data(c * w * h, 0);
    for (int y = 0; y < w; y++) {
        for (int x = 0; x < h; x++) {
            data[3 * (y * h + x)] = 0;
            data[3 * (y * h + x) + 1] = 128;
            data[3 * (y * h + x) + 2] = 128;
        }
    }

    stbi_write_png("a.png", w, h, c, data.data(), 0);
}