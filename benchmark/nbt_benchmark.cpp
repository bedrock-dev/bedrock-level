#include "nbt.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

using clock_type = std::chrono::steady_clock;

bl::nbt::compound_tag make_sample(std::size_t entry_count) {
    bl::nbt::compound_tag root("root");

    auto* entries = new bl::nbt::list_tag("entries");
    entries->value.reserve(entry_count);
    for (std::size_t i = 0; i < entry_count; ++i) {
        auto* entry = new bl::nbt::compound_tag("");
        entry->put(new bl::nbt::int_tag("id", static_cast<int32_t>(i)));
        entry->put(new bl::nbt::long_tag("seed", static_cast<int64_t>(i) * 31));
        entry->put(new bl::nbt::float_tag("value", static_cast<float>(i) * 0.25f));
        entry->put(new bl::nbt::string_tag("name", "minecraft:benchmark_entry"));
        entries->append(entry);
    }
    root.put(entries);
    return root;
}

template <typename Function>
double measure_ns_per_operation(Function&& function, std::size_t iterations) {
    const auto start = clock_type::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        function();
    }
    const auto elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start).count();
    return elapsed / static_cast<double>(iterations);
}

}  // namespace

int main() {
    // Four children per entry plus the entry compound and the root/list nodes.
    constexpr std::size_t entry_count = 100'000;
    constexpr std::size_t node_count = entry_count * 5 + 2;
    constexpr std::size_t iterations = 10;
    const auto sample = make_sample(entry_count);
    const auto serialized = sample.to_raw();

    volatile std::size_t serialized_bytes = 0;
    const auto serialize_ns = measure_ns_per_operation(
        [&] {
            const auto raw = sample.to_raw();
            serialized_bytes += raw.size();
        },
        iterations);

    volatile std::size_t parsed_bytes = 0;
    const auto deserialize_ns = measure_ns_per_operation(
        [&] {
            int read = 0;
            auto* parsed = bl::nbt::read_one_palette(
                reinterpret_cast<const byte_t*>(serialized.data()), serialized.size(), read);
            parsed_bytes += static_cast<std::size_t>(read);
            delete parsed;
        },
        iterations);

    const auto mib = [](double bytes) { return bytes / (1024.0 * 1024.0); };
    const auto serialize_mib_per_second = mib(static_cast<double>(serialized.size())) * 1'000'000'000.0 / serialize_ns;
    const auto deserialize_mib_per_second = mib(static_cast<double>(serialized.size())) * 1'000'000'000.0 / deserialize_ns;
    const auto serialize_nodes_per_second = static_cast<double>(node_count) * 1'000'000'000.0 / serialize_ns;
    const auto deserialize_nodes_per_second = static_cast<double>(node_count) * 1'000'000'000.0 / deserialize_ns;

    std::cout << "NBT benchmark (" << node_count << " nodes, " << iterations << " iterations, " << serialized.size()
              << " bytes)\n"
              << "  serialize:   " << serialize_ns / 1'000'000.0 << " ms/op, " << serialize_mib_per_second
              << " MiB/s, " << serialize_nodes_per_second << " nodes/s\n"
              << "  deserialize: " << deserialize_ns / 1'000'000.0 << " ms/op, " << deserialize_mib_per_second
              << " MiB/s, " << deserialize_nodes_per_second << " nodes/s\n"
              << "  checksums:   " << serialized_bytes << ", " << parsed_bytes << '\n';
    return 0;
}
