
#include "config.h"

namespace {
    bool log_mismatched_actor_{true};

}
namespace bl::config {
    void set_log_mismatched_actor(bool enable) { log_mismatched_actor_ = enable; }
    bool log_mismatched_actor() { return log_mismatched_actor_; }
}  // namespace bl::config