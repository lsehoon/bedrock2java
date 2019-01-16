struct alias_t {
	const char *bedname;
	const char *javaname;
	int data;
} aliases[] = {
{"minecraft:beetroot",               "minecraft:carrots", -1},
{"minecraft:podzol",                 "minecraft:dirt", 2},
{"minecraft:blue_ice",               "minecraft:ice", 0},
{"minecraft:spruce_trapdoor",        "minecraft:trapdoor", 0},
{"minecraft:stripped_spruce_log",    "minecraft:log", 1},
{"minecraft:stripped_oak_log",       "minecraft:log", 0},
{"minecraft:carved_pumpkin",         "minecraft:pumpkin", 0},
{"minecraft:double_wooden_slab",     "minecraft:planks", 0},
{"minecraft:dark_oak_trapdoor",      "minecraft:trapdoor", 0},
{"minecraft:frame",                  "minecraft:air", 0},
{"minecraft:stripped_dark_oak_log",  "minecraft:log2", 1},
{"minecraft:dark_oak_trapdoor",      "minecraft:trapdoor", 0},
{"minecraft:dark_oak_button",        "minecraft:wooden_button", 0},
{"minecraft:birch_button",           "minecraft:wooden_button", 0},
{"minecraft:stripped_jungle_log",    "minecraft:log", 3},
{"minecraft:jungle_trapdoor",        "minecraft:trapdoor", 0},
};
