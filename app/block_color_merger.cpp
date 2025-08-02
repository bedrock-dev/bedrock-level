#include <cstdio>
#include <string>
#include <unordered_set>

#include "json/json.hpp"

using namespace nlohmann;

int main(int argc, const char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.json...>\n", argv[0]);
        return 1;
    }
    json result;
    std::pmr::unordered_set<std::string> unique_names;
    for (int i = 1; i < argc; ++i) {
        const char* filename = argv[i];
        FILE* file = fopen(filename, "r");
        if (!file) {
            fprintf(stderr, "Error opening file: %s\n", filename);
            return 1;
        }

        json j;
        try {
            j = json::parse(file);
        } catch (json::parse_error& e) {
            fprintf(stderr, "Error parsing JSON from file %s: %s\n", filename, e.what());
            fclose(file);
            return 1;
        }
        fclose(file);
        for (auto& item : j) {
            auto name = item["name"].get<std::string>();
            auto status = item["states"].dump();
            name += status;
            if (unique_names.find(name) != unique_names.end()) {
                continue;
            } else {
                result.push_back(item);
                unique_names.insert(name);
            }
        }
    }

    auto r = result.dump(4);
    printf("%s", r.c_str());
}