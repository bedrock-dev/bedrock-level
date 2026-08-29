
#include "config.h"

namespace {
    bool log_mismatched_actor_{true};
    bool log_missing_block_color_{false};
}  // namespace
namespace bl::config {
    void set_log_mismatched_actor(bool enable) { log_mismatched_actor_ = enable; }
    bool log_mismatched_actor() { return log_mismatched_actor_; }

    void set_log_missing_block_color(bool enable) { log_missing_block_color_ = enable; }
    bool log_missing_block_color() { return log_missing_block_color_; }
}  // namespace bl::config