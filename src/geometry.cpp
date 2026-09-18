//
// Geometric primitives shared by the bedrock-level data model.
//

#include "geometry.h"

#include <random>

#include "config.h"

namespace bl {

    std::string chunk_pos::to_string() const {
        return std::to_string(this->x) + ", " + std::to_string(this->z) + ", " + std::to_string(this->dim);
    }

    bool chunk_pos::operator<(const chunk_pos& rhs) const {
        if (x < rhs.x) return true;
        if (rhs.x < x) return false;
        if (z < rhs.z) return true;
        if (rhs.z < z) return false;
        return dim < rhs.dim;
    }

    bool chunk_pos::operator==(const chunk_pos& p) const { return this->x == p.x && this->dim == p.dim && this->z == p.z; }

    int32_t dimension_min_y(int32_t dim) noexcept {
        if (dim == 1 || dim == 2) return 0;  // nether and the end keep their legacy 0-based floors
        if (dim == 0) return -64;            // overworld, 1.18+
        // Custom dimensions have no built-in convention; the host configures their floor.
        return config::custom_dimension_min_y();
    }

    bool chunk_pos::is_slime() const {
        auto seed = (x * 0x1f1f1f1fu) ^ (uint32_t)z;
        std::mt19937 mt(seed);
        return mt() % 10 == 0;
    }

    chunk_pos block_pos::to_chunk_pos() const {
        auto cx = x < 0 ? x - 15 : x;
        auto cz = z < 0 ? z - 15 : z;
        return {cx / 16, cz / 16, -1};
    }

    chunk_pos block_pos::in_chunk_offset() const {
        auto ox = x % 16;
        auto oz = z % 16;
        if (ox < 0) ox += 16;
        if (oz < 0) oz += 16;
        return {ox, oz, -1};
    }

}  // namespace bl
