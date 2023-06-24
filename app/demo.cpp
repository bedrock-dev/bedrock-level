//
// Created by xhy on 2023/6/22.
//
#include <list>
#include <unordered_map>
#include <utility>

#include "bedrock_key.h"
#include "bedrock_level.h"

template <typename Key, typename Value>
class LRUCache {
   public:
    explicit LRUCache(int capacity) : cap(capacity) {}
    LRUCache() : LRUCache(1024) {}

    Value *get(const Key &key) {
        auto iter = this->cache.find(key);
        if (iter == cache.end()) {
            return nullptr;
        }
        auto value = iter->second->second;
        list.erase(iter->second);
        list.insert(list.begin(), {key, value});
        cache[key] = list.begin();
        return value;
    }

    void put(const Key &key, Value *value) {
        auto iter = this->cache.find(key);
        if (iter == cache.end()) {
            this->list.insert(list.begin(), {key, value});
            this->cache[key] = list.begin();
            if (list.size() > cap) {
                auto last = list.back().first;
                list.pop_back();
                cache.erase(last);
            }
        } else {
            // 找到了,擦除旧的，并挪动到头部
            list.erase(iter->second);
            this->list.insert(list.begin(), {key, value});
            this->cache[key] = list.begin();
        }
    }

   private:
    std::list<std::pair<Key, Value *>> list;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value *>>::iterator> cache;
    int cap{};
};

int main() {
    const std::string path = R"(C:\Users\xhy\Desktop\t)";
    bl::bedrock_level level;
    level.open(path);

    return 0;
}