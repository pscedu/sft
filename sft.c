/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef MPI
#include <mpi.h>
MPI_Group world;
#endif

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/types.h"
#include "psc_util/alloc.h"
#include "psc_util/crc.h"
#include "psc_util/log.h"

int		 pes;
int		 mype;
unsigned long	 crc;
int		 debug;
ssize_t		 bufsz = 131072;

uint64_t	 filecrc;

char		*file;
char		*buf;

const char	*progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-cd] [-b bufsz] [-f filename]\n"
	    "\t -d (enable debugging output)\n"
	    "\t -c (enable MD5 checksummming)\n"
	    "\t -f filename (specify the filename to read)\n", progname);
	exit(1);
}

void
sft_barrier(void)
{
#ifdef MPI
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

#ifdef MPI
void
sft_parallel_init(int argc, char *argv[])
{
	int rc;

	if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
		abort();

	MPI_Comm_size(MPI_COMM_WORLD, &pes);
	MPI_Comm_rank(MPI_COMM_WORLD, &mype);

	rc = MPI_Comm_group(MPI_COMM_WORLD, &world);
	if (rc != MPI_SUCCESS)
		abort();
}
#else
void
sft_parallel_init(__unusedx int argc, __unusedx char *argv[])
{
}
#endif

static void
sft_parallel_finalize(void)
{
#ifdef MPI
	MPI_Finalize();
#endif
}

int
main(int argc, char *argv[])
{
	ssize_t rem, szrc;
	struct stat stb;
	size_t tmp;
	int c, fd;

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "b:cdf:")) != -1)
		switch (c) {
		case 'b':
			bufsz = strtol(optarg, NULL, 10);
			break;
		case 'c':
			crc = 1;
			PSC_CRC64_INIT(&filecrc);
			break;
		case 'd':
			debug = strtol(optarg, NULL, 10);
			break;
		case 'f':
			file = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	if (!file) {
		warnx("No input file specified");
		usage();
	}

	buf = PSCALLOC(bufsz);

	sft_parallel_init(argc, argv);
	sft_barrier();

	fd = open(file, O_RDONLY);
	if (fd < 0)
		abort();

	if (fstat(fd, &stb))
		abort();

	if (debug)
		fprintf(stdout, "filesize=%"PSCPRIdOFFT, stb.st_size);

	rem = stb.st_size;

	while (rem) {
		tmp = MIN(rem, bufsz);
		szrc = read(fd, buf, tmp);
		if (szrc != (ssize_t)tmp) {
			perror("failed to read");
			abort();
		} else {
			rem -= tmp;
			if (crc)
				psc_crc64_add(&filecrc, buf, tmp);
		}
	}

	if (crc) {
		PSC_CRC64_FIN(&filecrc);
		fprintf(stdout, "F '%s' CRC=%"PSCPRIxCRC64"\n", file, filecrc);
	}
	close(fd);

	sft_parallel_finalize();
	return (0);
}
