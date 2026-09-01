#ifndef BEDROCK_LEVEL_CONFIG_H
#define BEDROCK_LEVEL_CONFIG_H

namespace bl::config {
    void set_log_mismatched_actor(bool);
    bool log_mismatched_actor();

    void set_log_missing_block_color(bool);
    bool log_missing_block_color();

    // strict chunk existence check: require non-empty marker key value
    void set_strict_chunk_existence(bool);
    bool strict_chunk_existence();
}  // namespace bl::config

#endif