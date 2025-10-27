#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <asm/hwprobe.h> // For struct riscv_hwprobe and the syscall number

int main() {
    struct riscv_hwprobe probe_items[2];

    probe_items[0].key = RISCV_HWPROBE_KEY_BASE_BEHAVIOR;
    probe_items[0].value = 0; // Value will be filled by the kernel
    probe_items[1].key = RISCV_HWPROBE_KEY_IMA_EXT_0;
    probe_items[1].value = 0; // Value will be filled by the kernel

    // Call the hwprobe syscall
    long result = syscall(SYS_riscv_hwprobe, probe_items, 2, 0, NULL, 0);

    if (result < 0) {
        perror("sys_riscv_hwprobe failed");
        return 1;
    }

    // Check the result
    if (probe_items[0].value & RISCV_HWPROBE_BASE_BEHAVIOR_IMA) {
        printf("RISC-V base supported.\n");
    } else {
        printf("RISC-V base is notsupported.\n");
	exit(1);
    }

    // Check the result
    printf("RISCV_HWPROBE_KEY_IMA_EXT_0: 0x%llx\n", probe_items[1].value);
    if (probe_items[1].value & RISCV_HWPROBE_EXT_ZBA) {
        printf("RISC-V hardware probe: Zba extension is supported.\n");
    } else {
        printf("RISC-V hardware probe: Zba extension is not supported.\n");
    }

    return 0;
}
