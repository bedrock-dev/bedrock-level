//
// Created by xhy on 2023/3/29.
//

#include "utils.h"

#include <gtest/gtest.h>

TEST(Utils, Logger) {
    int a = 1;
    LOG_F(ERROR, "This is a error message with a  = %d", a);
    LOG_F(INFO, "This is a logger message with a  = %d", a);
}