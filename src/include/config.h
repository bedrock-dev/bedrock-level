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

    /// Sub-chunk Y index window (inclusive) scanned when reading a chunk's terrain. The default
    /// spans -4..19, i.e. y -64..319 of the 1.18+ overworld. Dimensions whose terrain sits
    /// outside that window need it widened, otherwise their sub-chunks never make it back.
    void set_subchunk_index_range(int8_t minimum, int8_t maximum);
    std::pair<int8_t, int8_t> subchunk_index_range();

    /// World floor used for dimensions with no built-in convention, i.e. anything other than
    /// the overworld / nether / end. Custom dimensions can place it wherever they like.
    void set_custom_dimension_min_y(int32_t minimum);
    int32_t custom_dimension_min_y();

    /// Multiplier applied to the biome tint colors (grass / leaves / water) when the biome color
    /// table is loaded, so the map can be lightened or darkened without editing the tables. 1.0
    /// (the default) keeps them as authored.
    ///
    /// Only tints are scaled: a tinted block stores a gray value and is colored by
    /// `gray / 255 * tint`, so scaling the tint changes every such pixel exactly once. The block
    /// table is left alone, because untinted blocks (stone, sand, ...) already match the
    /// reference renderer.
    ///
    /// The tables are baked at load time, so a change only takes effect on the next table load.
    void set_color_brightness(float factor);
    float color_brightness();
}  // namespace bl::config

#endif