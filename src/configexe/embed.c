#include "embed.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(WIN32) || defined(_WIN32)
# define QC_OS_WIN
# include <windows.h>
#endif

// this hash stuff is total overkill but i was bored

static const char *BLOCKSIG_ID = "QCONF_CONFIGWIN_BLOCKSIG";

static unsigned int calc_hash(const char *in, int len)
{
	unsigned int x;
	int n;

	x = 0;
	for(n = 0; n < len; ++n)
	{
		x <<= 4;
		x ^= (unsigned int)in[n];
	}
	return x;
}

// result should be "QCONF_CONFIGWIN_BLOCKSIG_68b7e7d7"
//
// the basic idea here is that we use this string as a marker for our
//   appended data section, but we don't want to include this actual
//   string in our code otherwise it could be misinterpretted as the
//   marker
static char *blocksig()
{
	char *blocksig_data;
	int idlen;
	unsigned int hash;
	char hashstr[9];
	int n;

	idlen = strlen(BLOCKSIG_ID);
	hash = calc_hash(BLOCKSIG_ID, idlen);
	blocksig_data = (char *)malloc(idlen + 1 + 8 + 1);
	memcpy(blocksig_data, BLOCKSIG_ID, idlen);
	blocksig_data[idlen] = '_';
	sprintf(hashstr, "%08x", hash);
	for(n = 0; n < 8; ++n)
		blocksig_data[idlen + 1 + n] = hashstr[n];
	blocksig_data[idlen + 1 + 8] = 0;

	return blocksig_data;
}

static char *app_file_path(const char *argv0)
{
#ifdef QC_OS_WIN
	char module_name[MAX_PATH+1];
	(void)argv0;
	GetModuleFileNameA(0, module_name, MAX_PATH);
	module_name[MAX_PATH] = 0;
	return strdup(module_name);
#else
	return strdup(argv0);
#endif
}

static int find_partial(const char *in, int in_size, const char *sub, int sub_size)
{
	int n;
	int size;

	for(n = 0; n < in_size; ++n)
	{
		if(sub_size < in_size - n)
			size = sub_size;
		else
			size = in_size - n;
		if(memcmp(in + n, sub, size) == 0)
			return n;
	}

	return -1;
}

static int seek_string(FILE *fp, const char *str)
{
	char block[8192];
	int str_at;
	int str_len;
	int size;
	int ret;
	int pos;

	str_len = strlen(str);
	str_at = 0;
	pos = ftell(fp);
	while(!feof(fp))
	{
		size = fread(block, 1, 8192, fp);
		if(size < 1)
			break;

		ret = find_partial(block, size, str + str_at, str_len - str_at);
		if(ret != -1)
		{
			if(str_at + (size - ret) >= str_len)
			{
				if(fseek(fp, pos + ret - str_at + str_len, SEEK_SET) != 0)
					return 0;

				return 1;
			}
			else
				str_at += (size - ret);
		}

		pos += size;
	}

	return 0;
}

unsigned int read32(const unsigned char *in)
{
	unsigned int out = in[0];
	out <<= 8;
	out += in[1];
	out <<= 8;
	out += in[2];
	out <<= 8;
	out += in[3];
	return out;
}

// format is:
//    <blocksig> <uint32 size> <data....>

static int import_data(const char *argv0, const char *sig, unsigned char **ret_data, unsigned int *ret_size)
{
	char *fname;
	FILE *fp;
	int ret;
	unsigned char buf[4];
	unsigned int size;
	unsigned char *data;

	fname = app_file_path(argv0);
	if(!fname)
		return 0;
	fp = fopen(fname, "rb");
	if(!fp)
		return 0;
	if(!seek_string(fp, sig))
		return 0;
	ret = fread(buf, 4, 1, fp);
	if(ret < 1)
		return 0;
	size = read32(buf);
	data = (unsigned char *)malloc(size);
	if(!data)
		return 0;
	ret = fread(data, size, 1, fp);
	if(ret < 1)
	{
		free(data);
		return 0;
	}
	fclose(fp);

	*ret_data = data;
	*ret_size = size;
	return 1;
}

int embed_get_data(const char *argv0, unsigned char **ret_data, unsigned int *ret_size)
{
	char *sig;
	int ret;

	sig = blocksig();
	//printf("%s\n", sig);
	ret = import_data(argv0, sig, ret_data, ret_size);
	free(sig);

	return ret;
}
