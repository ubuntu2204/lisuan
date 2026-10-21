/*
 * hda-probe.c — dump a PCI HD-Audio controller's global registers.
 *
 * Used to tell "this PCI function really is a standard HDA controller"
 * (GCAP/VMIN/VMAJ/STATESTS readable) from "it is some vendor-private block".
 *
 * Build: gcc -O2 -o hda-probe hda-probe.c
 * Run  : sudo ./hda-probe 0000:05:00.1
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
#define DUMP_WORDS 16

int main(int argc, char **argv)
{
	const char *bdf = (argc > 1) ? argv[1] : "0000:05:00.1";
	char path[256];
	int fd;
	void *map;
	volatile uint32_t *reg;
	int i;

	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/resource0", bdf);

	fd = open(path, O_RDWR | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", path, strerror(errno));
		return 1;
	}

	/* Map with write access so we do not rely on sysfs read() semantics. */
	map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	reg = (volatile uint32_t *)map;

	printf("%s HDA global registers:\n", bdf);
	for (i = 0; i < DUMP_WORDS; i++)
		printf("  0x%02x: 0x%08x\n", i * 4, reg[i]);

	{
		uint16_t gcap = (uint16_t)(reg[0] & 0xffff);
		uint8_t vmin = (uint8_t)((reg[0] >> 16) & 0xff);
		uint8_t vmaj = (uint8_t)((reg[0] >> 24) & 0xff);

		printf("\n  GCAP=0x%04x  VMIN=%u  VMAJ=%u  STATESTS=0x%04x\n",
		       gcap, vmin, vmaj, (uint16_t)(reg[2] & 0xffff));
		if (gcap == 0x0000 || gcap == 0xffff)
			printf("  => not a live HDA controller (clk/power off?)\n");
		else
			printf("  => looks like a live HDA controller "
			       "(NSDO=%u, NSDI=%u, BOSS=%u, 64BIT=%u)\n",
			       (gcap >> 12) & 0xf, (gcap >> 8) & 0xf,
			       (gcap >> 1) & 1, gcap & 1);
	}

	munmap(map, MAP_SIZE);
	close(fd);
	return 0;
}
