/*
 * Get or set RPC debug flags.
 *
 * I would have loved to write this without recourse to the sysctl
 * interface, but the only plausible approach (reading and writing
 * /dev/kmem at the offsets indicated by the _debug symbols from
 * /proc/ksyms) didn't work, because /dev/kmem doesn't translate virtual
 * addresses on write. Unfortunately, modules are stuffed into memory
 * allocated via vmalloc.
 *
 * Copyright (C) 1996, 1997, Olaf Kirch <okir@monad.swb.de>
 */

#include "config.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <nfs/debug.h>

static int		verbose = 0;

static int		find_sysname(char *module);
static unsigned int	find_flag(char **module, char *name);
static unsigned int	get_flags(char *);
static unsigned int	set_flags(char *, unsigned int value);
static void		print_flags(FILE *, char *, unsigned int, int);
static char *		strtolower(char *str);
static void		usage(int excode);

int
main(int argc, char **argv)
{
	int		opt_s = 0,
			opt_c = 0;
	unsigned int	flags = 0, oflags;
	char *		module = NULL;
	int		c;

	while ((c = getopt(argc, argv, "chm:sv")) != EOF) {
		switch (c) {
		case 'c':
			opt_c = 1;
			break;
		case 'h':
			usage(0);
		case 'm':
			module = optarg;
			break;
		case 's':
			opt_s = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			fprintf(stderr, "rpcdebug: unknown option -%c\n",
						optopt);
			usage(1);
		}
	}

	if (opt_c + opt_s > 1) {
		fprintf(stderr, "You can use at most one of -c and -s\n");
		usage(1);
	}

	if (argc == optind) {
		flags = ~(unsigned int) 0;
	} else {
		for (; optind < argc; optind++)
			flags |= find_flag(&module, argv[optind]);
		if (flags && !opt_c)
			opt_s = 1;
	}

	if (!module) {
		fprintf(stderr, "rpcdebug: no module name specified, and "
				"could not be inferred.\n");
		usage(1);
	}

	oflags = get_flags(module);

	if (opt_c) {
		oflags = set_flags(module, oflags & ~flags);
	} else if (opt_s) {
		oflags = set_flags(module, oflags | flags);
	}
	print_flags(stdout, module, oflags, 0);

	return 0;
}

#define FLAG(mname, fname)	\
      { #mname, #fname, mname##DBG_##fname }
#define SHORTFLAG(mname, fname, dbgname)	\
      { #mname, #fname, mname##DBG_##dbgname }

static struct flagmap {
	char *		module;
	char *		name;
	unsigned int	value;
}			flagmap[] = {
	/* rpc */
	FLAG(RPC,	XPRT),
	FLAG(RPC,	CALL),
	FLAG(RPC,	DEBUG),
	FLAG(RPC,	NFS),
	FLAG(RPC,	AUTH),
	FLAG(RPC,	PMAP),
	FLAG(RPC,	SCHED),
	FLAG(RPC,	SVCSOCK),
	FLAG(RPC,	SVCDSP),
	FLAG(RPC,	MISC),
	FLAG(RPC,	ALL),

	/* nfs */
	FLAG(NFS,	VFS),
	FLAG(NFS,	DIRCACHE),
	FLAG(NFS,	LOOKUPCACHE),
	FLAG(NFS,	PAGECACHE),
	FLAG(NFS,	PROC),
	FLAG(NFS,	ALL),
	SHORTFLAG(NFS,	dir,	DIRCACHE),
	SHORTFLAG(NFS,	lookup,	LOOKUPCACHE),
	SHORTFLAG(NFS,	page,	PAGECACHE),

	/* nfsd */
	FLAG(NFSD,	SOCK),
	FLAG(NFSD,	FH),
	FLAG(NFSD,	EXPORT),
	FLAG(NFSD,	SVC),
	FLAG(NFSD,	PROC),
	FLAG(NFSD,	FILEOP),
	FLAG(NFSD,	AUTH),
	FLAG(NFSD,	REPCACHE),
	FLAG(NFSD,	XDR),
	FLAG(NFSD,	LOCKD),
	FLAG(NFSD,	ALL),

	/* lockd */
	FLAG(NLM,	SVC),
	FLAG(NLM,	CLIENT),
	FLAG(NLM,	CLNTLOCK),
	FLAG(NLM,	SVCLOCK),
	FLAG(NLM,	MONITOR),
	FLAG(NLM,	CLNTSUBS),
	FLAG(NLM,	SVCSUBS),
	FLAG(NLM,	ALL),

      { NULL,		NULL,		0 }
};

static unsigned int
find_flag(char **module, char *name)
{
	char		*mod = *module;
	unsigned int	value = 0;
	int		i;

	for (i = 0; flagmap[i].module; i++) {
		if ((mod && strcasecmp(mod, flagmap[i].module))
		 || strcasecmp(name, flagmap[i].name))
			continue;
		if (value) {
			fprintf(stderr,
				"rpcdebug: ambiguous symbol name %s.\n"
				"This name is used by more than one module, "
				"please specify the module name using\n"
				"the -m option.\n",
				name);
			usage(1);
		}
		value = flagmap[i].value;
		if (*module)
			return value;
		mod = flagmap[i].module;
	}

	if (!value) {
		if (*module)
			fprintf(stderr,
				"rpcdebug: unknown module or flag %s/%s\n",
				*module, name);
		else
			fprintf(stderr,
				"rpcdebug: unknown flag %s\n",
				name);
		exit(1);
	}

	*module = mod;
	return value;
}

static unsigned int
get_flags(char *module)
{
	char	buffer[256], *sp;
	int	sysfd, len, namelen;

	if ((sysfd = open("/proc/net/rpc/debug", O_RDONLY)) < 0) {
		perror("/proc/net/rpc/debug");
		exit(1);
	}
	if ((len = read(sysfd, buffer, sizeof(buffer))) < 0) {
		perror("read");
		exit(1);
	}
	close(sysfd);
	buffer[len - 1] = '\0';

	namelen = strlen(module);
	for (sp = strtok(buffer, " \t"); sp; sp = strtok(NULL, " \t")) {
		if (!strncmp(sp, module, namelen) && sp[namelen] == '=') {

			return strtoul(sp + namelen + 1, NULL, 0);
		}
	}

	fprintf(stderr, "Unknown module %s\n", module);
	exit(1);
}

static unsigned int
set_flags(char *module, unsigned int value)
{
	char	buffer[64];
	int	sysfd, len, ret;

	len = sprintf(buffer, "%s=%u\n", module, value);
	if ((sysfd = open("/proc/net/rpc/debug", O_WRONLY)) < 0) {
		perror("/proc/net/rpc/debug");
		exit(1);
	}
	if ((ret = write(sysfd, buffer, len)) < 0) {
		perror("write");
		exit(1);
	}
	if (ret < len) {
		fprintf(stderr, "error: short write in set_flags!\n");
		exit(1);
	}
	close(sysfd);
	return value;
}

static int
find_sysname(char *module)
{
	char	procname[1024];
	int	fd;

	module = strtolower(module);

	sprintf(procname, "/proc/sys/sunrpc/%s_debug", module);
	if ((fd = open(procname, O_RDWR)) < 0) {
		perror(procname);
		exit(1);
	}

	return fd;
}

static char *
strtolower(char *str)
{
	static char	temp[64];
	char		*sp;

	strcpy(temp, str);
	for (sp = temp; *sp; sp++)
		*sp = tolower(*sp);
	return temp;
}

static void
print_flags(FILE *ofp, char *module, unsigned int flags, int show_all)
{
	char		*lastmod = NULL;
	unsigned int	shown = 0;
	int		i;

	if (module) {
		fprintf(ofp, "%-10s", strtolower(module));
		if (!flags) {
			fprintf(ofp, "<no flags set>\n");
			return;
		}
		/* printf(" <%x>", flags); */
	}

	for (i = 0, shown = 0; flagmap[i].module; i++) {
		if (module) {
			if (strcasecmp(flagmap[i].module, module))
				continue;
		} else if (!lastmod || strcmp(lastmod, flagmap[i].module)) {
			if (lastmod) {
				fprintf(ofp, "\n");
				shown = 0;
			}
			fprintf(ofp, "%-10s", strtolower(flagmap[i].module));
			lastmod = flagmap[i].module;
		}
		if (!(flags & flagmap[i].value)
		 || (!show_all && (shown & flagmap[i].value))
		 || (module && !strcasecmp(flagmap[i].name, "all")))
			continue;
		fprintf(ofp, " %s", strtolower(flagmap[i].name));
		shown |= flagmap[i].value;
	}
	fprintf(ofp, "\n");
}

static void
usage(int excode)
{
	fprintf(stderr, "usage: rpcdebug [-m module] [-cs] flags ...\n");
	if (verbose) {
		fprintf(stderr, "\nModule     Valid flags\n");
		print_flags(stderr, NULL, ~(unsigned int) 0, 1);
	} else {
		fprintf(stderr,
		    "       (use rpcdebug -vh to get a list of valid flags)\n");
	}
	exit (excode);
}

