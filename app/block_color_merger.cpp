#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_set>

#include "json/json.hpp"

using namespace nlohmann;

int main(int argc, const char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.json...> <output.json>\n", argv[0]);
        return 1;
    }
    json result;
    std::pmr::unordered_set<std::string> unique_names;
    for (int i = 1; i < argc - 1; ++i) {
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

    std::ofstream out(argv[argc - 1]);
    if (!out) {
        fprintf(stderr, "Error opening output file: %s\n", argv[argc - 1]);
        return 1;
    }
    out << result.dump(4);
    out.close();
    return 0;
}