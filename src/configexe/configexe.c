/*
Copyright (C) 2009  Justin Karneges

This file is free software; unlimited permission is given to copy and/or
distribute it, with or without modifications, as long as this notice is
preserved.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "embed.h"

#if defined(WIN32) || defined(_WIN32)
# define QC_OS_WIN
#endif

#ifdef QC_OS_WIN
#include <direct.h>
#endif

#ifdef QC_OS_WIN
static char *qconftemp_path = "qconftemp";
#else
static char *qconftemp_path = ".qconftemp";
#endif

static int qc_verbose = 0;
static char *ex_qtdir = 0;

enum ArgType
{
	ArgValue,
	ArgFlag
};

typedef struct qcarg
{
	char *name;
	char *envvar;
	int type;
	char *val; // we populate this later based on actual passed args
} qcarg_t;

typedef struct qcfile
{
	char *name;
	unsigned char *data;
	unsigned int size;
} qcfile_t;

typedef struct qcdata
{
	char *usage;

	qcarg_t *args;
	int args_count;

	qcfile_t *files;
	int files_count;

	char *pro_name;
	char *pro_file;

	char *qtinfo;
} qcdata_t;

static char *alloc_str(const unsigned char *src, int len)
{
	char *out;
	out = (char *)malloc(len + 1);
	memcpy(out, src, len);
	out[len] = 0;
	return out;
}

static int index_of(const char *in, char sub)
{
	int n;
	for(n = 0; in[n]; ++n)
	{
		if(in[n] == sub)
			return n;
	}
	return -1;
}

static int index_of_str(const char *in, const char *sub)
{
	char *p;
	p = strstr(in, sub);
	if(p)
		return p - in;
	else
		return -1;
}

static char *selection_insert(const char *in, int at, int len, const char *sub)
{
	int ilen;
	int slen;
	int newsize;
	char *out;

	ilen = strlen(in);
	slen = strlen(sub);
	newsize = ilen - len + slen;
	out = (char *)malloc(newsize + 1);
	memcpy(out, in, at);
	memcpy(out + at, sub, slen);
	memcpy(out + at + slen, in + at + len, ilen - at - len);
	out[newsize] = 0;

	return out;
}

static char *find_replace(const char *in, const char *old, const char *newv)
{
	int at;
	int olen;

	olen = strlen(old);
	at = index_of_str(in, old);
	if(at != -1)
		return selection_insert(in, at, olen, newv);
	else
		return strdup(in);
}

static char *append_str(const char *in, const char *add)
{
	return selection_insert(in, strlen(in), 0, add);
}

// creates new string and frees old
static char *append_free(char *in, const char *add)
{
	char *out;
	out = append_str(in, add);
	free(in);
	return out;
}

// FIXME: handle bad data, don't malloc 0, etc
static qcdata_t *parse_data(unsigned char *data, unsigned int size)
{
	unsigned char *p;
	qcdata_t *q;
	unsigned int len;
	int n;

	(void)size;

	q = (qcdata_t *)malloc(sizeof(qcdata_t));
	p = data;
	len = read32(p);
	p += 4;
	q->usage = alloc_str(p, len);
	p += len;

	q->args_count = read32(p);
	p += 4;
	q->args = (qcarg_t *)malloc(sizeof(qcarg_t) * q->args_count);
	for(n = 0; n < q->args_count; ++n)
	{
		len = read32(p);
		p += 4;
		q->args[n].name = alloc_str(p, len);
		p += len;

		len = read32(p);
		p += 4;
		q->args[n].envvar = alloc_str(p, len);
		p += len;

		q->args[n].type = *(p++);

		q->args[n].val = NULL;
	}

	q->files_count = read32(p);
	p += 4;
	q->files = (qcfile_t *)malloc(sizeof(qcfile_t) * q->files_count);
	for(n = 0; n < q->files_count; ++n)
	{
		len = read32(p);
		p += 4;
		q->files[n].name = alloc_str(p, len);
		p += len;

		len = read32(p);
		p += 4;
		q->files[n].data = (unsigned char *)malloc(len);
		memcpy(q->files[n].data, p, len);
		q->files[n].size = len;
		p += len;
	}

	len = read32(p);
	p += 4;
	q->pro_name = alloc_str(p, len);
	p += len;

	len = read32(p);
	p += 4;
	q->pro_file = alloc_str(p, len);
	p += len;

	len = read32(p);
	p += 4;
	q->qtinfo = alloc_str(p, len);
	p += len;

	return q;
}

static void qcdata_delete(qcdata_t *q)
{
	int n;

	if(q->usage)
		free(q->usage);

	for(n = 0; n < q->args_count; ++n)
	{
		free(q->args[n].name);
		free(q->args[n].envvar);
		if(q->args[n].val)
			free(q->args[n].val);
	}

	for(n = 0; n < q->files_count; ++n)
	{
		free(q->files[n].name);
		free(q->files[n].data);
	}

	if(q->pro_name)
		free(q->pro_name);
	if(q->pro_file)
		free(q->pro_file);

	free(q);
}

static int find_arg(const qcarg_t *args, int count, const char *name)
{
	int n;
	for(n = 0; n < count; ++n)
	{
		if(strcmp(args[n].name, name) == 0)
			return n;
	}
	return -1;
}

static int find_file(const qcfile_t *files, int count, const char *name)
{
	int n;
	for(n = 0; n < count; ++n)
	{
		if(strcmp(files[n].name, name) == 0)
			return n;
	}
	return -1;
}

// adapted from qt
static int set_envvar(const char *var, const char *val)
{
#if defined(_MSC_VER) && _MSC_VER >= 1400
	return (_putenv_s(var, val) == 0 ? 1 : 0);
#else
	char *str;
	str = malloc(strlen(var) + 1 + strlen(val) + 1);
	strcpy(str, var);
	strcat(str, "=");
	strcat(str, val);
	return (putenv(str) == 0 ? 1 : 0);
#endif
}

static char *get_envvar(const char *var)
{
	return getenv(var);
}

static char *separators_to_native(const char *in)
{
	char *out;

#ifdef QC_OS_WIN
	int len;
	int n;

	out = strdup(in);
	len = strlen(in);
	for(n = 0; n < len; ++n)
	{
		if(out[n] == '/')
			out[n] = '\\';
	}
#else
	out = strdup(in);
#endif

	return out;
}

static int file_exists(const char *path)
{
	char *npath;
	int ret;
#ifdef QC_OS_WIN
	struct _stat buf;
#else
	struct stat buf;
#endif

	npath = separators_to_native(path);

#ifdef QC_OS_WIN
	ret = _stat(npath, &buf);
#else
	ret = stat(npath, &buf);
#endif

	free(npath);
	if(ret == 0)
		return 1;
	else
		return 0;
}

static char *check_qmake_path(const char *qtdir)
{
	char *str;

	str = strdup(qtdir);
#ifdef QC_OS_WIN
	str = append_free(str, "/bin/qmake.exe");
#else
	str = append_free(str, "/bin/qmake");
#endif
	if(file_exists(str))
	{
		return str;
	}
	else
	{
		free(str);
		return NULL;
	}
}

static char *find_qmake()
{
	char *qtdir;
	char *path;

	qtdir = ex_qtdir;
	if(qtdir)
	{
		path = check_qmake_path(qtdir);
		if(path)
			return path;
		if(qc_verbose)
			printf("Warning: qmake not found via --qtdir\n");
	}

	qtdir = get_envvar("QTDIR");
	if(qtdir)
	{
		path = check_qmake_path(qtdir);
		if(path)
			return path;
		if(qc_verbose)
			printf("Warning: qmake not found via %%QTDIR%%\n");
	}

	return NULL;
}

static int run_silent_stdout(const char *cmd)
{
	char *str;
	int ret;

	str = strdup(cmd);
#ifdef QC_OS_WIN
	str = append_free(str, " >NUL");
#else
	str = append_free(str, " >/dev/null");
#endif
	ret = system(str);
	free(str);

	return ret;
}

static int run_silent_all(const char *cmd)
{
	char *str;
	int ret;

	str = strdup(cmd);
#ifdef QC_OS_WIN
	str = append_free(str, " >NUL 2>&1");
#else
	str = append_free(str, " >/dev/null 2>&1");
#endif
	ret = system(str);
	free(str);

	return ret;
}

static int run_conflog_all(const char *cmd)
{
	char *str;
	int ret;

	str = strdup(cmd);
#ifdef QC_OS_WIN
	str = append_free(str, " >..\\conf.log 2>&1");
#else
	str = append_free(str, " >../conf.log 2>&1");
#endif
	ret = system(str);
	free(str);

	return ret;
}

static int qc_ensuredir(const char *path)
{
#ifdef QC_OS_WIN
	if(_mkdir(path) == 0)
		return 1;
	else if(errno == EEXIST)
		return 1;
	else
		return 0;
#else
	if(mkdir(path, S_IRWXU | S_IRWXG) == 0)
		return 1;
	else if(errno == EEXIST)
		return 1;
	else
		return 0;
#endif
}

static int qc_chdir(const char *path)
{
	int ret;

#ifdef QC_OS_WIN
	ret = _chdir(path);
#else
	ret = chdir(path);
#endif

	if(ret == 0)
		return 1;
	else
		return 0;
}

static int qc_removedir(const char *path)
{
	char *str;
	int ret;

#ifdef QC_OS_WIN
	str = strdup("deltree /y ");
	str = append_free(str, qconftemp_path);
	ret = run_silent_all(str);
	free(str);
	if(ret != 0)
	{
		str = strdup("rmdir /s /q ");
		str = append_free(str, qconftemp_path);
		ret = run_silent_all(str);
		free(str);
	}
#else
	str = strdup("rm -rf ");
	str = append_free(str, path);
	ret = system(str);
	free(str);
#endif

	if(ret == 0)
		return 1;
	else
		return 0;
}

static int gen_file(qcdata_t *q, const char *name, const char *dest)
{
	int at;
	char *str, *npath;
	FILE *fp;

	at = find_file(q->files, q->files_count, name);
	if(at == -1)
		return 0;

	str = strdup(dest);
	str = append_free(str, "/");
	str = append_free(str, name);
	npath = separators_to_native(str);
	free(str);
	fp = fopen(npath, "wb");
	free(npath);
	if(!fp)
		return 0;
	/*if(*/fwrite(q->files[at].data, q->files[at].size, 1, fp);/* < 1)
	{
		fclose(fp);
		return 0;
	}*/
	fclose(fp);
	return 1;
}

static int gen_files(qcdata_t *q, const char *dest)
{
	if(!gen_file(q, "modules.cpp", dest))
		return 0;
	if(!gen_file(q, "modules_new.cpp", dest))
		return 0;
	if(!gen_file(q, "conf4.h", dest))
		return 0;
	if(!gen_file(q, "conf4.cpp", dest))
		return 0;
	if(!gen_file(q, "conf4.pro", dest))
		return 0;
	return 1;
}

static int try_make(const char *makecmd, char **maketool)
{
	char *str;
	int ret;

	str = strdup(makecmd);
	str = append_free(str, " clean");
	ret = run_silent_all(str);
	free(str);
	if(ret != 0)
		return 0;

	if(run_conflog_all(makecmd) != 0)
		return 0;

	*maketool = strdup(makecmd);
	return 1;
}

static char *maketool_list[] =
{
	"make",
	"mingw32-make",
	"nmake",
	NULL
};

static int do_conf_create(qcdata_t *q, const char *qmake_path, char **maketool)
{
	char *str;
	int n;
	int at;

	if(!qc_ensuredir(qconftemp_path))
		return 0;

	if(!gen_files(q, qconftemp_path))
		return 0;

	if(!qc_chdir(qconftemp_path))
		return 0;

	// TODO: support -spec once QC_MAKESPEC matters
	str = strdup(qmake_path);
	str = append_free(str, " conf4.pro");
	if(run_silent_stdout(str) != 0)
	{
		free(str);
		qc_chdir("..");
		return 0;
	}
	free(str);

	at = -1;
	for(n = 0; maketool_list[n]; ++n)
	{
		if(qc_verbose)
			printf("Trying \"%s\"\n", maketool_list[n]);
		if(try_make(maketool_list[n], maketool))
		{
			at = n;
			break;
		}
	}

	qc_chdir("..");
	if(at == -1)
		return 0;

	return 1;
}

static int do_conf_run()
{
	char *str, *npath;
	int ret;

	str = strdup(qconftemp_path);
	str = append_free(str, "/conf");
	npath = separators_to_native(str);
	free(str);
	ret = system(npath);
	free(npath);

	return ret;
}

static void cleanup_qconftemp()
{
	qc_removedir(qconftemp_path);
}

static void try_print_var(const char *var, const char *val)
{
	printf("%s=", var);
	if(val)
		printf("%s", val);
	printf("\n");
}

static int do_conf(qcdata_t *q, const char *argv0)
{
	char *qmake_path;
	char *maketool;
	int n;
	int ret;

	printf("Configuring %s ...\n", q->pro_name);

	if(qc_verbose)
	{
		printf("\n");
		for(n = 0; n < q->args_count; ++n)
			try_print_var(q->args[n].envvar, q->args[n].val);
	}

	printf("Verifying Qt 4 build environment ... ");
	fflush(stdout);

	if(qc_verbose)
		printf("\n");

	qmake_path = find_qmake();
	if(!qmake_path)
	{
		if(qc_verbose)
			printf(" -> fail\n");
		else
			printf("fail\n");
		printf("\n");
		printf("Reason: Unable to find the 'qmake' tool for Qt 4.\n");
		printf("\n");
		printf("%s", q->qtinfo);
		return 0;
	}
	if(qc_verbose)
		printf("qmake found in %s\n", qmake_path);

	// TODO: in verbose mode, print out default makespec and what we're
	//   overriding the makespec to (if any).  since at this time we don't
	//   ever override the makespec on windows, we don't need this yet.

	cleanup_qconftemp();
	maketool = NULL;
	if(!do_conf_create(q, qmake_path, &maketool))
	{
		cleanup_qconftemp();
		if(qc_verbose)
			printf(" -> fail\n");
		else
			printf("fail\n");
		printf("\n");
		printf("Reason: There was an error compiling 'conf'.  See conf.log for details.\n");
		printf("\n");
		printf("%s", q->qtinfo);

		if(qc_verbose)
		{
			printf("conf.log:\n");
			system("type conf.log");
		}

		free(qmake_path);
		return 0;
	}

	set_envvar("QC_COMMAND", argv0);
	set_envvar("QC_PROFILE", q->pro_file);
	set_envvar("QC_QMAKE", qmake_path);
	// TODO: unix configure will set QC_MAKESPEC here if it needs to
	//   override the mkspec.  currently, it only does this for macx-xcode
	//   so the behavior doesn't apply to windows yet.
	set_envvar("QC_MAKETOOL", maketool);

	free(qmake_path);

	ret = do_conf_run();
	if(ret == 1)
	{
		cleanup_qconftemp();
		printf("\n");
		free(maketool);
		return 0;
	}
	else if(ret != 0)
	{
		cleanup_qconftemp();
		if(qc_verbose)
			printf(" -> fail\n");
		else
			printf("fail\n");
		printf("\n");
		printf("Reason: Unexpected error launching 'conf'\n");
		printf("\n");
		free(maketool);
		return 0;
	}

	cleanup_qconftemp();
	printf("\n");
	printf("Good, your configure finished.  Now run %s.\n", maketool);
	printf("\n");
	free(maketool);

	return 1;
}

int main(int argc, char **argv)
{
	unsigned char *data;
	unsigned int size;
	qcdata_t *q;
	int n;
	int at;
	int quit;
	char *arg, *var, *val;

	if(!embed_get_data(argv[0], &data, &size))
	{
		fprintf(stderr, "Error: Can't import data.\n");
		return 1;
	}

	q = parse_data(data, size);
	if(!q)
	{
		fprintf(stderr, "Error: Can't parse internal data.\n");
		free(data);
		return 1;
	}

	if(q->usage)
	{
		val = find_replace(q->usage, "$0", argv[0]);
		free(q->usage);
		q->usage = val;
	}

	quit = 0;
	for(n = 1; n < argc && !quit; ++n)
	{
		arg = argv[n];

		if(arg[0] != '-' || arg[1] != '-')
		{
			printf("%s", q->usage);
			quit = 1;
			break;
		}

		at = index_of(arg + 2, '=');
		if(at != -1)
		{
			var = alloc_str((unsigned char *)arg + 2, at);
			val = strdup(arg + 2 + at + 1);
		}
		else
		{
			var = strdup(arg + 2);
			val = 0;
		}

		if(strcmp(var, "help") == 0)
		{
			printf("%s", q->usage);
			quit = 1;
		}
		else if(strcmp(var, "verbose") == 0)
		{
			qc_verbose = 1;
			set_envvar("QC_VERBOSE", "Y");
		}
		else
		{
			at = find_arg(q->args, q->args_count, var);
			if(at != -1)
			{
				// keep a stash of ex_qtdir
				if(strcmp(var, "qtdir") == 0)
				{
					if(ex_qtdir)
						free(ex_qtdir);
					ex_qtdir = strdup(val);
				}

				if(q->args[at].val)
					free(q->args[at].val);

				if(q->args[at].type == ArgValue)
					q->args[at].val = strdup(val);
				else // ArgFlag
					q->args[at].val = strdup("Y");

				set_envvar(q->args[at].envvar, q->args[at].val);
			}
			else
			{
				printf("%s", q->usage);
				quit = 1;
			}
		}

		free(var);
		if(val)
			free(val);
	}

	if(quit)
	{
		qcdata_delete(q);
		if(ex_qtdir)
			free(ex_qtdir);
		return 1;
	}

	n = do_conf(q, argv[0]);
	qcdata_delete(q);
	if(ex_qtdir)
		free(ex_qtdir);

	if(n)
		return 0;
	else
		return 1;
}
