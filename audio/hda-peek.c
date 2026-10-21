/*
 * hda-peek.c — read or write one 32-bit register of a PCI HD-Audio controller.
 *
 * Handy for poking INTSTS/INTCTL/GCTL on a controller that has no working
 * driver yet, without rebooting:
 *
 *   sudo ./hda-peek 0000:05:00.1 0x24          # read INTSTS
 *   sudo ./hda-peek 0000:05:00.1 0x24 0xc0000000  # write-1-to-clear CIS/GIS
 *
 * License: Apache-2.0
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAP_SIZE 4096

int main(int argc, char **argv)
{
	const char *bdf;
	char path[256];
	unsigned long off = 0;
	int fd, write_mode = 0;
	uint32_t val = 0;
	void *map;
	volatile uint32_t *reg;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <bdf> <offset> [value]\n", argv[0]);
		return 2;
	}
	bdf = argv[1];
	off = strtoul(argv[2], NULL, 0);
	if (argc >= 4) {
		write_mode = 1;
		val = (uint32_t)strtoul(argv[3], NULL, 0);
	}
	if (off >= MAP_SIZE || (off & 3)) {
		fprintf(stderr, "offset must be 32-bit aligned and < 0x%x\n", MAP_SIZE);
		return 2;
	}

	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/resource0", bdf);
	fd = open(path, O_RDWR | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", path, strerror(errno));
		return 1;
	}
	map = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	reg = (volatile uint32_t *)((char *)map + off);
	if (write_mode) {
		uint32_t old = *reg;

		*reg = val;
		printf("%s[0x%03lx]: 0x%08x -> wrote 0x%08x, now 0x%08x\n",
		       bdf, off, old, val, *reg);
	} else {
		printf("%s[0x%03lx]: 0x%08x\n", bdf, off, *reg);
	}

	munmap(map, MAP_SIZE);
	close(fd);
	return 0;
}
