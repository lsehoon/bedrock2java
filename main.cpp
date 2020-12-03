#include <iostream>
#include <sstream>
#include <string>
#include <arpa/inet.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "leveldb/db.h"
#include "leveldb/decompress_allocator.h"
#include "leveldb/zlib_compressor.h"

#include "block2.h"
#include "tblock.h"
#include "sblock.h"
#include "alias.h"
#include "btypes.h"

using namespace std;

void hexdump(const void *p, int len) {
	int i, j;
	unsigned char *data = (unsigned char *)p;

	for (i = 0; i < len; i+=16) {
		printf("%08x: ", i);
		for (j = 0; j < 16; j++) {
			if (i + j < len)
				printf("%02x ", data[i+j]);
			else
				printf("   ");
		}
		printf(" ");
		for (j = 0; j < 16; j++) {
			if (i + j < len)
				printf("%c", (data[i+j]>=32 && data[i+j]<=126) ? data[i+j] : '.');
		}
		printf("\n");
	}
	printf("\n");
}

unsigned char header[] = {
 0x0a,0x00 ,0x00,0x0a ,0x00,0x05 ,0x4c,0x65 ,0x76,0x65 ,0x6c,
 0x07, 0x00, 0x06, 'B', 'i', 'o', 'm', 'e', 's', 0x00, 0x00, 0x01, 0x00,

 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,


 0x01, 0x00, 0x10, 'T', 'e', 'r', 'r', 'a', 'i', 'n',
 'P', 'o', 'p',  'u', 'l', 'a', 't', 'e', 'd', 1,

 0x09, 0x00, 0x0c, 'T', 'i', 'l', 'e', 'E', 'n', 't', 'i', 't', 'i', 'e', 's',
 0x00, 0x00, 0x00, 0x00, 0x00,

 0x03 ,0x00,0x04 ,0x78,0x50
,0x6f,0x73 ,0x00,0x00 ,0x00,0x00 ,0x03,0x00 ,0x04,0x7a ,0x50,0x6f ,0x73,0x00 ,0x00,0x00
,0x09,0x09 ,0x00,0x08 ,0x53,0x65 ,0x63,0x74 ,0x69,0x6f ,0x6e,0x73 ,0x0a,0x00 ,0x00,0x00
,0x00,
};

//xpos: 0x12+269, zpos: 0x1d+269, cnt: 0x2d+269
//xpos: 0x3A+269, zpos: 0x45+269, cnt: 0x55+269

unsigned char header_blocks[] = {
 0x07 ,0x00,0x06 ,0x42,0x6c ,0x6f,0x63 ,0x6b,0x73 ,0x00,0x00 ,0x10,0x00
};

unsigned char header_data[] = {
 0x07,0x00 ,0x04,0x44 ,0x61,0x74 ,0x61,0x00 ,0x00,0x08 ,0x00
};

unsigned char header_skylight[] = {
 0x07,0x00 ,0x08,0x53 ,0x6b,0x79 ,0x4c,0x69 ,0x67,0x68 ,0x74,0x00 ,0x00,0x08 ,0x00
};

unsigned char header_blocklight[] = {
	0x07 ,0x00,0x0a ,0x42,0x6c ,0x6f,0x63 ,0x6b,0x4c ,0x69,0x67 ,0x68,0x74 ,0x00,0x00 ,0x08,0x00
};

unsigned char footer[] = {
 0x01 ,0x00,0x01 ,0x59,0x00,0x00
};
//ypos: 0x04

unsigned char ender[] = {
 0x00,0x00
};

unsigned char wbuf[1048576];
unsigned int wpos = 0;
unsigned char cbuf[1048576];
long unsigned int cpos = 0;

int _indexof(const char **blocknames, int count, unsigned char *name, int len) {
	for (int j = 0; j < count; j++) {
		if (len != strlen(blocknames[j]))
			continue;
		if (!memcmp(blocknames[j], name, len)) {
			return j;
		}
	}
	return -1;
}

#define indexof(a) _indexof(a, sizeof(a)/sizeof(*(a)), d, l)
#define nameis(a) (len == sizeof(a)-1 && !memcmp(a, name, len))

int extractchunk(const void *data, int size, int cx, int cy, int cz) {
	unsigned char *d = (unsigned char *)data;
	unsigned char btable[4096], dtable[4096];

	if (d[0] != 8)
		return -1; //unsupported version

	int count = d[1];
	int bpb = d[2];

	int bipw = bpb / 2;
	int blpw = 32 / bipw;
	int blen = (4095 + blpw) / blpw;

	unsigned int *n = (unsigned int *)(d + 3), mask = (1 << bipw) - 1;
	memset(btable, 0, sizeof(btable));
	memset(dtable, 0, sizeof(dtable));

	d += blen * 4 + 3;
	size -= blen * 4 + 3;
	int typecnt = *(unsigned int *)d;
	if (typecnt > mask + 1)
		return -2; //parse error
	d += 4; size -= 4;
	int i = -1, dtabled = 0;
	unsigned int l;

	for (; size > 0 && i < typecnt;) {
		int t = d[0];
//			printf("t:%d\n", t);
		d += 1; size -= 1;
		if (t == 0)
			continue;

		l = *(unsigned short *)d;
		d += 2; size -= 2;

		if (t == 8) {
			if (l == 4 && !memcmp("name", d, l)) {
				d += l; size -= l;
				l = *(unsigned short *)d;
				d += 2; size -= 2;

				int block = -1, data = 0, len = l;
				unsigned char *name = d;
//				fwrite(name, len, 1, stdout);
//				printf("\n");
				i++;
				dtabled = 0;

				for (int j = 0; j < sizeof(aliases)/sizeof(*aliases); j++) {
					if (l != strlen(aliases[j].bedname))
						continue;
					if (!memcmp(aliases[j].bedname, name, len)) {
						name = (unsigned char *)aliases[j].javaname;
						len = strlen((char *)name);
						data = aliases[j].data;
						dtabled = 1;
						break;
					}
				}

				for (int j = 0; j < sizeof(blocknames)/sizeof(char *); j++) {
					if (len != strlen(blocknames[j]))
						continue;
					if (!memcmp(blocknames[j], name, len)) {
						block = j;
						break;
					}
				}
				if (block == -1) {
					for (int j = 0; j < sizeof(sblocks)/sizeof(char *); j++) {
						if (len != strlen(sblocks[j]))
							continue;
						if (!memcmp(sblocks[j], name, len)) {
							block = 9;
							data = 0;
							break;
						}
					}

					if (block == -1) {
						fwrite(name, len, 1, stdout);
						fprintf(stdout, "\n");
						block = 0;
					}
				}
				btable[i] = block;
				dtable[i] = data;
				d += l; size -= l;
			}
			else {
				unsigned char *name = d;
				int len = l;

				d += l; size -= l;
				l = *(unsigned short *)d;
				d += 2; size -= 2;
				int data = 0;

				if (nameis("chisel_type"))
					data = indexof(chisel_types);
				else if (nameis("color"))
					data = indexof(colors);
				else if (nameis("double_plant_type"))
					data = indexof(double_plant_types);
				else if (nameis("flower_type"))
					data = indexof(flower_types);
				else if (nameis("new_leaf_type") || nameis("old_leaf_type") || nameis("sapling_type") || nameis("wood_type"))
					data = indexof(wood_types);
				else if (nameis("new_log_type")) {
					data = (data << 2) | indexof(new_log_types);
					dtabled = 0;
				}
				else if (nameis("old_log_type")) {
					data = (data << 2) | indexof(old_log_types);
					dtabled = 0;
				}
				else if (nameis("prismarine_block_type"))
					data = indexof(prismarine_block_types);
				else if (nameis("sand_stone_type"))
					data = indexof(sand_stone_types);
				else if (nameis("sand_type"))
					data = indexof(sand_types);
				else if (nameis("stone_brick_type"))
					data = indexof(stone_brick_types);
				else if (nameis("stone_slab_type"))
					data = indexof(stone_slab_types);
				else if (nameis("stone_slab_type_2"))
					data = indexof(stone_slab2_types);
				else if (nameis("stone_slab_type_3"))
					data = indexof(stone_slab3_types);
				else if (nameis("stone_slab_type_4"))
					data = indexof(stone_slab4_types);
				else if (nameis("stone_type"))
					data = indexof(stone_types);
				else if (nameis("torch_facing_direction"))
					data = indexof(torch_facing_directions);
				else if (nameis("tall_grass_type"))
					data = indexof(tall_grass_types);


				if (data >= 0) {
					if (!dtabled)
						dtable[i] = data;
					dtabled = 1;
				}
				else {
					fwrite(name, len, 1, stdout);
					printf(":");
					fwrite(d, l, 1, stdout);
					printf("\n");
				}

				d += l; size -= l;
			}
		}
		else if (t == 3) {
			if ((l >= 9 && !memcmp("direction", d + (l - 9), 9)) || (l == 6 && !memcmp("growth", d, l)) || (l == 19 && !memcmp("vine_direction_bits", d, l)) || (l == 12 && !memcmp("liquid_depth", d, l)) || (l == 15 && !memcmp("redstone_signal", d, l))) {
				if (!dtabled)
					dtable[i] = d[l];
				dtabled = 1;
			}
			d += l + 4; size -= l + 4;
		}
		else if (t == 2) {
			if (l == 3 && !memcmp("val", d, l)) {
				d += l; size -= l;
				if (!dtabled)
					dtable[i] = d[0];
				dtabled = 1;
				d += 3; size -= 3;
			}
		}
		else if (t == 1) {
			d += l + 1; size -= l + 1;
		}
		else  {
			d += l; size -= l;
		}
	}
	i++;
	if (i < typecnt) {
		printf("no block info %d %d %d\n", i, typecnt);
		return -33;
	}

	memcpy(wbuf + wpos, header_blocks, sizeof(header_blocks));
	wpos += sizeof(header_blocks);
	unsigned char *bbuf = wbuf + wpos;
	wpos += 4096;
	memcpy(wbuf + wpos, header_data, sizeof(header_data));
	wpos += sizeof(header_data);
	unsigned char *dbuf = wbuf + wpos;
	wpos += 2048;
	memcpy(wbuf + wpos, header_skylight, sizeof(header_skylight));
	wpos += sizeof(header_skylight);
	memset(wbuf + wpos, 0xff, 2048);
	wpos += 2048;
	memcpy(wbuf + wpos, header_blocklight, sizeof(header_blocklight));
	wpos += sizeof(header_blocklight);
	memset(wbuf + wpos, 0, 2048);
	wpos += 2048;
	memcpy(wbuf + wpos, footer, sizeof(footer));
	wbuf[wpos + 4] = cy;
	wpos += sizeof(footer);
	memset(dbuf, 0, 2048);

	for (int x = 0; x < 4096; x++) {
		unsigned int idx = (n[x / blpw] >> ((x % blpw) * bipw)) & mask;
		if (idx >= 4096)
			return -6; //out of bound

		int xx = ((x & 0xF) << 8) | (x >> 8) | (x & 0xF0);

		bbuf[xx] = btable[idx];
//		if (bbuf[xx] == 52 && cx > -0 && cx < 32 && cz > -16 && cz < 16)
//			printf("(%d,%d,%d)\n", (cx<<4)|(xx&0xF), (cy<<4)|((xx>>8)&0xF), (cz<<4)|((xx>>4)&0xF));
		if (!(xx & 1))
			dbuf[xx/2] |= dtable[idx];
		else
			dbuf[xx/2] |= (dtable[idx] << 4);

/*		printf("%3d ", btable[idx]);
		if ((x & 15) == 15)
			printf("\n");
		if ((x & 255) == 255)
			printf("\n");*/
	}

	return 0;
}

int writechunk(int x, int z) {
	if (x == 0x7fffffff || z == 0x7fffffff)
		return 0;
    int i, j;
    for (i = 0; i < wpos; i+=16) {
        for (j = 0; j < 16; j++) {
            if (i + j < wpos) {
                if (wbuf[i+j] == 0x73)
                if (wbuf[i+j+1] == 0x0A)
                if (wbuf[i+j+2] == 0x00)
                if (wbuf[i+j+3] == 0x00)
                if (wbuf[i+j+4] == 0x00)
                if (wbuf[i+j+5] == 0x00) {
                    if (x == -1 && z == -1) {
                        wbuf[i+j+5] = 0x01;
                        memcpy(wbuf + wpos, ender, sizeof(ender));
                        wpos += sizeof(ender);
                    }
                }
            }
        }
    }

	char fn[1024];
	sprintf(fn, "region/r.%d.%d.mca", x >> 5, z >> 5);

	unsigned int locations[1024], timestamps[1024];
	int offset = ((x & 31) + (z & 31) * 32);

	FILE *fp = fopen(fn, "rb+");
	if (fp == NULL) {
		memset(locations, 0, sizeof(locations));
		memset(timestamps, 0, sizeof(timestamps));
		fp = fopen(fn, "wb");
		if (fp == NULL)
			return -1;
		fwrite(locations, 4096, 1, fp);
		fwrite(timestamps, 4096, 1, fp);
		fclose(fp);
		fp = fopen(fn, "rb+");
		if (fp == NULL)
			return -2;

		printf("%s\n", fn);
	}
	fread(locations, 4096, 1, fp);
	fread(timestamps, 4096, 1, fp);

	if (locations[offset] == 0) {
		fseek(fp, 0, SEEK_END);
		locations[offset] = htonl(((ftell(fp) >> 4) & ~0xff) | 3);
		timestamps[offset] = htonl(time(0));
		fseek(fp, 0, SEEK_SET);
		fwrite(locations, 4096, 1, fp);
		fwrite(timestamps, 4096, 1, fp);

		fclose(fp);
		fp = fopen(fn, "ab");
		if (fp == NULL)
			return -3;
	}
	else {
		if ((htonl(locations[offset]) & 0xFF) != 3)
			return -4;

		timestamps[offset] = htonl(time(0));
		fseek(fp, 0, SEEK_SET);
		fwrite(locations, 4096, 1, fp);
		fwrite(timestamps, 4096, 1, fp);
		fseek(fp, (htonl(locations[offset]) & ~0xFF) << 4, SEEK_SET);
	}

	memset(cbuf, 0, sizeof(cbuf));
	cpos = sizeof(cbuf);
	compress(cbuf + 5, &cpos, wbuf, wpos);
	if (cpos > 12288)
		printf("%d\n",cpos);
	*(unsigned int *)cbuf = htonl(cpos + 1);
	cbuf[4] = 2;

	fwrite(cbuf, 12288, 1, fp);
	fclose(fp);

	return 0;
}

void updatelight(int cnt) {
	int ychunksize = sizeof(header_blocks) + 4096 + sizeof(header_data) + 2048 + sizeof(header_skylight) + 2048 + sizeof(header_blocklight) + 2048 + sizeof(footer);
	if (!cnt)
		return;

	for (int xz = 0; xz < 256; xz++) {
		int hide = 0;
		for (int y = (cnt << 4) - 1; y >= 0; y--) {
			unsigned char *b = wbuf + sizeof(header) + (y / 16) * ychunksize + sizeof(header_blocks);
			unsigned char *l = b + 4096 + sizeof(header_data) + 2048 + sizeof(header_skylight);
			int xx = xz | ((y & 0xF) << 8);
			if (!hide && !tblocks[b[xx]])
				hide = 1;
			if (hide) {
				if (!(xx & 1))
					l[xx/2] = (l[xx/2] & 0xF0) | 0x0d;
				else
					l[xx/2] = (l[xx/2] & 0x0F) | 0xd0;
			}
		}
	}
//	printf("cnt=%d, wpos=%d\n", cnt, wpos);
//	hexdump(wbuf,wpos);
//	exit(0);
}

int main(int argc, char** argv)
{
    // Set up database connection information and open database
    leveldb::DB* db;
    leveldb::Options options;
	char path[1024];

	if (argc < 2) {
		printf("bed2java (map directory path)\n");
		return -1;
	}
	if (strlen(argv[1]) > 1000) {
		printf("path too long\n");
		return -2;
	}
	sprintf(path, "%s/db", argv[1]);

	mkdir("region", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    options.create_if_missing = false;
	//options.filter_policy = leveldb::NewBloomFilterPolicy(10);
	//options.block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);
	options.write_buffer_size = 4 * 1024 * 1024;
	options.compressors[0] = new leveldb::ZlibCompressorRaw(-1);
	options.compressors[1] = new leveldb::ZlibCompressor();

	leveldb::ReadOptions readOptions;
	readOptions.decompress_allocator = new leveldb::DecompressAllocator();

    leveldb::Status status = leveldb::DB::Open(options, path, &db);

    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './testdb'" << endl;
        cerr << status.ToString() << endl;
        return -1;
    }

    // Iterate over each item in the database and print them
    leveldb::Iterator* it = db->NewIterator(readOptions);

	int lx = 0x7fffffff, lz = 0x7fffffff, ret = 0, cnt = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        const char *str = it->key().ToString().c_str();
        const unsigned char *data = (const unsigned char *)it->value().data();
		int x = *(int *)str, z = *(int *)(str + 4);
		if (it->key().size() > 10)
			continue;
		//printf("%d,%d,%d(%d - %d,%d) - %d\n", x, z, str[8], str[9], data[1],data[2], it->value().size());

		//if (str[8] == 47 && data[2] == 12)
		//	hexdump(it->value().data(), it->value().size());

		if (str[8] != 45 && str[8] != 47)
			continue;

		if (lx != x || lz != z) {
			updatelight(cnt);

			memcpy(wbuf + wpos, ender, sizeof(ender));
			wpos += sizeof(ender);
			*(unsigned int *)(wbuf + 0x55 + 269) = htonl(cnt);
			if (ret = writechunk(lx, lz)) {
				printf("writechunk:%d\n", ret);
				return -3;
			}
			memcpy(wbuf, header, sizeof(header));
			wpos = sizeof(header);
			cnt = 0;
			*(unsigned int *)(wbuf + 0x3A + 269) = htonl(x);
			*(unsigned int *)(wbuf + 0x45 + 269) = htonl(z);
		}
		if (str[8] == 45) {
			memcpy(wbuf + 24, data + 0x200, 256);
//			for (int i = 0; i < 256; i++) {
//				unsigned char c = data[0x200 + i];
//				if (c == 21 || c == 22 || c ==149 || c == 23 || c == 151) {
//					printf("x:%d z:%d - %d\n", x, z, c);   
//					break;
//				}
//			}
		}
		else if (str[8] == 47) {
			if (ret = extractchunk(data, it->value().size(), x, str[9], z)) {
				printf("extractchunk:%d - cx:%d cy:%d cz:%d\n", ret, x, str[9], z);
				hexdump(data, it->value().size());
				return -2;
			}
			cnt++;
		}

		lx = x;
		lz = z;
    }

	if (ret = writechunk(lx, lz)) {
		printf("writechunk:%d\n", ret);
		return -3;
	}

    if (false == it->status().ok())
    {
        cerr << "An error was found during the scan" << endl;
        cerr << it->status().ToString() << endl;
    }

    delete it;

    // Close the database
    delete db;
}

