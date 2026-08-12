# Minecraft Bedrock 存档格式

基于 `bedrock-level` 库的源代码分析，总结 Minecraft Bedrock 版（基岩版）的存档格式。

## 概述

Minecraft Bedrock 版存档使用 **LevelDB** 作为底层键值存储数据库。整个存档由两部分组成：

- **`level.dat`**：存档元数据文件，使用 NBT（Named Binary Tag）格式编码，带有 8 字节头部
- **`db/`**：LevelDB 数据库目录，存储所有世界数据（区块、实体、村民、玩家等）

## 1. LevelDB 键空间

LevelDB 中的每条记录由一个键和对应的值组成。键的类型决定了其存储的数据类型。键分类如下：

### 1.1 区块键 (`chunk_key`)

区块键由 **区块坐标** + **维度** + **键类型** + **子区块索引**（可选）组成。

键类型枚举（来自 `chunk_key::key_type`）：

| 值  | 名称                                 | 说明                           |
| --- | ------------------------------------ | ------------------------------ |
| 43  | `Data3D` (0x2b)                      | 3D 数据：高度图 + 3D 生物群系  |
| 44  | `VersionNew` (0x2c)                  | 新版区块版本号                 |
| 45  | `Data2D` (0x2d)                      | 2D 数据：旧版高度图 + 生物群系 |
| 46  | `Data2DLegacy` (0x2e)                | 遗留 2D 数据                   |
| 47  | `SubChunkTerrain` (0x2f)             | 子区块地形数据（方块存储）     |
| 48  | `LegacyTerrain`                      | 遗留地形数据                   |
| 49  | `BlockEntity` (0x31)                 | 方块实体（容器、命令方块等）   |
| 50  | `Entity` (0x32)                      | 实体（旧版存储，新版不再使用） |
| 51  | `PendingTicks` (0x33)                | 待处理的刻（计划刻）           |
| 52  | `BlockExtraData`                     | 方块额外数据                   |
| 53  | `BiomeState`                         | 生物群系状态                   |
| 54  | `FinalizedState`                     | 区块终态标志                   |
| 55  | `ConversionData`                     | 转换数据                       |
| 56  | `BorderBlocks`                       | 边界方块（教育版）             |
| 57  | `HardCodedSpawnAreas`                | 硬编码生成区域（结构生成）     |
| 58  | `RandomTicks`                        | 随机刻                         |
| 59  | `Checksums` (0x3b)                   | 校验和                         |
| 60  | `GenerationSeed`                     | 生成种子                       |
| 61  | `GeneratedPreCavesAndCliffsBlending` | 洞穴与山崖预生成混合信息       |
| 62  | `BlendingBiomeHeight`                | 混合生物群系高度               |
| 63  | `MetaDataHash`                       | 元数据哈希                     |
| 64  | `BlendingData`                       | 混合数据                       |
| 65  | `ActorDigestVersion`                 | 实体摘要版本                   |
| 118 | `VersionOld` (0x76)                  | 旧版区块版本号（`v` 字符）     |

区块键的序列化格式（二进制）：

```
[4字节: 区块X坐标] [4字节: 区块Z坐标] [1字节: 键类型] [1字节: 维度]
// 对于 SubChunkTerrain，额外有 [1字节: Y索引]
```

### 1.2 实体键 (`actor_key`)

新版本实体以独立键存储，键格式：

```
`actorprefix` + [8字节: 实体唯一ID (UID)]
```

### 1.3 实体摘要键 (`actor_digest_key`)

摘要键记录一个区块内有哪些实体，格式：

```
`digp` + [4字节: X坐标] [4字节: Z坐标] [1字节: 维度]
```

值内容为 8 字节重复排列的 UID 列表，每个实体的实际数据通过 `actorprefix` + UID 键读取。

### 1.4 村民键 (`village_key`)

用于存储村庄/掠夺者前哨站数据，键格式：

```
`Villages` + [36字节: UUID字符串] + [4字节: 维度] + [类型后缀]
```

类型后缀可以是 `info`、`dwellers`、`players`、`poi`。

### 1.5 全局键

不是区块键、实体键、村民键的条目统称为全局键，包括：

- `player` 前缀：玩家数据
- `map_*` 前缀：地图物品数据
- `mobevents`：事件数据
- `Overworld`、`Nether`、`TheEnd`：维度数据
- `scoreboard`：计分板
- `LocalPlayer`：本地玩家
- `autonomousentities`：自治实体

## 2. 区块结构

每个区块（Chunk）是 16×256(或384)×16 的区域，由以下部分组成：

### 2.1 坐标系统

- **区块坐标** (`chunk_pos`)：由 `(x, z, dim)` 三元组标识
  - `dim`: 0=主世界, 1=地狱, 2=末地
  - 区块在 x/z 方向覆盖 16 个方块
- **方块坐标** (`block_pos`)：由 `(x, y, z)` 标识方块级位置
  - 可通过 `block_pos::to_chunk_pos()` 计算所属区块
  - 可通过 `block_pos::in_chunk_offset()` 获得在区块内的偏移

### 2.2 Y 轴范围

不同维度/版本的 Y 轴范围不同：

| 维度   | Y 最小值 | Y 最大值 | 说明                 |
| ------ | -------- | -------- | -------------------- |
| 主世界 | -64      | 319      | 1.18+ 洞穴与山崖更新 |
| 地狱   | 0        | 127      |                      |
| 末地   | 0        | 255      |                      |

Y 到子区块的映射函数 `map_y_to_subchunk`：

```
子区块索引 = y < 0 ? (y - 15) / 16 : y / 16
子区块内偏移 = y % 16  (若为负数则加16)
```

### 2.3 子区块 (`sub_chunk`)

每个子区块覆盖 16×16×16 的区域，按 Y 轴从低到高排列。

**子区块版本**：

- 版本 8：1.2~1.17 格式，无 Y 索引字段
- 版本 9：1.18+ 格式，包含 1 字节 Y 索引

**子区块头部**（二进制）：

```
[1字节: 版本号] [1字节: 子区块层数]
// 版本9额外有：
[1字节: Y索引]
```

### 2.4 方块存储格式

每个子区块包含一到多个层（Layer）。方块（以及方块状态/属性）通过 **调色板 + 索引** 的方式进行存储，极大地压缩了存储空间。

**层头部**（1 字节）：

```
[bit 0]: 类型 (0=调色板, 1=运行时)
[bit 1-7]: 每个方块的比特数 (bits per block)
```

- 如果 `bits == 0`：该层是 **均匀块**（uniform），所有方块相同，调色板只有 1 个条目
- 如果 `bits > 0`：方块索引按位打包在 `blocks` 数组中

**索引数据**（位打包）：

```
每个字(4字节)包含 floor(32/bits) 个索引
索引数量 = 4096 (16×16×16)
总字数 = ceil(4096 / 每字索引数)
```

每个索引对应调色板中的一个位置。

**调色板**：

```
[4字节: 调色板长度 N]
然后依次排列 N 个 NBT Compound 标签
```

每个调色板条目是一个 NBT Compound，包含方块的完整定义，通常有：

```nbt
{
    "name": "minecraft:stone",     // 方块名称
    "version": 17879555,           // 版本号（可选）
    "states": {                    // 方块状态（可选）
        "facing": "north",
        "waterlogged": false
    }
}
```

### 2.5 区块数据加载流程

1. 检查区块是否存在（通过 `VersionOld` 或 `VersionNew` 键）
2. 加载子区块（遍历所有 Y 索引，读取 `SubChunkTerrain` 键）
3. 加载生物群系和高度图
4. 加载实体
5. 按策略位掩码加载（Terrain/PendingTick/Actor/BlockActor/Others，默认 All）
6. 加载 HSA（硬编码生成区域）

## 3. 生物群系和高度图

### 3.1 旧版 (Data2D, key=45)

- 512 字节高度图（256×2 字节，短整型）
- 256 字节生物群系（256×1 字节，每个方块一个群系ID）

区块级别的 2D 生物群系，整个区块的所有 Y 层共享同一个 2D 生物群系阵列。

### 3.2 新版 (Data3D, key=43)

- 512 字节高度图
- 之后是连续的 3D 生物群系子区块（每 16 层一个）

每个 3D 生物群系子区块（16×16×16 个点）的编码方式与子区块方块存储格式相同：

- 1 字节头部（位打包格式）
- 位打包的索引数组
- 调色板（4 字节整型列表，每个代表一个生物群系 ID）

块的排列顺序：`index = x*256 + z*16 + y`

### 3.3 生物群系列表（部分）

`data_3d.h` 定义了约 100 种生物群系枚举，包括：

- 基础群系：`ocean(0)`、`plains(1)`、`desert(2)`、`forest(4)` 等
- 变种：`sunflower_plains(129)`、`flower_forest(132)` 等
- 特殊值：`none(0xff)` 表示无数据

## 4. 方块颜色 (`color`)

方块颜色系统通过两个外部文件初始化：

- **`biome_colors.json`**：生物群系调色板，每个生物群系对应一个颜色
- **`block_colors.json`**：方块调色板，每个方块名称对应一个颜色

颜色混合：通过 `blend_color_with_biome()` 将方块颜色与所在生物群系的颜色混合，实现生物群系着色效果。

颜色格式（RGBA）：

```cpp
struct color {
    uint8_t r, g, b, a;
};
```

## 5. 方块实体 (Block Entities)

存储在 `BlockEntity` 键（49）中，是该区块内所有方块实体的列表。

格式为**连续的 NBT Compound 标签**，使用 `read_palette_to_end()` 从原始字节流中逐个解析。

常见的方块实体：

- 箱子（Chest）、漏斗（Hopper）、熔炉（Furnace）
- 命令方块（Command Block）
- 告示牌（Sign）
- 刷怪笼（Mob Spawner）

## 6. 实体 (Entities / Actors)

Bedrock 版中实体（Actor）的存储方式发生过一次重大变更。目前新版世界采用"摘要 + 独立存储"的方式，但旧版仍然使用区块内联存储。加载代码会同时尝试两种方式。

### 6.1 关键数据结构

```cpp
class actor {
    int64_t uid_;                    // 唯一标识符 (UniqueID)
    std::string identifier_;         // 类型标识 (如 "minecraft:creeper")
    vec3 pos_;                       // 位置 (浮点数坐标)
    bl::nbt::compound_tag* root_; // 完整 NBT 根标签
};
```

`actor::preload()` 从 NBT 根标签中解析三个必需字段，缺少任何一个则加载失败：

- **`Pos`**：位置，`[float, float, float]` 列表
- **`identifier`**：实体类型字符串
- **`UniqueID`**：8 字节长整型唯一标识符

### 6.2 旧版：Entity 键内联存储 (key=50)

储存在区块的 `Entity` 键（值 50, `'2'`）中。

```
LevelDB Key:   [4B X] [4B Z] [0x32] [1B dim]
LevelDB Value: 连续排列的 NBT Compound 标签
```

加载过程 (`chunk::load_entities` → 第一步)：

1. 构造 `Entity` 区块键
2. 读取原始值
3. 调用 `read_palette_to_end()` 将连续 NBT 解析为 `compound_tag` 列表
4. 对每个 NBT 调用 `actor::load_from_nbt(nbt)` 构造 actor 对象
5. **用完即释放 NBT**（`delete a`），actor 内部持有 copy

这种方式的缺点是：所有实体的数据直接嵌入在区块键中，即使实体跨区块移动也需要更新区块数据。

### 6.3 新版：Actor Digest + ActorPrefix 分离存储

新版将实体数据从区块中分离出来，采用两级索引结构。

#### 第一级：Actor Digest 摘要键

```
LevelDB Key:   "digp" + [4B X] + [4B Z] [+ 4B dim (仅非零维度时)]
               前缀 4B    坐标 8B    可选的维度 4B
LevelDB Value: [8B UID_1] [8B UID_2] ... (每8字节一个 UID)
```

摘要键值的总长度应为 8 的倍数，每个 8 字节段是一个实体的 `UniqueID`（小端序）。

```cpp
// 如果维度为 0，总键长 = 4 + 8 = 12 字节
// 如果维度不为 0，总键长 = 4 + 12 = 16 字节
```

#### 第二级：ActorPrefix 独立键

```
LevelDB Key:   "actorprefix" + [8B UID]
               前缀 11B         UID 8B
               总长度 = 19 字节
LevelDB Value: 单个 NBT Compound 标签
```

加载过程 (`chunk::load_entities` → 第二步)：

1. 构造 `actor_digest_key`（`digp` + 区块坐标）
2. 读取摘要值，解析出该区块所有实体的 UID 列表
3. 对每个 UID，构造 `actorprefix` + UID 键
4. 读取独立存储的 NBT 数据
5. 调用 `actor::load()` 解析 NBT 构造 actor 对象

### 6.4 加载流程（两种方式共存）

`chunk::load_entities()` 的实现同时兼容两种格式：

```
load_entities():
  ├── 步骤 1: 尝试旧版 Entity 键 (key=50)
  │   ├── 构造 Entity 区块键
  │   ├── 读取键值，解析 NBT 列表
  │   ├── 对每个 NBT → actor::load_from_nbt()
  │   └── 释放 NBT
  │
  └── 步骤 2: 尝试新版 Digest 键
      ├── 构造 actor_digest_key ("digp" + 坐标)
      ├── 读取摘要值（8 字节对齐的 UID 列表）
      ├── 对每个 UID:
      │     ├── 构造 "actorprefix" + UID 键 (19 字节)
      │     ├── 读取独立 NBT 数据
      │     └── actor::load()
      └── Actor 合并到区块的 entities_ 列表
```

**如果两种方式都包含有效数据，实体会被合并**（不会去重）。

### 6.5 两种存储方案的对比

| 特性           | 旧版（Entity 键）    | 新版（Digest + Prefix）             |
| -------------- | -------------------- | ----------------------------------- |
| 键格式         | 标准区块键 (type=50) | `digp` + 坐标 / `actorprefix` + UID |
| 数据位置       | 直接嵌在区块键中     | 独立于区块，按 UID 寻址             |
| NBT 解析       | 连续多个 Compound    | 每个键单个 Compound                 |
| 实体跨区块移动 | 需要重写区块键       | 只需更新摘要键                      |
| UID 作用域     | 无独立索引           | 全局唯一，可独立查找                |
| 兼容性         | 旧世界               | 新世界 1.18+                        |

### 6.6 Entity 键的别名：`actor_digest_key` 的遗留

注意区块键枚举中 `Entity = 50` 在注释中标注为"no longer used"。代码头部注释中曾提到：

```
/* 实体摘要信息
 * key - "digp" + chunk_pos.to_raw()
 * value = key*
 * key = "actorprefix" + uid
 */
```

这个注释揭示了演变过程：旧版直接用 `Entity` 键嵌实体，新版改为摘要 + 独立存储。

## 7. level.dat

文件结构：

```
[8字节: 文件头部（魔数/版本）] [NBT数据]
```

解析过程：

1. 读取前 8 字节作为头部
2. 从第 9 字节开始解析 NBT Compound 标签

预加载的关键字段：

- `LevelName` (string)：世界名称
- `SpawnX`、`SpawnY`、`SpawnZ` (int)：出生点坐标
- `StorageVersion` (int)：存储版本号
- `GameType`、`DayTime`、`Time`、`Difficulty`、`Seed` 等

## 8. 全局数据

从 LevelDB 中遍历所有非区块/非实体/非村民的键值对，分类：

### 玩家数据 (`player_data`)

- 键包含 `"player"` 的条目
- 存储为 `general_kv_nbts`（键 → NBT Compound 标签的映射）

### 地图物品 (`map_item_data`)

- 键以 `"map"` 开头的条目
- 存储地图物品的 NBT 数据（地图像素、中心坐标等）

### 村民/村庄数据 (`village_data`)

- 通过 `village_key` 解析的条目
- 按维度存储（4 个维度，每个维度一个哈希表）
- 每个村庄由 4 个 NBT 标签组成：`info`、`dwellers`、`players`、`poi`

### 其他数据 (`other_data`)

- 不属于以上分类的全局键值对

## 9. LevelDB 配置

在 `bedrock_level` 构造函数中设置的 LevelDB 选项：

| 参数              | 值          | 说明             |
| ----------------- | ----------- | ---------------- |
| Bloom Filter 策略 | 10 bits/key | 快速键查找       |
| 块缓存 (LRU)      | 20 MB       | 数据块缓存       |
| 写缓冲区大小      | 4 MB        | SST 文件写缓冲   |
| 块大小            | ~160 KB     | 数据块大小       |
| 压缩器 0          | Zlib Raw    | LevelDB 数据压缩 |
| 压缩器 1          | Zlib        | LevelDB 数据压缩 |

## 10. 硬编码生成区域 (HSA)

存储在 `HardCodedSpawnAreas` 键（57）中。

二进制格式：

```
[4字节: 区域数量 N]
重复 N 次：
  [4字节: min_x] [4字节: min_y] [4字节: min_z]
  [4字节: max_x] [4字节: max_y] [4字节: max_z]
  [1字节: 类型]
```

HSA 类型：
| 值 | 类型 |
|----|------------------|
| 1 | 下界要塞 |
| 2 | 沼泽小屋 |
| 3 | 海底神殿 |
| 5 | 掠夺者前哨站 |

## 数据流总结

```
level.dat ──→ level_dat (元数据)
db/LevelDB ──→ bedrock_level
  ├── 区块键 ──→ chunk
  │   ├── SubChunkTerrain ──→ sub_chunk ──→ layer + palette (方块)
  │   ├── Data3D/Data2D    ──→ biome3d (生物群系 + 高度图)
  │   ├── Entity/digest    ──→ actor (实体)
  │   ├── BlockEntity       ──→ compound_tag (方块实体)
  │   ├── PendingTicks      ──→ compound_tag (计划刻)
  │   └── HardCodedSpawnAreas ──→ HSA (结构区域)
  ├── actorprefix + UID   ──→ actor (新版本实体)
  └── 非区块键 ──→ 全局数据
       ├── player*          ──→ 玩家 NBT
       ├── map_*            ──→ 地图物品 NBT
       ├── Villages*        ──→ 村庄 NBT
       └── 其他             ──→ 其他键值 NBT
```
