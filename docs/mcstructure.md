[int]: https://i.imgur.com/aOoHjFc.png
[list]: https://i.imgur.com/quP0K47.png
[compound]: https://i.imgur.com/GXsHWoh.png
[string]: https://i.imgur.com/Jdy0o6A.png
## Bedrock `.mcstructure` files

### Saving and Loading
`mcstructure` files are created by the **Export** button in a structure block. To load them in game with a load structure block, the files must be placed in a behavior pack. The path determines the structure identifier, which is typed into the structure block to load the structure.

**Examples:**  
`myBP/structures/house.mcstructure` → `mystructure:house`  
`myBP/structures/dungeon/entrance.mcstructure` → `dungeon:entrance`  
`myBP/structures/stuff/towers/diamond.mcstructure` → `stuff:towers/diamond`

The first subfolder defines the namespace, and subsequent folders define the path, ending with the structure file's name. This is different from Java's file structure, where the `structures` folder is inside the namespace folder itself.

Any files directly in the `structures` folder are given the `mystructure` namespace. If a structure exists in the `structures` folder and shares a name with a structure in an explicit `mystructure` folder, the game produces the following content log warning:
```
[Structure][warning]-There was a conflict loading a structure in the default namespace. A structure with name <name> was found in both in the root directory and the mystructure directory.
```
In this case, the file in the `mystructure` folder takes priority, resulting in the file directly in the `structures` folder being ignored.

Aside from behavior packs, structures can also be embedded directly in a world. The "Save" button in a structure block and the `/structure save` command both do this. Embedded structures take priority when loading, resulting in any identically-named structure in a behavior pack being ignored. Embedded structures can be removed with `/structure delete`.

### File Format
`mcstructure` files are [NBT files](https://minecraft.wiki/w/NBT_format#Binary_format). Like all Bedrock Edition NBT files, they are stored in little-endian format. [They are also uncompressed, making them very large.](https://bugs.mojang.com/browse/MCPE-105278) The tag structure is as follows:

> ![Integer][int] `format_version`: Currently always set to `1`.  
> ![List][list] `size`: List of three integers describing the size of the structure's bounds, measured in blocks.
> > ![Integer][int] Size of the structure in the X direction (west/east).  
> > ![Integer][int] Size of the structure in the Y direction (down/up).  
> > ![Integer][int] Size of the structure in the Z direction (north/south).
>
> ![Compound][compound] `structure`: Contains block and entity information.
> > ![List][list] `block_indices`: List containing two sublists, one for each layer. These contain the blocks in the structure. Each block is stored as an integer index into the palette (see below). Entries proceed in ZYX order from the lowest corner to the highest one. To convert from an index to relative block coordinates: `(i/SZ/SY, i/SZ%SY, i%SZ)`. To convert from coordinates to index: `SZ*SY*X + SZ*Y + Z`. Index values equal to `-1` indicate no block, causing any existing block to remain upon loading. This occurs when structure voids are saved, and is the case for most blocks in the second layer. Both layers share the same palette.
> > > ![List][list] of ![Integer][int] Indices for blocks in the primary layer.  
> > > ![List][list] of ![Integer][int] Indices for blocks in the secondary layer. This layer is usually empty, except for water when the block here is waterlogged.
> >
> > ![List][list] of ![Compound][compound] `entities`: List of entities as NBT, stored exactly the same as they appear in the world database. Tags like `Pos` and `UniqueID` are saved, but replaced upon loading.
> >
> > ![Compound][compound] `palette`: Contains multiple named palettes, presumably to support multiple variants of the same structure. However, currently only `default` is saved and loaded.
> > > ![Compound][compound] A single palette (currently only named `default`).
> > > > ![List][list] `block_palette`: List of block states. This list contains the ordered entries that the block indices are referring to.
> > > > > ![Compound][compound] A single block state.
> > > > > > ![String][string] `name`: The block's identifier, such as `minecraft:planks`.  
> > > > > > ![Compound][compound] `states`: The block's state properties as keys and values. Examples: `wood_type:"acacia"`, `bite_counter:3`, `open_bit:1b`. The values are the appropriate NBT type for the state: strings for enum values, integers for scalar numbers, and bytes for boolean values.  
> > > > > > ![Integer][int] `version`: Compatibility versioning number for this block. The four bytes making up this integer determine the game version number. For example, `17879555` is hex `01 10 D2 03`, meaning 1.16.210.03.  
> > > >
> > > > ![Compound][compound] `block_position_data`: Contains additional data for individual blocks in the structure. Each key is an integer index into the flattened list of blocks inside of `block_indices`, and can be converted to relative block coordinates the same way.
> > > > > ![Compound][compound] `<index>`: A single piece of additional block data, relevant to the block at its index position.
> > > > > > ![Compound][compound] `block_entity_data`: Block entity data as NBT, stored exactly the same as they appear in the world database. Position tags (`x`, `y`, `z`) are saved, but replaced upon loading. Layer is unspecified, as multiple block entities cannot coexist in a block space.
> > > > > >
> > > > > > ![List][list] `tick_queue_data`: Contains one more compounds of scheduled tick information. This is used for blocks like coral to make it die, water to make it flow, and other various scheduled updates.
> > > > > > > ![Compound][compound] A single pending tick.
> > > > > > > > ![Integer][int] `tick_delay`: The amount of ticks remaining until this block should be updated. No other values seem to exist adjacent to this one at this time.
>
> ![List][list] `structure_world_origin`: List of three integers describing where in the world the structure was originally saved. Equal to the position of the saving structure block, plus its offset settings. This is used to determine where entities should be placed when loading. An entity's new absolute position is equal to its old position, minus these values, plus the origin of the structure's loading position.
> > ![Integer][int] Structure origin X position.  
> > ![Integer][int] Structure origin Y position.  
> > ![Integer][int] Structure origin Z position.

### What Happens If...
Results from testing to see what happens when modified structure files are loaded:

* If the dimensions in `size` (and block layer lists) exceed the vanilla save limit of `64*256*64`, the structure can still be loaded just as expected.
* If the values in a block layer list are not int tags, all values are treated as `0`.
* If a value in a block layer list is equal to or larger than the palette size, or less than `-1`, an air block is placed.
* If the `default` palette is not present, loading the structure results in no blocks being placed.
* If any of the tags that have constant names are unspecified or are the wrong tag type, the structure fails to load with the following content log error:
```
[Structure][error]-Loading structure '<identifier>' from behavior pack: '<path>' | "<tag>" field, a required field, is missing from the structure.
```
* If `block_indices` does not contain exactly two values, the structure fails to load with the following content log error:
```
[Structure][error]-Loading structure '<identifier>' from behavior pack: '<path>' | The "block_indices" field should be an array with 2 arrays and instead we have <count> arrays.
```
* If the values inside of `block_indices` are not list tags, the structure fails to load with the following content log error:
```
[Structure][error]-Loading structure '<identifier>' from behavior pack: '<path>' | The "block_indices" field's first array is either missing or not a list.
```
* If the length of the two lists in `block_indices` are not equal, the structure fails to load with the following content log error:
```
[Structure][error]-Loading structure '<identifier>' from behavior pack: '<path>' | The "block_indices" field's arrays need to both be the same size.
```
* If the length of the two lists in `block_indices` does not equal the product of the structure's dimensions, the structure fails to load with the following content log error:
```
[Structure][error]-Loading structure '<identifier>' from behavior pack: '<path>' | The "block_indices" field should have as many elements as defined by the "size" field.
```