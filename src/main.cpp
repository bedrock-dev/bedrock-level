//
// Created by xhy on 2023/3/28.
//
#include <iostream>
#include "leveldb/db.h"

int main() {
    leveldb::DB *db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "demo", &db);
    assert(status.ok());
    return 0;
}