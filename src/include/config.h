#ifndef BEDROCK_LEVEL_CONFIG_H
#define BEDROCK_LEVEL_CONFIG_H

#include <cstdint>
#include <utility>

namespace bl::config {
    void set_log_mismatched_actor(bool);
    bool log_mismatched_actor();

    void set_log_missing_block_color(bool);
    bool log_missing_block_color();

    // strict chunk existence check: require non-empty marker key value
    void set_strict_chunk_existence(bool);
    bool strict_chunk_existence();

    /// Inclusive sub-chunk Y range scanned for terrain; default is -4..19 (-64..319).
    void set_subchunk_index_range(int8_t minimum, int8_t maximum);
    std::pair<int8_t, int8_t> subchunk_index_range();

    /// World floor for dimensions without an overworld, nether, or end convention.
    void set_custom_dimension_min_y(int32_t minimum);
    int32_t custom_dimension_min_y();

}  // namespace bl::config

#endif
