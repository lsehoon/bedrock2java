#include <iostream>
#include <sstream>
#include <string>
#include <arpa/inet.h>
#include <zlib.h>

#include "leveldb/db.h"
#include "leveldb/decompress_allocator.h"
#include "leveldb/zlib_compressor.h"

#include "block2.h"

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
 0x0a,0x00 ,0x00,0x0a ,0x00,0x05 ,0x4c,0x65 ,0x76,0x65 ,0x6c,0x03 ,0x00,0x04 ,0x78,0x50
,0x6f,0x73 ,0x00,0x00 ,0x00,0x00 ,0x03,0x00 ,0x04,0x7a ,0x50,0x6f ,0x73,0x00 ,0x00,0x00
,0x09,0x09 ,0x00,0x08 ,0x53,0x65 ,0x63,0x74 ,0x69,0x6f ,0x6e,0x73 ,0x0a,0x00 ,0x00,0x00
,0x00,
};

//xpos: 0x12, zpos: 0x1d, cnt: 0x2d

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

int extractchunk(const void *data, int size, int y) {
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
	unsigned int typecnt = *(unsigned int *)d;
	if (typecnt > mask + 1)
		return -2; //parse error
	d += 4; size -= 4;
	for (int i = 0; i < typecnt;) {
		int t = d[0];
		unsigned int l = *(unsigned short *)(d + 1);
		d += 3; size -= 3;

		if (t == 8) {
			if (l != 4 || memcmp("name", d, 4))
				return -3; //parse error 
			d += l; size -= l;

			l = *(unsigned short *)d;
			int block = -1;
			for (int j = 0; j < sizeof(blocknames)/sizeof(char *); j++) {
				if (l != strlen(blocknames[j]))
					continue;
				if (!memcmp(blocknames[j], d + 2, l)) {
					block = j;
					break;
				}
			}
			if (block == -1) {
//				fwrite(d + 2, l, 1, stdout);
//				fprintf(stdout, "\n");
				block = 0;
			}
				
			d += l + 2; size -= l + 2;

			if (d[0] != 2)
				return -4; //parse error

			l = *(unsigned short *)(d + 1);
			d += 3; size -= 3;
			if (l != 3 || memcmp("val", d, 3))
				return -5; //parse error 
			d += l; size -= l;

			btable[i] = block;
			dtable[i] = d[0];

			d += 3; size -= 3;
			i++;
		}
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
	memset(wbuf + wpos, 0xff, 2048);
	wpos += 2048;
	memcpy(wbuf + wpos, footer, sizeof(footer));
	wbuf[wpos + 4] = y;
	wpos += sizeof(footer);

	memset(dbuf, 0, 2048);

	for (int x = 0; x < 4096; x++) {
		unsigned int idx = (n[x / blpw] >> ((x % blpw) * bipw)) & mask;
		if (idx >= 4096)
			return -6; //out of bound

		int xx = ((x & 0xF) << 8) | (x >> 8) | (x & 0xF0);
		
		bbuf[xx] = btable[idx];
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
		
		if (str[8] != 47)
			continue;
		if (lx != x || lz != z) {
			memcpy(wbuf + wpos, ender, sizeof(ender));
			wpos += sizeof(ender);
			*(unsigned int *)(wbuf + 0x2d) = htonl(cnt);
			if (ret = writechunk(lx, lz)) {
				printf("writechunk:%d\n", ret);
				return -3;
			}
			memcpy(wbuf, header, sizeof(header));
			wpos = sizeof(header);
			cnt = 0;
			*(unsigned int *)(wbuf + 0x12) = htonl(x);
			*(unsigned int *)(wbuf + 0x1d) = htonl(z);
		}
		if (str[8] == 47) {
			if (ret = extractchunk(data, it->value().size(), str[9])) {
				printf("extractchunk:%d\n", ret);
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

