# bedrock2java
Minecraft Bedrock (Pocket) Edition to Java Edition Map Converter (for Overviewer)

BE version should be greater than 1.10

## Dependency
- [leveldb-mcpe](https://github.com/Mojang/leveldb-mcpe)
- Overviewer (0.12.280 works fine)

## Build
```
make
```

## How to Use
- Copy map files from Minecraft BE folder to `map/`
- Run `bed2java map` to convert the map. converted files will be saved to `region/`
- Run `overviewer.py --rendermodes=smooth-lighting --forcerender . /path/to/output` to build the map.

## Notes
Block IDs of Minecraft BE are different from Minecraft Java Edition. So some blocks may be missing or looks different.

[Sample overviewer page - https://mybz.net/output](https://mybz.net/output)
