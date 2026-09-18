//
// Geometric primitives shared by the bedrock-level data model.
//

#ifndef BEDROCK_LEVEL_GEOMETRY_H
#define BEDROCK_LEVEL_GEOMETRY_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>

namespace bl {

    struct block_pos;
    struct chunk_pos {
        int32_t x{0};
        int32_t z{0};
        int32_t dim{-1};

        chunk_pos(int32_t xx, int32_t zz, int32_t dimension) : x(xx), z(zz), dim(dimension) {}

        chunk_pos() = default;

        [[nodiscard]] bool valid() const { return this->dim >= 0; }

        [[nodiscard]] std::string to_string() const;

        bool operator==(const chunk_pos& p) const;

        bool operator<(const chunk_pos& rhs) const;

        [[nodiscard]] bool is_slime() const;
    };

    /// World Y that a Data3D biome payload starts at. The payload does not record where it
    /// begins, so this is a property of the dimension rather than of any particular chunk: it is
    /// the bottom of the dimension. Unrelated to how a chunk is serialized -- Data3D only exists
    /// in worlds whose height is the 1.18+ one. Dimensions without a built-in convention read the
    /// floor from bl::config.
    [[nodiscard]] int32_t dimension_min_y(int32_t dim) noexcept;

    struct block_pos {
        int x{};
        int y{};
        int z{};

        block_pos() = default;
        block_pos(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}

        [[nodiscard]] chunk_pos to_chunk_pos() const;

        [[nodiscard]] chunk_pos in_chunk_offset() const;

        [[nodiscard]] bool operator==(const block_pos& rhs) const noexcept { return x == rhs.x && y == rhs.y && z == rhs.z; }
        [[nodiscard]] bool operator!=(const block_pos& rhs) const noexcept { return !(*this == rhs); }
        [[nodiscard]] block_pos operator+(const block_pos& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
        [[nodiscard]] block_pos operator-(const block_pos& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
        block_pos& operator+=(const block_pos& rhs) noexcept {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
            return *this;
        }
        block_pos& operator-=(const block_pos& rhs) noexcept {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
            return *this;
        }
    };

    // Axis-aligned integer block region. The minimum corner is inclusive and
    // the maximum corner is exclusive: [min_pos, max_pos).
    struct block_box {
        block_pos min_pos{0, 0, 0};
        block_pos max_pos{0, 0, 0};

        block_box() = default;
        block_box(const block_pos& minimum, const block_pos& maximum) : min_pos(minimum), max_pos(maximum) {}

        [[nodiscard]] static block_box from_min_and_size(const block_pos& minimum, int size_x, int size_y, int size_z) noexcept {
            return {{minimum.x, minimum.y, minimum.z}, {minimum.x + size_x, minimum.y + size_y, minimum.z + size_z}};
        }

        [[nodiscard]] bool is_valid() const noexcept { return min_pos.x < max_pos.x && min_pos.y < max_pos.y && min_pos.z < max_pos.z; }

        [[nodiscard]] int size_x() const noexcept { return max_pos.x - min_pos.x; }
        [[nodiscard]] int size_y() const noexcept { return max_pos.y - min_pos.y; }
        [[nodiscard]] int size_z() const noexcept { return max_pos.z - min_pos.z; }

        [[nodiscard]] bool contains(const block_pos& pos) const noexcept {
            return pos.x >= min_pos.x && pos.x < max_pos.x && pos.y >= min_pos.y && pos.y < max_pos.y && pos.z >= min_pos.z &&
                   pos.z < max_pos.z;
        }

        [[nodiscard]] block_box normalized() const noexcept {
            return {{std::min(min_pos.x, max_pos.x), std::min(min_pos.y, max_pos.y), std::min(min_pos.z, max_pos.z)},
                    {std::max(min_pos.x, max_pos.x), std::max(min_pos.y, max_pos.y), std::max(min_pos.z, max_pos.z)}};
        }

        [[nodiscard]] block_box intersected(const block_box& rhs) const noexcept {
            return {{std::max(min_pos.x, rhs.min_pos.x), std::max(min_pos.y, rhs.min_pos.y), std::max(min_pos.z, rhs.min_pos.z)},
                    {std::min(max_pos.x, rhs.max_pos.x), std::min(max_pos.y, rhs.max_pos.y), std::min(max_pos.z, rhs.max_pos.z)}};
        }

        [[nodiscard]] block_box translated(int dx, int dy, int dz) const noexcept { return translated(block_pos{dx, dy, dz}); }

        [[nodiscard]] block_box translated(const block_pos& offset) const noexcept { return {min_pos + offset, max_pos + offset}; }
    };

    struct vec3 {
        float x{};
        float y{};
        float z{};

        vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
    };

}  // namespace bl

namespace std {

    template <>
    struct hash<bl::chunk_pos> {
        size_t operator()(const bl::chunk_pos& cp) const noexcept {
            size_t h1 = hash<int32_t>{}(cp.x);
            size_t h2 = hash<int32_t>{}(cp.z);
            size_t h3 = hash<int32_t>{}(cp.dim);
            return h1 ^ (h2 << 7) ^ (h3 << 15);
        }
    };
}  // namespace std

#endif  // BEDROCK_LEVEL_GEOMETRY_H
