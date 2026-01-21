// SPDX-License-Identifier: MIT
/*
 * Check if system supports RVA23
 */

package main

import (
	"fmt"
	"os"
	"strconv"
	"strings"
	"syscall"
	"unsafe"
)

// Define constants for the keys based on your kernel definitions
const (
	SYS_riscv_hwprobe                    = 258
	RISCV_HWPROBE_KEY_BASE_BEHAVIOR      = 3
	RISCV_HWPROBE_BASE_BEHAVIOR_IMA      = 1
	RISCV_HWPROBE_KEY_IMA_EXT_0          = 4
)

// Extensions retrieved via RISCV_HWPROBE_KEY_IMA_EXT_0
const (
	RISCV_HWPROBE_IMA_FD          uint64 = 1 << 0
	RISCV_HWPROBE_IMA_C           uint64 = 1 << 1
	RISCV_HWPROBE_IMA_V           uint64 = 1 << 2
	RISCV_HWPROBE_EXT_ZBA         uint64 = 1 << 3
	RISCV_HWPROBE_EXT_ZBB         uint64 = 1 << 4
	RISCV_HWPROBE_EXT_ZBS         uint64 = 1 << 5
	RISCV_HWPROBE_EXT_ZICBOZ      uint64 = 1 << 6
	RISCV_HWPROBE_EXT_ZBC         uint64 = 1 << 7
	RISCV_HWPROBE_EXT_ZBKB        uint64 = 1 << 8
	RISCV_HWPROBE_EXT_ZBKC        uint64 = 1 << 9
	RISCV_HWPROBE_EXT_ZBKX        uint64 = 1 << 10
	RISCV_HWPROBE_EXT_ZKND        uint64 = 1 << 11
	RISCV_HWPROBE_EXT_ZKNE        uint64 = 1 << 12
	RISCV_HWPROBE_EXT_ZKNH        uint64 = 1 << 13
	RISCV_HWPROBE_EXT_ZKSED       uint64 = 1 << 14
	RISCV_HWPROBE_EXT_ZKSH        uint64 = 1 << 15
	RISCV_HWPROBE_EXT_ZKT         uint64 = 1 << 16
	RISCV_HWPROBE_EXT_ZVBB        uint64 = 1 << 17
	RISCV_HWPROBE_EXT_ZVBC        uint64 = 1 << 18
	RISCV_HWPROBE_EXT_ZVKB        uint64 = 1 << 19
	RISCV_HWPROBE_EXT_ZVKG        uint64 = 1 << 20
	RISCV_HWPROBE_EXT_ZVKNED      uint64 = 1 << 21
	RISCV_HWPROBE_EXT_ZVKNHA      uint64 = 1 << 22
	RISCV_HWPROBE_EXT_ZVKNHB      uint64 = 1 << 23
	RISCV_HWPROBE_EXT_ZVKSED      uint64 = 1 << 24
	RISCV_HWPROBE_EXT_ZVKSH       uint64 = 1 << 25
	RISCV_HWPROBE_EXT_ZVKT        uint64 = 1 << 26
	RISCV_HWPROBE_EXT_ZFH         uint64 = 1 << 27
	RISCV_HWPROBE_EXT_ZFHMIN      uint64 = 1 << 28
	RISCV_HWPROBE_EXT_ZIHINTNTL   uint64 = 1 << 29
	RISCV_HWPROBE_EXT_ZVFH        uint64 = 1 << 30
	RISCV_HWPROBE_EXT_ZVFHMIN     uint64 = 1 << 31
	RISCV_HWPROBE_EXT_ZFA         uint64 = 1 << 32
	RISCV_HWPROBE_EXT_ZTSO        uint64 = 1 << 33
	RISCV_HWPROBE_EXT_ZACAS       uint64 = 1 << 34
	RISCV_HWPROBE_EXT_ZICOND      uint64 = 1 << 35
	RISCV_HWPROBE_EXT_ZIHINTPAUSE uint64 = 1 << 36
	RISCV_HWPROBE_EXT_ZVE32X      uint64 = 1 << 37
	RISCV_HWPROBE_EXT_ZVE32F      uint64 = 1 << 38
	RISCV_HWPROBE_EXT_ZVE64X      uint64 = 1 << 39
	RISCV_HWPROBE_EXT_ZVE64F      uint64 = 1 << 40
	RISCV_HWPROBE_EXT_ZVE64D      uint64 = 1 << 41
	RISCV_HWPROBE_EXT_ZIMOP       uint64 = 1 << 42
	RISCV_HWPROBE_EXT_ZCA         uint64 = 1 << 43
	RISCV_HWPROBE_EXT_ZCB         uint64 = 1 << 44
	RISCV_HWPROBE_EXT_ZCD         uint64 = 1 << 45
	RISCV_HWPROBE_EXT_ZCF         uint64 = 1 << 46
	RISCV_HWPROBE_EXT_ZCMOP       uint64 = 1 << 47
	RISCV_HWPROBE_EXT_ZAWRS       uint64 = 1 << 48
	RISCV_HWPROBE_EXT_SUPM        uint64 = 1 << 49
	RISCV_HWPROBE_EXT_ZICNTR      uint64 = 1 << 50
	RISCV_HWPROBE_EXT_ZIHPM       uint64 = 1 << 51
	RISCV_HWPROBE_EXT_ZFBFMIN     uint64 = 1 << 52
	RISCV_HWPROBE_EXT_ZVFBFMIN    uint64 = 1 << 53
	RISCV_HWPROBE_EXT_ZVFBFWMA    uint64 = 1 << 54
	RISCV_HWPROBE_EXT_ZICBOM      uint64 = 1 << 55
	RISCV_HWPROBE_EXT_ZAAMO       uint64 = 1 << 56
	RISCV_HWPROBE_EXT_ZALRSC      uint64 = 1 << 57
	RISCV_HWPROBE_EXT_ZABHA       uint64 = 1 << 58
	RISCV_HWPROBE_EXT_ZALASR      uint64 = 1 << 59
	RISCV_HWPROBE_EXT_ZICBOP      uint64 = 1 << 60
	RISCV_HWPROBE_EXT_ZILSD       uint64 = 1 << 61
	RISCV_HWPROBE_EXT_ZCLSD       uint64 = 1 << 62
)

// Define riscv_hwprobe structure
type riscvHwprobe struct {
	Key   int64
	Value uint64
}

// Define extension descriptions
type extDesc struct {
	Key      uint64
	Text     string
	Required bool
	Since    uint32
}


func getKernelVersion() (uint32) {
	var uname syscall.Utsname
	var release string

	_, _, errno := syscall.Syscall(syscall.SYS_UNAME, uintptr(unsafe.Pointer(&uname)), 0, 0)
	if errno != 0 {
		fmt.Fprintf(os.Stderr, "sys_riscv_uname failed: %v\n", errno)
		return 0
	}
	release = string(uname.Release[:])

	fmt.Printf("Kernel release %s\n", release)

	// Split the release string on the dot
	parts := strings.Split(release, ".")
	if len(parts) < 2 {
		return 0
	}

	// Convert major and minor parts to uint32
	major, err := strconv.ParseUint(parts[0], 10, 32)
	if err != nil {
		return 0
	}

	minor, err := strconv.ParseUint(parts[1], 10, 32)
	if err != nil {
		return 0
	}

	// Combine major and minor versions into a single uint32 value
	version := (uint32(major) << 16) | uint32(minor)

	return version
}

func main() {
	var version uint32

	version = getKernelVersion()
	if (version == 0) {
		fmt.Fprintf(os.Stderr, "Can't get kernel version number")
		os.Exit(1)
	}

	// Initialize probe_items array
	probeItems := [2]riscvHwprobe{
		{Key: RISCV_HWPROBE_KEY_BASE_BEHAVIOR},
		{Key: RISCV_HWPROBE_KEY_IMA_EXT_0},
	}

	// Call the hwprobe syscall
	ret, _, errno := syscall.Syscall(SYS_riscv_hwprobe, uintptr(unsafe.Pointer(&probeItems[0])), 2, 0)
	if int(ret) < 0 {
		fmt.Fprintf(os.Stderr, "sys_riscv_hwprobe failed: %v\n", errno)
		os.Exit(1)
	}

	// Check RISC-V base behavior
	if probeItems[0].Value&RISCV_HWPROBE_BASE_BEHAVIOR_IMA == 0 {
		fmt.Println("RISC-V base is NOT supported.")
		os.Exit(1)
	}

	// Define extensions
	extensions := []extDesc{
		{Key: RISCV_HWPROBE_IMA_FD, Text: "F and D", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_IMA_C, Text: "C", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_IMA_V, Text: "V", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZBA, Text: "Zba", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZBB, Text: "Zbb", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZBS, Text: "Zbs", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZICBOZ, Text: "Zicboz", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZBC, Text: "Zbc", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZBKB, Text: "Zbkb", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZBKC, Text: "Zbkc", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZBKX, Text: "Zbkx", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZKND, Text: "Zknd", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZKNE, Text: "Zkne", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZKNH, Text: "Zknh", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZKSED, Text: "Zksed", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZKSH, Text: "Zksh", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZKT, Text: "Zkt", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVBB, Text: "Zvbb", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVBC, Text: "Zvbc", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKB, Text: "Zvkb", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKG, Text: "Zvkg", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKNED, Text: "Zvkned", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKNHA, Text: "Zvknha", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKNHB, Text: "Zvknhb", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKSED, Text: "Zvksed", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKSH, Text: "Zvksh", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVKT, Text: "Zvkt", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZFH, Text: "Zfh", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZFHMIN, Text: "Zfhmin", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZIHINTNTL, Text: "Zihintntl", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVFH, Text: "Zvfh", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVFHMIN, Text: "Zvfhmin", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZFA, Text: "Zfa", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZTSO, Text: "Ztso", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZACAS, Text: "Zacas", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZICNTR, Text: "Zicntr", Required: true, Since: 0x0006000f}, // 6.15
		{Key: RISCV_HWPROBE_EXT_ZICOND, Text: "Zicond", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZIHINTPAUSE, Text: "Zihintpause", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZIHPM, Text: "Zihpm", Required: true, Since: 0x0006000f}, // 6.15
		{Key: RISCV_HWPROBE_EXT_ZVE32X, Text: "Zve32x", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVE32F, Text: "Zve32f", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVE64X, Text: "Zve64x", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVE64F, Text: "Zve64f", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVE64D, Text: "Zfe64d", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZIMOP, Text: "Zimop", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZCA, Text: "Zca", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZCB, Text: "Zcb", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZCD, Text: "Zcd", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZCF, Text: "Zcf", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZCMOP, Text: "Zcmop", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZAWRS, Text: "Zawrs", Required: true, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZAAMO, Text: "Zaamo", Required: true, Since: 0x0006000f}, // 6.15
		{Key: RISCV_HWPROBE_EXT_ZALRSC, Text: "Zalrsc", Required: true, Since: 0x0006000f}, // 6.15
		{Key: RISCV_HWPROBE_EXT_SUPM, Text: "Supm", Required: true, Since: 0x0006000d}, // 6.13
		{Key: RISCV_HWPROBE_EXT_ZFBFMIN, Text: "Zfbfmin", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVFBFMIN, Text: "Zvfbfmin", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZVFBFWMA, Text: "Zvfbfwma", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZICBOM, Text: "Zicbom", Required: true, Since: 0x0006000f}, // 6.15
		{Key: RISCV_HWPROBE_EXT_ZABHA, Text: "Zabha", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZALASR, Text: "Zalasr", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZICBOP, Text: "Zicbop", Required: true, Since: 0x00060013}, // 6.19
		{Key: RISCV_HWPROBE_EXT_ZILSD, Text: "Zilsd", Required: false, Since: 0},
		{Key: RISCV_HWPROBE_EXT_ZCLSD, Text: "Zclsd", Required: false, Since: 0},
	}

	// Check extensions
	for _, ext := range extensions {
		if probeItems[1].Value & ext.Key == 0 && ext.Required && ext.Since <= version {
			fmt.Printf("%s NOT supported\n", ext.Text)
			os.Exit(1)
		}
	}

	fmt.Println("All required extensions supported")
	os.Exit(0)
}
