// SPDX-License-Identifier: MIT
/*
 * Check if the current system is RVA23 ready
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <asm/hwprobe.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct ext_desc {
	long long key;
	const char *text;
	int required;
	unsigned int introduced;
};

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
	printf("Kernel version: %s\n", uname.release);

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

int main()
{
	unsigned int version;
	struct riscv_hwprobe probe_items[2];
	struct ext_desc exts[] = {
		{RISCV_HWPROBE_IMA_FD, "F and D", 1, 0},
		{RISCV_HWPROBE_IMA_C, "C", 1, 0},
		{RISCV_HWPROBE_IMA_V, "V", 1, 0},
		{RISCV_HWPROBE_EXT_ZBA, "Zba", 1, 0},
		{RISCV_HWPROBE_EXT_ZBB, "Zbb", 1, 0},
		{RISCV_HWPROBE_EXT_ZBS, "Zbs", 1, 0},
		{RISCV_HWPROBE_EXT_ZICBOZ, "Zicboz", 1, 0},
		{RISCV_HWPROBE_EXT_ZBC, "Zbc", 0, 0},
		{RISCV_HWPROBE_EXT_ZBKB, "Zbkb", 0, 0},
		{RISCV_HWPROBE_EXT_ZBKC, "Zbkc", 0, 0},
		{RISCV_HWPROBE_EXT_ZBKX, "Zbkx", 0, 0},
		{RISCV_HWPROBE_EXT_ZKND, "Zknd", 0, 0},
		{RISCV_HWPROBE_EXT_ZKNE, "Zkne", 0, 0},
		{RISCV_HWPROBE_EXT_ZKNH, "Zknh", 0, 0},
		{RISCV_HWPROBE_EXT_ZKSED, "Zksed", 0, 0},
		{RISCV_HWPROBE_EXT_ZKSH, "Zksh", 0, 0},
		{RISCV_HWPROBE_EXT_ZKT, "Zkt", 1, 0},
		{RISCV_HWPROBE_EXT_ZVBB, "Zvbb", 1, 0},
		{RISCV_HWPROBE_EXT_ZVBC, "Zvbc", 0, 0},
		{RISCV_HWPROBE_EXT_ZVKB, "Zvkb", 1, 0},
		{RISCV_HWPROBE_EXT_ZVKG, "Zvkg", 0, 0},
		{RISCV_HWPROBE_EXT_ZVKNED, "Zvkned", 0, 0},
		{RISCV_HWPROBE_EXT_ZVKNHA, "Zvknha", 0, 0},
		{RISCV_HWPROBE_EXT_ZVKNHB, "Zvknhb", 0, 0},
		{RISCV_HWPROBE_EXT_ZVKSED, "Zvksed", 0, 0},
		{RISCV_HWPROBE_EXT_ZVKSH, "Zvksh", 0, 0},
		{RISCV_HWPROBE_EXT_ZVKT, "Zvkt", 1, 0},
		{RISCV_HWPROBE_EXT_ZFH, "Zfh", 0, 0},
		{RISCV_HWPROBE_EXT_ZFHMIN, "Zfhmin", 1, 0},
		{RISCV_HWPROBE_EXT_ZIHINTNTL, "Zihintntl", 1, 0},
		{RISCV_HWPROBE_EXT_ZVFH, "Zvfh", 0, 0},
		{RISCV_HWPROBE_EXT_ZVFHMIN, "Zvfhmin", 1, 0},
		{RISCV_HWPROBE_EXT_ZFA, "Zfa", 1, 0},
		{RISCV_HWPROBE_EXT_ZTSO, "Ztso", 0, 0},
		{RISCV_HWPROBE_EXT_ZACAS, "Zacas", 0, 0},
		{RISCV_HWPROBE_EXT_ZICNTR, "Zicntr", 1, 0},
		{RISCV_HWPROBE_EXT_ZICOND, "Zicond", 1, 0},
		{RISCV_HWPROBE_EXT_ZIHINTPAUSE, "Zihintpause", 1, 0},
		{RISCV_HWPROBE_EXT_ZIHPM, "Zihpm", 1, 0},
		{RISCV_HWPROBE_EXT_ZVE32X, "Zve32x", 1, 0},
		{RISCV_HWPROBE_EXT_ZVE32F, "Zve32f", 1, 0},
		{RISCV_HWPROBE_EXT_ZVE64X, "Zve64x", 1, 0},
		{RISCV_HWPROBE_EXT_ZVE64F, "Zve64f", 1, 0},
		{RISCV_HWPROBE_EXT_ZVE64D, "Zfe64d", 1, 0},
		{RISCV_HWPROBE_EXT_ZIMOP, "Zimop", 1, 0},
		{RISCV_HWPROBE_EXT_ZCA, "Zca", 1, 0},
		{RISCV_HWPROBE_EXT_ZCB, "Zcb", 1, 0},
		{RISCV_HWPROBE_EXT_ZCD, "Zcd", 1, 0},
		{RISCV_HWPROBE_EXT_ZCF, "Zcf", 0, 0},
		{RISCV_HWPROBE_EXT_ZCMOP, "Zcmop", 1, 0},
		{RISCV_HWPROBE_EXT_ZAWRS, "Zawrs", 1, 0},
		{RISCV_HWPROBE_EXT_ZAAMO, "Zaamo", 1, 0},
		{RISCV_HWPROBE_EXT_ZALRSC, "Zalrsc", 1, 0},
		{RISCV_HWPROBE_EXT_SUPM, "Supm", 1, 0},
		{RISCV_HWPROBE_EXT_ZFBFMIN, "Zfbfmin", 0, 0},
		{RISCV_HWPROBE_EXT_ZVFBFMIN, "Zvfbfmin", 0, 0},
		{RISCV_HWPROBE_EXT_ZVFBFWMA, "Zvfbfwma", 0, 0},
		{RISCV_HWPROBE_EXT_ZICBOM, "Zicbom", 1, 0},
		{RISCV_HWPROBE_EXT_ZABHA, "Zabha", 0, 0},
		{RISCV_HWPROBE_EXT_ZALASR, "Zalasr", 0, 0},
		{RISCV_HWPROBE_EXT_ZICBOP, "Zicbop", 1, 0},
		{RISCV_HWPROBE_EXT_ZILSD, "Zilsd", 0, 0},
		{RISCV_HWPROBE_EXT_ZCLSD, "Zclsd", 0, 0},
	};
	long ret;

	probe_items[0].key = RISCV_HWPROBE_KEY_BASE_BEHAVIOR;
	probe_items[0].value = 0;
	probe_items[1].key = RISCV_HWPROBE_KEY_IMA_EXT_0;
	probe_items[1].value = 0;

	version = kernel_version();

	// Call the hwprobe syscall
	ret = syscall(SYS_riscv_hwprobe, probe_items, 2, 0, NULL, 0);
	if (ret) {
		perror("sys_riscv_hwprobe failed");

		return EXIT_FAILURE;
	}

	// Check RISC-V base
	if (!(probe_items[0].value & RISCV_HWPROBE_BASE_BEHAVIOR_IMA)) {
		printf("RISC-V base is NOT supported.\n");

		return EXIT_FAILURE;
	}

	// Check extensions
	for (size_t i = 0; i < ARRAY_SIZE(exts); ++i) {
		if (!(probe_items[1].value & exts[i].key) &&
		    exts[i].required && exts[i].introduced <= version) {
			printf("%s NOT supported\n", exts[i].text);
			return EXIT_FAILURE;
		}
	}

	printf("SYS_riscv_hwprobe %d\n", SYS_riscv_hwprobe);

	return EXIT_SUCCESS;
}
