
#include "config.h"

namespace {
    bool log_mismatched_actor_{true};
    bool log_missing_block_color_{false};
    bool strict_chunk_existence_{true};
    // -4..19 covers y -64..319, the 1.18+ overworld
    std::pair<int8_t, int8_t> subchunk_index_range_{-4, 19};
    int32_t custom_dimension_min_y_{-64};
}  // namespace
namespace bl::config {
    void set_log_mismatched_actor(bool enable) { log_mismatched_actor_ = enable; }
    bool log_mismatched_actor() { return log_mismatched_actor_; }

    void set_log_missing_block_color(bool enable) { log_missing_block_color_ = enable; }
    bool log_missing_block_color() { return log_missing_block_color_; }

    void set_strict_chunk_existence(bool enable) { strict_chunk_existence_ = enable; }
    bool strict_chunk_existence() { return strict_chunk_existence_; }

    void set_subchunk_index_range(int8_t minimum, int8_t maximum) { subchunk_index_range_ = {minimum, maximum}; }
    std::pair<int8_t, int8_t> subchunk_index_range() { return subchunk_index_range_; }

    void set_custom_dimension_min_y(int32_t minimum) { custom_dimension_min_y_ = minimum; }
    int32_t custom_dimension_min_y() { return custom_dimension_min_y_; }
}  // namespace bl::config