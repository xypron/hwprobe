// SPDX-License-Identifier: MIT
/*
 * Unit tests for hwprobe.c using synthetic riscv_hwprobe() results modelled
 * on the Linux 7.3 and 7.4 interfaces. Runs on any architecture.
 */

#define main hwprobe_main
#include "hwprobe.c"
#undef main

#define LINUX_7_3	0x00070003
#define LINUX_7_4	0x00070004

static int failures;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, \
			__LINE__, #cond); \
		++failures; \
	} \
} while (0)

/* Fill probe like a kernel that knows all keys and reports all extensions */
static void probe_full(struct probe_result *probe, unsigned int version)
{
	probe_init(probe, version);
	probe->items[PROBE_BASE].value = RISCV_HWPROBE_BASE_BEHAVIOR_IMA;
	probe->items[PROBE_EXT_0].value = ~0ULL;
	probe->items[PROBE_EXT_1].value = ~0ULL;
	probe->items[PROBE_ZICBOZ_SIZE].value = 64;
	probe->items[PROBE_ZICBOM_SIZE].value = 64;
	probe->items[PROBE_ZICBOP_SIZE].value = 128;
	probe->items[PROBE_HIGHEST_VA].value = (1ULL << 47) - 1;
}

static void probe_unknown(struct probe_result *probe, int item)
{
	probe->items[item].key = -1;
	probe->items[item].value = 0;
}

static const struct server_row *find_row(const struct server_row *rows,
					 size_t n, const char *item)
{
	for (size_t i = 0; i < n; ++i)
		if (!strcmp(rows[i].item, item))
			return &rows[i];

	return NULL;
}

static enum server_result row_result(const struct probe_result *probe,
				     int rva23u_missing, const char *item)
{
	struct server_row rows[SERVER_ROWS_MAX];
	size_t n = check_server(probe, rva23u_missing, rows);
	const struct server_row *row = find_row(rows, n, item);

	CHECK(n <= SERVER_ROWS_MAX);
	CHECK(row != NULL);

	return row ? row->result : SRV_NA;
}

static void test_probe_keys(void)
{
	CHECK(RISCV_HWPROBE_KEY_ZICBOZ_BLOCK_SIZE == 6);
	CHECK(RISCV_HWPROBE_KEY_ZICBOM_BLOCK_SIZE == 12);
	CHECK(RISCV_HWPROBE_KEY_ZICBOP_BLOCK_SIZE == 15);
	CHECK(probe_keys[PROBE_ZICBOZ_SIZE] == 6);
	CHECK(probe_keys[PROBE_ZICBOM_SIZE] == 12);
	CHECK(probe_keys[PROBE_ZICBOP_SIZE] == 15);
}

static void test_block_size_text(void)
{
	struct probe_result probe;
	char text[32];

	probe_full(&probe, LINUX_7_4);
	block_size_text(&probe, &blocks[0], text, sizeof(text));
	CHECK(!strcmp(text, "64 bytes"));
	block_size_text(&probe, &blocks[1], text, sizeof(text));
	CHECK(!strcmp(text, "64 bytes"));
	block_size_text(&probe, &blocks[2], text, sizeof(text));
	CHECK(!strcmp(text, "128 bytes"));

	/* Extension absent */
	probe.items[PROBE_EXT_0].value &= ~RISCV_HWPROBE_EXT_ZICBOZ;
	block_size_text(&probe, &blocks[0], text, sizeof(text));
	CHECK(!strcmp(text, "-"));

	/* Extension present but size 0 */
	probe.items[PROBE_ZICBOM_SIZE].value = 0;
	block_size_text(&probe, &blocks[1], text, sizeof(text));
	CHECK(!strcmp(text, "unknown"));

	/* Kernel without the key, e.g. Zicbop before 6.19 */
	probe_unknown(&probe, PROBE_ZICBOP_SIZE);
	block_size_text(&probe, &blocks[2], text, sizeof(text));
	CHECK(!strcmp(text, "not reported by kernel"));
}

static void test_extensions(void)
{
	struct probe_result probe;

	probe_full(&probe, LINUX_7_3);
	CHECK(check_extensions(&probe, 0) == 0);

	/* Linux 7.4 knows the same extensions as 7.3 */
	probe_full(&probe, LINUX_7_4);
	CHECK(check_extensions(&probe, 0) == 0);

	/* Missing required Zicbop and Zicclsm */
	probe.items[PROBE_EXT_0].value &= ~RISCV_HWPROBE_EXT_ZICBOP;
	probe.items[PROBE_EXT_1].value &= ~RISCV_HWPROBE_EXT_ZICCLSM;
	CHECK(check_extensions(&probe, 0) == 2);

	/* Without IMA_EXT_1 the 7.3 extensions are not required */
	probe_full(&probe, LINUX_7_3);
	probe_unknown(&probe, PROBE_EXT_1);
	CHECK(check_extensions(&probe, 0) == 5);
}

static void test_server_pass(void)
{
	struct probe_result probe;
	struct server_row rows[SERVER_ROWS_MAX];
	size_t n;

	probe_full(&probe, LINUX_7_4);
	n = check_server(&probe, 0, rows);

	CHECK(server_count(rows, n, SRV_FAIL) == 0);
	CHECK(server_count(rows, n, SRV_WARN) == 0);
	CHECK(row_result(&probe, 0, "RVA23S64") == SRV_NA);
	CHECK(row_result(&probe, 0, "Sv48") == SRV_PASS);
	CHECK(row_result(&probe, 0, "Zicntr") == SRV_PASS);
	CHECK(row_result(&probe, 0, "Zihpm") == SRV_PASS);
	CHECK(row_result(&probe, 0, "Zkr") == SRV_NA);
	CHECK(row_result(&probe, 0, "Ssaia") == SRV_NA);
	CHECK(row_result(&probe, 0, "Svadu") == SRV_NA);
	CHECK(row_result(&probe, 0, "Ssctr") == SRV_NA);
}

static void test_server_fail(void)
{
	struct probe_result probe;

	probe_full(&probe, LINUX_7_3);

	CHECK(row_result(&probe, 3, "RVA23S64") == SRV_FAIL);

	/* Sv39 only */
	probe.items[PROBE_HIGHEST_VA].value = (1ULL << 38) - 1;
	CHECK(row_result(&probe, 0, "Sv48") == SRV_FAIL);
	/* Sv57 */
	probe.items[PROBE_HIGHEST_VA].value = (1ULL << 56) - 1;
	CHECK(row_result(&probe, 0, "Sv48") == SRV_PASS);
	probe_unknown(&probe, PROBE_HIGHEST_VA);
	CHECK(row_result(&probe, 0, "Sv48") == SRV_NA);

	probe.items[PROBE_EXT_0].value &= ~RISCV_HWPROBE_EXT_ZICNTR;
	probe.items[PROBE_EXT_0].value &= ~RISCV_HWPROBE_EXT_ZIHPM;
	CHECK(row_result(&probe, 0, "Zicntr") == SRV_FAIL);
	CHECK(row_result(&probe, 0, "Zihpm") == SRV_FAIL);
}

static void test_server_failed_ids(void)
{
	struct probe_result probe;
	struct server_row rows[SERVER_ROWS_MAX];
	char ids[128];
	size_t n;

	probe_full(&probe, LINUX_7_4);
	n = check_server(&probe, 0, rows);
	server_failed_ids(rows, n, ids, sizeof(ids));
	CHECK(!strcmp(ids, ""));

	/* Each ID is listed once, in table order */
	probe.items[PROBE_HIGHEST_VA].value = (1ULL << 38) - 1;
	probe.items[PROBE_EXT_0].value &= ~RISCV_HWPROBE_EXT_ZICNTR;
	probe.items[PROBE_EXT_0].value &= ~RISCV_HWPROBE_EXT_ZIHPM;
	n = check_server(&probe, 2, rows);
	server_failed_ids(rows, n, ids, sizeof(ids));
	CHECK(!strcmp(ids, "RVA_010, RVA_020, RVA_060"));
}

static void test_server_old_kernel(void)
{
	struct probe_result probe;

	/* Linux 6.14 has no Zicntr/Zihpm bits */
	probe_full(&probe, 0x0006000e);
	CHECK(row_result(&probe, 0, "Zicntr") == SRV_NA);
	CHECK(row_result(&probe, 0, "Zihpm") == SRV_NA);
}

int main(void)
{
	test_probe_keys();
	test_block_size_text();
	test_extensions();
	test_server_pass();
	test_server_fail();
	test_server_failed_ids();
	test_server_old_kernel();

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	printf("hwprobe_test: all checks passed\n");

	return EXIT_SUCCESS;
}
