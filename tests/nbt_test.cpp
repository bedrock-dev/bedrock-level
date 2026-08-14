//
// Created by xhy on 2023/3/29.
//

#include "nbt.h"

#include <gtest/gtest.h>

#include <string>


namespace {
    // root -> A(compound) -> B(list) -> [0]/[1] compounds, each with an int "C"
    bl::nbt::compound_tag *make_tree() {
        auto *root = new bl::nbt::compound_tag("");
        auto *a = new bl::nbt::compound_tag("A");
        auto *b = new bl::nbt::list_tag("B");
        auto *e0 = new bl::nbt::compound_tag("");
        auto *e1 = new bl::nbt::compound_tag("");
        e0->put(new bl::nbt::int_tag("C", 42));
        e1->put(new bl::nbt::int_tag("C", 43));
        b->append(e0);
        b->append(e1);
        a->put(b);
        root->put(a);
        return root;
    }
}  // namespace

TEST(NbtGetByPath, FindsNestedNodes) {
    auto *root = make_tree();

    auto *c1 = root->getByPath("A.B[1].C");
    ASSERT_NE(c1, nullptr);
    EXPECT_EQ(c1->as<bl::nbt::int_tag *>()->value, 43);

    auto *c0 = root->getByPath("A.B[0].C");
    ASSERT_NE(c0, nullptr);
    EXPECT_EQ(c0->as<bl::nbt::int_tag *>()->value, 42);

    // direct compound->compound path without a list
    EXPECT_EQ(root->getByPath("A"), root->get("A"));

    delete root;
}

TEST(NbtGetByPath, ReturnsNullOnAnyMiss) {
    auto *root = make_tree();

    EXPECT_EQ(root->getByPath("A.X"), nullptr);       // missing key
    EXPECT_EQ(root->getByPath("A.B[5].C"), nullptr);  // index out of range
    EXPECT_EQ(root->getByPath("A.B.C"), nullptr);     // B is a list, not a compound
    EXPECT_EQ(root->getByPath("A.B[x]"), nullptr);    // malformed index
    EXPECT_EQ(root->getByPath("A.B[]"), nullptr);     // empty index
    EXPECT_EQ(root->getByPath(""), root);             // empty path returns the node itself

    delete root;
}
