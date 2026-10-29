// SPDX-License-Identifier: MIT
/*
 * Check if the current system is RVA23 ready
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <asm/hwprobe.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Indices into the probe item array */
enum probe_idx {
	PROBE_BASE,
	PROBE_EXT_0,
	PROBE_EXT_1,
	PROBE_ZICBOZ_SIZE,
	PROBE_ZICBOM_SIZE,
	PROBE_ZICBOP_SIZE,
	PROBE_COUNT,
};

static const long long probe_keys[PROBE_COUNT] = {
	[PROBE_BASE] = RISCV_HWPROBE_KEY_BASE_BEHAVIOR,
	[PROBE_EXT_0] = RISCV_HWPROBE_KEY_IMA_EXT_0,
	[PROBE_EXT_1] = RISCV_HWPROBE_KEY_IMA_EXT_1,
	[PROBE_ZICBOZ_SIZE] = RISCV_HWPROBE_KEY_ZICBOZ_BLOCK_SIZE,
	[PROBE_ZICBOM_SIZE] = RISCV_HWPROBE_KEY_ZICBOM_BLOCK_SIZE,
	[PROBE_ZICBOP_SIZE] = RISCV_HWPROBE_KEY_ZICBOP_BLOCK_SIZE,
};

struct ext_desc {
	int probe_item;
	long long key;
	const char *text;
	int required;
	unsigned int introduced;
};

static void print_usage(FILE *stream, const char *prog)
{
	fprintf(stream,
		"Usage: %s [-v|--verbose] [-h|--help]\n"
		"\n"
		"Check whether this RISC-V system supports the extensions required\n"
		"by the RVA23U64 profile, using the riscv_hwprobe() syscall.\n"
		"\n"
		"Options:\n"
		"  -v, --verbose  list every extension with its detected/required status\n"
		"                 and the cache block sizes\n"
		"  -h, --help     display this help and exit\n",
		prog);
}

static unsigned int kernel_version(void)
{
	struct utsname uname;
	unsigned int major, minor;
	char *token;
	char *endptr;
	int ret;

	ret = syscall(SYS_uname, &uname);
	if (ret) {
		perror("sys_uname failed");
		return 0;
	}
	printf("Kernel release %s\n\n", uname.release);

	token = strtok(uname.release, ".");
	if (!token)
		goto error;
	major = strtol(token, &endptr, 10);
	if (*endptr)
		goto error;
	token = strtok(NULL, ".");
	if (!token)
		goto error;
	minor = strtol(token, &endptr, 10);

	return major << 16 | minor;

error:
	fprintf(stderr, "Invalid kernel version string\n");

	return 0;
}

static const struct ext_desc exts[] = {
	{PROBE_EXT_0, RISCV_HWPROBE_IMA_FD, "F and D", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_IMA_C, "C", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_IMA_V, "V", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZBA, "Zba", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZBB, "Zbb", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZBS, "Zbs", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZICBOZ, "Zicboz", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZBC, "Zbc", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZBKB, "Zbkb", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZBKC, "Zbkc", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZBKX, "Zbkx", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZKND, "Zknd", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZKNE, "Zkne", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZKNH, "Zknh", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZKSED, "Zksed", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZKSH, "Zksh", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZKT, "Zkt", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVBB, "Zvbb", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVBC, "Zvbc", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKB, "Zvkb", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKG, "Zvkg", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKNED, "Zvkned", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKNHA, "Zvknha", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKNHB, "Zvknhb", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKSED, "Zvksed", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKSH, "Zvksh", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVKT, "Zvkt", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZFH, "Zfh", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZFHMIN, "Zfhmin", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZIHINTNTL, "Zihintntl", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVFH, "Zvfh", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVFHMIN, "Zvfhmin", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZFA, "Zfa", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZTSO, "Ztso", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZACAS, "Zacas", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZICOND, "Zicond", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZIHINTPAUSE, "Zihintpause", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVE32X, "Zve32x", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVE32F, "Zve32f", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVE64X, "Zve64x", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVE64F, "Zve64f", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVE64D, "Zfe64d", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZIMOP, "Zimop", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZCA, "Zca", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZCB, "Zcb", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZCD, "Zcd", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZCF, "Zcf", 0, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZCMOP, "Zcmop", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZAWRS, "Zawrs", 1, 0},
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_SUPM, "Supm", 1, 0x0006000d}, // 6.13
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZICNTR, "Zicntr", 1, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZIHPM, "Zihpm", 1, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZFBFMIN, "Zfbfmin", 0, 0x0006000f }, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVFBFMIN, "Zvfbfmin", 0, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZVFBFWMA, "Zvfbfwma", 0, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZICBOM, "Zicbom", 1, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZAAMO, "Zaamo", 1, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZALRSC, "Zalrsc", 1, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZABHA, "Zabha", 0, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZALASR, "Zalasr", 0, 0x0006000f}, // 6.15
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZICBOP, "Zicbop", 1, 0x00060013}, // 6.19
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZILSD, "Zilsd", 0, 0x00060013}, // 6.19
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZCLSD, "Zclsd", 0, 0x00060013}, // 6.19
	{PROBE_EXT_0, RISCV_HWPROBE_EXT_ZICFILP, "Zicfilp", 0, 0x00070000}, // 7.0
	{PROBE_EXT_1, RISCV_HWPROBE_EXT_ZICFISS, "Zicfiss", 0, 0x00070000}, // 7.0
	{PROBE_EXT_1, RISCV_HWPROBE_EXT_ZICCLSM, "Zicclsm", 1, 0x00070003}, // 7.3
	{PROBE_EXT_1, RISCV_HWPROBE_EXT_ZICCAMOA, "Ziccamoa", 1, 0x00070003}, // 7.3
	{PROBE_EXT_1, RISCV_HWPROBE_EXT_ZICCIF, "Ziccif", 1, 0x00070003}, // 7.3
	{PROBE_EXT_1, RISCV_HWPROBE_EXT_ZICCRSE, "Ziccrse", 1, 0x00070003}, // 7.3
	{PROBE_EXT_1, RISCV_HWPROBE_EXT_ZA64RS, "Za64rs", 1, 0x00070003}, // 7.3
};

struct block_desc {
	int size_item;
	long long ext_key;
	const char *text;
};

static const struct block_desc blocks[] = {
	{PROBE_ZICBOZ_SIZE, RISCV_HWPROBE_EXT_ZICBOZ, "Zicboz"},
	{PROBE_ZICBOM_SIZE, RISCV_HWPROBE_EXT_ZICBOM, "Zicbom"},
	{PROBE_ZICBOP_SIZE, RISCV_HWPROBE_EXT_ZICBOP, "Zicbop"},
};

struct probe_result {
	struct riscv_hwprobe items[PROBE_COUNT];
	unsigned int version;
};

static void probe_init(struct probe_result *probe, unsigned int version)
{
	for (size_t i = 0; i < PROBE_COUNT; ++i) {
		probe->items[i].key = probe_keys[i];
		probe->items[i].value = 0;
	}
	probe->version = version;
}

/* The kernel sets the key of an unknown item to -1 */
static int probe_known(const struct probe_result *probe, int item)
{
	return probe->items[item].key == probe_keys[item];
}

static int ext_available(const struct probe_result *probe,
			 const struct ext_desc *ext)
{
	return probe_known(probe, ext->probe_item) &&
	       ext->introduced <= probe->version;
}

static int ext_detected(const struct probe_result *probe,
			const struct ext_desc *ext)
{
	return ext_available(probe, ext) &&
	       (probe->items[ext->probe_item].value & ext->key) != 0;
}

/*
 * Describe the block size of a cache block operation extension.
 */
static void block_size_text(const struct probe_result *probe,
			    const struct block_desc *block,
			    char *buf, size_t len)
{
	unsigned long long size = probe->items[block->size_item].value;

	if (!probe_known(probe, block->size_item))
		snprintf(buf, len, "not reported by kernel");
	else if (!probe_known(probe, PROBE_EXT_0) ||
		 !(probe->items[PROBE_EXT_0].value & block->ext_key))
		snprintf(buf, len, "-");
	else if (!size)
		snprintf(buf, len, "unknown");
	else
		snprintf(buf, len, "%llu bytes", size);
}

static void print_block_sizes(const struct probe_result *probe)
{
	char text[32];

	printf("%-14s %s\n", "Block size", "Size");
	for (size_t i = 0; i < ARRAY_SIZE(blocks); ++i) {
		block_size_text(probe, &blocks[i], text, sizeof(text));
		printf("%-14s %s\n", blocks[i].text, text);
	}
	printf("\n");
}

/* Returns the number of required extensions that are missing */
static int check_extensions(const struct probe_result *probe, int verbose)
{
	int missing_count = 0;

	if (verbose)
		printf("%-14s %-10s %s\n",
		       "Extension", "Detected", "Required by RVA23U64");

	for (size_t i = 0; i < ARRAY_SIZE(exts); ++i) {
		int available = ext_available(probe, &exts[i]);
		int detected = ext_detected(probe, &exts[i]);
		int required = exts[i].required &&
			       exts[i].introduced <= probe->version;

		if (verbose) {
			printf("%-14s %-10s %s\n", exts[i].text,
			       available ? (detected ? "yes" : "no") : "-",
			       exts[i].required ? "yes" : "no");
		}

		if (!detected && required) {
			++missing_count;
			if (!verbose)
				printf("%s NOT supported\n", exts[i].text);
		}
	}

	return missing_count;
}

static int do_hwprobe(struct probe_result *probe)
{
#ifdef __riscv
	return syscall(SYS_riscv_hwprobe, probe->items, PROBE_COUNT, 0, NULL,
		       0);
#else
	(void)probe;
	errno = ENOSYS;

	return -1;
#endif
}

int main(int argc, char *argv[])
{
	static const struct option long_opts[] = {
		{"verbose", no_argument, NULL, 'v'},
		{"help",    no_argument, NULL, 'h'},
		{NULL,      0,           NULL, 0},
	};
	int verbose = 0;
	int opt;
	int missing_count;
	struct probe_result probe;

	while ((opt = getopt_long(argc, argv, "vh", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case 'h':
			print_usage(stdout, argv[0]);
			return EXIT_SUCCESS;
		default:
			print_usage(stderr, argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind != argc) {
		fprintf(stderr, "%s: unexpected argument '%s'\n\n",
			argv[0], argv[optind]);
		print_usage(stderr, argv[0]);
		return EXIT_FAILURE;
	}

#ifndef __riscv
	fprintf(stderr, "%s: riscv_hwprobe() requires a RISC-V system\n",
		argv[0]);
	return EXIT_FAILURE;
#endif

	probe_init(&probe, kernel_version());

	// Call the hwprobe syscall
	if (do_hwprobe(&probe)) {
		perror("sys_riscv_hwprobe failed");

		return EXIT_FAILURE;
	}

	// Check RISC-V base
	if (!(probe.items[PROBE_BASE].value & RISCV_HWPROBE_BASE_BEHAVIOR_IMA)) {
		printf("RISC-V base is NOT supported.\n");

		return EXIT_FAILURE;
	}

	missing_count = check_extensions(&probe, verbose);

	if (verbose || missing_count)
		printf("\n");

	if (verbose)
		print_block_sizes(&probe);

	if (!missing_count) {
		printf("This system probably supports the RVA23U64 profile.\n");
		return EXIT_SUCCESS;
	}

	printf("This system does not support the RVA23U64 profile.\n");

	return EXIT_FAILURE;
}
