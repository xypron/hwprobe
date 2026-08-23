#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Check if the current system is RVA23U64 ready using riscv_hwprobe.
"""

import ctypes
import errno
import os
import re
import sys
from dataclasses import dataclass

# riscv_hwprobe syscall number on riscv64 Linux.
SYS_RISCV_HWPROBE = 258

RISCV_HWPROBE_KEY_BASE_BEHAVIOR = 3
RISCV_HWPROBE_BASE_BEHAVIOR_IMA = 1 << 0
RISCV_HWPROBE_KEY_IMA_EXT_0 = 4
RISCV_HWPROBE_KEY_IMA_EXT_1 = 16

RISCV_HWPROBE_IMA_FD = 1 << 0
RISCV_HWPROBE_IMA_C = 1 << 1
RISCV_HWPROBE_IMA_V = 1 << 2
RISCV_HWPROBE_EXT_ZBA = 1 << 3
RISCV_HWPROBE_EXT_ZBB = 1 << 4
RISCV_HWPROBE_EXT_ZBS = 1 << 5
RISCV_HWPROBE_EXT_ZICBOZ = 1 << 6
RISCV_HWPROBE_EXT_ZBC = 1 << 7
RISCV_HWPROBE_EXT_ZBKB = 1 << 8
RISCV_HWPROBE_EXT_ZBKC = 1 << 9
RISCV_HWPROBE_EXT_ZBKX = 1 << 10
RISCV_HWPROBE_EXT_ZKND = 1 << 11
RISCV_HWPROBE_EXT_ZKNE = 1 << 12
RISCV_HWPROBE_EXT_ZKNH = 1 << 13
RISCV_HWPROBE_EXT_ZKSED = 1 << 14
RISCV_HWPROBE_EXT_ZKSH = 1 << 15
RISCV_HWPROBE_EXT_ZKT = 1 << 16
RISCV_HWPROBE_EXT_ZVBB = 1 << 17
RISCV_HWPROBE_EXT_ZVBC = 1 << 18
RISCV_HWPROBE_EXT_ZVKB = 1 << 19
RISCV_HWPROBE_EXT_ZVKG = 1 << 20
RISCV_HWPROBE_EXT_ZVKNED = 1 << 21
RISCV_HWPROBE_EXT_ZVKNHA = 1 << 22
RISCV_HWPROBE_EXT_ZVKNHB = 1 << 23
RISCV_HWPROBE_EXT_ZVKSED = 1 << 24
RISCV_HWPROBE_EXT_ZVKSH = 1 << 25
RISCV_HWPROBE_EXT_ZVKT = 1 << 26
RISCV_HWPROBE_EXT_ZFH = 1 << 27
RISCV_HWPROBE_EXT_ZFHMIN = 1 << 28
RISCV_HWPROBE_EXT_ZIHINTNTL = 1 << 29
RISCV_HWPROBE_EXT_ZVFH = 1 << 30
RISCV_HWPROBE_EXT_ZVFHMIN = 1 << 31
RISCV_HWPROBE_EXT_ZFA = 1 << 32
RISCV_HWPROBE_EXT_ZTSO = 1 << 33
RISCV_HWPROBE_EXT_ZACAS = 1 << 34
RISCV_HWPROBE_EXT_ZICOND = 1 << 35
RISCV_HWPROBE_EXT_ZIHINTPAUSE = 1 << 36
RISCV_HWPROBE_EXT_ZVE32X = 1 << 37
RISCV_HWPROBE_EXT_ZVE32F = 1 << 38
RISCV_HWPROBE_EXT_ZVE64X = 1 << 39
RISCV_HWPROBE_EXT_ZVE64F = 1 << 40
RISCV_HWPROBE_EXT_ZVE64D = 1 << 41
RISCV_HWPROBE_EXT_ZIMOP = 1 << 42
RISCV_HWPROBE_EXT_ZCA = 1 << 43
RISCV_HWPROBE_EXT_ZCB = 1 << 44
RISCV_HWPROBE_EXT_ZCD = 1 << 45
RISCV_HWPROBE_EXT_ZCF = 1 << 46
RISCV_HWPROBE_EXT_ZCMOP = 1 << 47
RISCV_HWPROBE_EXT_ZAWRS = 1 << 48
RISCV_HWPROBE_EXT_SUPM = 1 << 49
RISCV_HWPROBE_EXT_ZICNTR = 1 << 50
RISCV_HWPROBE_EXT_ZIHPM = 1 << 51
RISCV_HWPROBE_EXT_ZFBFMIN = 1 << 52
RISCV_HWPROBE_EXT_ZVFBFMIN = 1 << 53
RISCV_HWPROBE_EXT_ZVFBFWMA = 1 << 54
RISCV_HWPROBE_EXT_ZICBOM = 1 << 55
RISCV_HWPROBE_EXT_ZAAMO = 1 << 56
RISCV_HWPROBE_EXT_ZALRSC = 1 << 57
RISCV_HWPROBE_EXT_ZABHA = 1 << 58
RISCV_HWPROBE_EXT_ZALASR = 1 << 59
RISCV_HWPROBE_EXT_ZICBOP = 1 << 60
RISCV_HWPROBE_EXT_ZILSD = 1 << 61
RISCV_HWPROBE_EXT_ZCLSD = 1 << 62
RISCV_HWPROBE_EXT_ZICFILP = 1 << 63
RISCV_HWPROBE_EXT_ZICFISS = 1 << 0
RISCV_HWPROBE_EXT_ZICCLSM = 1 << 1
RISCV_HWPROBE_EXT_ZICCAMOA = 1 << 2
RISCV_HWPROBE_EXT_ZICCIF = 1 << 3
RISCV_HWPROBE_EXT_ZICCRSE = 1 << 4
RISCV_HWPROBE_EXT_ZA64RS = 1 << 5


class RiscvHwprobe(ctypes.Structure):
    """ctypes mapping of struct riscv_hwprobe."""

    _fields_ = [
        ("key", ctypes.c_longlong),
        ("value", ctypes.c_ulonglong),
    ]


@dataclass(frozen=True)
class ExtDesc:
    """Description of one extension requirement checked via hwprobe."""

    probe_item: int
    key: int
    text: str
    required: bool
    introduced: int


def kernel_version() -> int:
    """Return kernel version encoded as (major << 16) | minor."""

    release = os.uname().release
    print(f"Kernel release {release}")

    match = re.match(r"^(\d+)\.(\d+)", release)
    if not match:
        print("Invalid kernel version string", file=sys.stderr)
        return 0

    major = int(match.group(1))
    minor = int(match.group(2))
    return (major << 16) | minor


def check_architecture() -> bool:
    """Check if running on RISC-V architecture.

    Returns:
        True if running on RISC-V, False otherwise.
    """
    machine = os.uname().machine
    if not machine.startswith("riscv"):
        print(f"Error: This tool is for RISC-V systems only.", file=sys.stderr)
        print(f"Current architecture: {machine}", file=sys.stderr)
        return False
    return True


def main() -> int:
    """Probe hwcaps and return 0 when required RVA23U64 extensions exist."""

    if not check_architecture():
        return 1

    exts = [
        ExtDesc(1, RISCV_HWPROBE_IMA_FD, "F and D", True, 0),
        ExtDesc(1, RISCV_HWPROBE_IMA_C, "C", True, 0),
        ExtDesc(1, RISCV_HWPROBE_IMA_V, "V", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZBA, "Zba", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZBB, "Zbb", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZBS, "Zbs", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZICBOZ, "Zicboz", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZBC, "Zbc", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZBKB, "Zbkb", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZBKC, "Zbkc", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZBKX, "Zbkx", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZKND, "Zknd", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZKNE, "Zkne", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZKNH, "Zknh", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZKSED, "Zksed", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZKSH, "Zksh", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZKT, "Zkt", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVBB, "Zvbb", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVBC, "Zvbc", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKB, "Zvkb", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKG, "Zvkg", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKNED, "Zvkned", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKNHA, "Zvknha", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKNHB, "Zvknhb", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKSED, "Zvksed", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKSH, "Zvksh", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVKT, "Zvkt", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZFH, "Zfh", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZFHMIN, "Zfhmin", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZIHINTNTL, "Zihintntl", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVFH, "Zvfh", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVFHMIN, "Zvfhmin", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZFA, "Zfa", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZTSO, "Ztso", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZACAS, "Zacas", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZICNTR, "Zicntr", True, 0x0006000F),  # 6.15
        ExtDesc(1, RISCV_HWPROBE_EXT_ZICOND, "Zicond", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZIHINTPAUSE, "Zihintpause", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZIHPM, "Zihpm", True, 0x0006000F),  # 6.15
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVE32X, "Zve32x", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVE32F, "Zve32f", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVE64X, "Zve64x", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVE64F, "Zve64f", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVE64D, "Zfe64d", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZIMOP, "Zimop", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZCA, "Zca", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZCB, "Zcb", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZCD, "Zcd", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZCF, "Zcf", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZCMOP, "Zcmop", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZAWRS, "Zawrs", True, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZAAMO, "Zaamo", True, 0x0006000F),  # 6.15
        ExtDesc(1, RISCV_HWPROBE_EXT_ZALRSC, "Zalrsc", True, 0x0006000F),  # 6.15
        ExtDesc(1, RISCV_HWPROBE_EXT_SUPM, "Supm", True, 0x0006000D),  # 6.13
        ExtDesc(1, RISCV_HWPROBE_EXT_ZFBFMIN, "Zfbfmin", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVFBFMIN, "Zvfbfmin", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZVFBFWMA, "Zvfbfwma", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZICBOM, "Zicbom", True, 0x0006000F),  # 6.15
        ExtDesc(1, RISCV_HWPROBE_EXT_ZABHA, "Zabha", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZALASR, "Zalasr", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZICBOP, "Zicbop", True, 0x00060013),  # 6.19
        ExtDesc(1, RISCV_HWPROBE_EXT_ZILSD, "Zilsd", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZCLSD, "Zclsd", False, 0),
        ExtDesc(1, RISCV_HWPROBE_EXT_ZICFILP, "Zicfilp", False, 0),
        ExtDesc(2, RISCV_HWPROBE_EXT_ZICFISS, "Zicfiss", False, 0),
        ExtDesc(2, RISCV_HWPROBE_EXT_ZICCLSM, "Zicclsm", True, 0x00070003),  # 7.3
        ExtDesc(2, RISCV_HWPROBE_EXT_ZICCAMOA, "Ziccamoa", True, 0x00070003),  # 7.3
        ExtDesc(2, RISCV_HWPROBE_EXT_ZICCIF, "Ziccif", True, 0x00070003),  # 7.3
        ExtDesc(2, RISCV_HWPROBE_EXT_ZICCRSE, "Ziccrse", True, 0x00070003),  # 7.3
        ExtDesc(2, RISCV_HWPROBE_EXT_ZA64RS, "Za64rs", True, 0x00070003),  # 7.3
    ]

    version = kernel_version()
    if version == 0:
        return 1

    probe_items = (RiscvHwprobe * 3)(
        RiscvHwprobe(RISCV_HWPROBE_KEY_BASE_BEHAVIOR, 0),
        RiscvHwprobe(RISCV_HWPROBE_KEY_IMA_EXT_0, 0),
        RiscvHwprobe(RISCV_HWPROBE_KEY_IMA_EXT_1, 0),
    )

    probe_item_count = 2
    if version >= 0x00070000:
        probe_item_count = 3

    libc = ctypes.CDLL(None, use_errno=True)
    ret = libc.syscall(
        ctypes.c_long(SYS_RISCV_HWPROBE),
        ctypes.byref(probe_items),
        ctypes.c_size_t(probe_item_count),
        ctypes.c_size_t(0),
        ctypes.c_void_p(0),
        ctypes.c_uint(0),
    )
    if ret != 0:
        err = ctypes.get_errno()
        err_msg = os.strerror(err) if err else "unknown error"
        print(f"sys_riscv_hwprobe failed: {err_msg}", file=sys.stderr)
        if err == errno.ENOSYS:
            print("This kernel does not support riscv_hwprobe", file=sys.stderr)
        return 1

    if (probe_items[0].value & RISCV_HWPROBE_BASE_BEHAVIOR_IMA) == 0:
        print("RISC-V base is NOT supported.")
        return 1

    for ext in exts:
        missing = ext.probe_item >= probe_item_count or (
            probe_items[ext.probe_item].value & ext.key
        ) == 0
        if missing and ext.required and ext.introduced <= version:
            print(f"{ext.text} NOT supported")
            return 1

    print("All required extensions supported")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
