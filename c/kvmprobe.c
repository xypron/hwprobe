// SPDX-License-Identifier: MIT
/*
 * Check if a KVM vcpu on this RISC-V system is RVA23 ready
 *
 * A VM and a vcpu are created, the ISA and SBI extensions and further
 * properties offered to the guest are read via KVM_GET_ONE_REG and the
 * AIA device, and everything is destroyed again.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <linux/kvm.h>
#include <asm/kvm.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#ifndef KVM_REG_SIZE_ULONG
#define KVM_REG_SIZE_ULONG \
	(sizeof(unsigned long) == 8 ? KVM_REG_SIZE_U64 : KVM_REG_SIZE_U32)
#endif

#define PROFILE_U	1
#define PROFILE_S	2
#define PROFILE_US	(PROFILE_U | PROFILE_S)

#define ZIC64B_BLOCK_SIZE	64
#define SATP_MODE_SV39		8
#define SATP_MODE_SV48		9
#define SERVER_ROWS_MAX		16
#define WRAP_COLUMN		76
#define WRAP_INDENT		4

enum ext_state {
	EXT_UNSUPPORTED,	/* host does not support the extension */
	EXT_DISABLED,		/* supported but not enabled for the vcpu */
	EXT_ENABLED,
};

struct ext_desc {
	const char *name;
	unsigned int profiles;	/* RVA23 profiles requiring the extension */
	const char *eid_range;	/* SBI only: EID range gate, not an extension */
};

/*
 * Indexed by the KVM ISA extension ID. Mandatory extensions of RVA23U64 and
 * RVA23S64 that KVM can report. Supm is covered by Ssnpm.
 */
static const struct ext_desc isa_exts[KVM_RISCV_ISA_EXT_MAX] = {
	[KVM_RISCV_ISA_EXT_A] = {"A", PROFILE_US},
	[KVM_RISCV_ISA_EXT_C] = {"C", PROFILE_US},
	[KVM_RISCV_ISA_EXT_D] = {"D", PROFILE_US},
	[KVM_RISCV_ISA_EXT_F] = {"F", PROFILE_US},
	[KVM_RISCV_ISA_EXT_H] = {"H", PROFILE_S},
	[KVM_RISCV_ISA_EXT_I] = {"I", PROFILE_US},
	[KVM_RISCV_ISA_EXT_M] = {"M", PROFILE_US},
	[KVM_RISCV_ISA_EXT_SVPBMT] = {"Svpbmt", PROFILE_S},
	[KVM_RISCV_ISA_EXT_SSTC] = {"Sstc", PROFILE_S},
	[KVM_RISCV_ISA_EXT_SVINVAL] = {"Svinval", PROFILE_S},
	[KVM_RISCV_ISA_EXT_ZIHINTPAUSE] = {"Zihintpause", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZICBOM] = {"Zicbom", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZICBOZ] = {"Zicboz", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZBB] = {"Zbb", PROFILE_US},
	[KVM_RISCV_ISA_EXT_SSAIA] = {"Ssaia", 0},
	[KVM_RISCV_ISA_EXT_V] = {"V", PROFILE_US},
	[KVM_RISCV_ISA_EXT_SVNAPOT] = {"Svnapot", PROFILE_S},
	[KVM_RISCV_ISA_EXT_ZBA] = {"Zba", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZBS] = {"Zbs", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZICNTR] = {"Zicntr", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZICSR] = {"Zicsr", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZIFENCEI] = {"Zifencei", PROFILE_S},
	[KVM_RISCV_ISA_EXT_ZIHPM] = {"Zihpm", PROFILE_US},
	[KVM_RISCV_ISA_EXT_SMSTATEEN] = {"Smstateen", PROFILE_S},
	[KVM_RISCV_ISA_EXT_ZICOND] = {"Zicond", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZBC] = {"Zbc", 0},
	[KVM_RISCV_ISA_EXT_ZBKB] = {"Zbkb", 0},
	[KVM_RISCV_ISA_EXT_ZBKC] = {"Zbkc", 0},
	[KVM_RISCV_ISA_EXT_ZBKX] = {"Zbkx", 0},
	[KVM_RISCV_ISA_EXT_ZKND] = {"Zknd", 0},
	[KVM_RISCV_ISA_EXT_ZKNE] = {"Zkne", 0},
	[KVM_RISCV_ISA_EXT_ZKNH] = {"Zknh", 0},
	[KVM_RISCV_ISA_EXT_ZKR] = {"Zkr", 0},
	[KVM_RISCV_ISA_EXT_ZKSED] = {"Zksed", 0},
	[KVM_RISCV_ISA_EXT_ZKSH] = {"Zksh", 0},
	[KVM_RISCV_ISA_EXT_ZKT] = {"Zkt", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZVBB] = {"Zvbb", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZVBC] = {"Zvbc", 0},
	[KVM_RISCV_ISA_EXT_ZVKB] = {"Zvkb", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZVKG] = {"Zvkg", 0},
	[KVM_RISCV_ISA_EXT_ZVKNED] = {"Zvkned", 0},
	[KVM_RISCV_ISA_EXT_ZVKNHA] = {"Zvknha", 0},
	[KVM_RISCV_ISA_EXT_ZVKNHB] = {"Zvknhb", 0},
	[KVM_RISCV_ISA_EXT_ZVKSED] = {"Zvksed", 0},
	[KVM_RISCV_ISA_EXT_ZVKSH] = {"Zvksh", 0},
	[KVM_RISCV_ISA_EXT_ZVKT] = {"Zvkt", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZFH] = {"Zfh", 0},
	[KVM_RISCV_ISA_EXT_ZFHMIN] = {"Zfhmin", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZIHINTNTL] = {"Zihintntl", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZVFH] = {"Zvfh", 0},
	[KVM_RISCV_ISA_EXT_ZVFHMIN] = {"Zvfhmin", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZFA] = {"Zfa", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZTSO] = {"Ztso", 0},
	[KVM_RISCV_ISA_EXT_ZACAS] = {"Zacas", 0},
	[KVM_RISCV_ISA_EXT_SSCOFPMF] = {"Sscofpmf", PROFILE_S},
	[KVM_RISCV_ISA_EXT_ZIMOP] = {"Zimop", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZCA] = {"Zca", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZCB] = {"Zcb", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZCD] = {"Zcd", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZCF] = {"Zcf", 0},
	[KVM_RISCV_ISA_EXT_ZCMOP] = {"Zcmop", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZAWRS] = {"Zawrs", PROFILE_US},
	[KVM_RISCV_ISA_EXT_SMNPM] = {"Smnpm", 0},
	[KVM_RISCV_ISA_EXT_SSNPM] = {"Ssnpm", PROFILE_US},
	[KVM_RISCV_ISA_EXT_SVADE] = {"Svade", PROFILE_S},
	[KVM_RISCV_ISA_EXT_SVADU] = {"Svadu", 0},
	[KVM_RISCV_ISA_EXT_SVVPTC] = {"Svvptc", 0},
	[KVM_RISCV_ISA_EXT_ZABHA] = {"Zabha", 0},
	[KVM_RISCV_ISA_EXT_ZICCRSE] = {"Ziccrse", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZAAMO] = {"Zaamo", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZALRSC] = {"Zalrsc", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZICBOP] = {"Zicbop", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZFBFMIN] = {"Zfbfmin", 0},
	[KVM_RISCV_ISA_EXT_ZVFBFMIN] = {"Zvfbfmin", 0},
	[KVM_RISCV_ISA_EXT_ZVFBFWMA] = {"Zvfbfwma", 0},
	[KVM_RISCV_ISA_EXT_ZCLSD] = {"Zclsd", 0},
	[KVM_RISCV_ISA_EXT_ZILSD] = {"Zilsd", 0},
	[KVM_RISCV_ISA_EXT_ZALASR] = {"Zalasr", 0},
	[KVM_RISCV_ISA_EXT_ZICFILP] = {"Zicfilp", 0},
	[KVM_RISCV_ISA_EXT_ZICFISS] = {"Zicfiss", 0},
	[KVM_RISCV_ISA_EXT_ZICCLSM] = {"Zicclsm", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZICCAMOA] = {"Ziccamoa", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZICCIF] = {"Ziccif", PROFILE_US},
	[KVM_RISCV_ISA_EXT_ZA64RS] = {"Za64rs", PROFILE_US},
};

/*
 * Indexed by the KVM SBI extension ID.
 *
 * EXPERIMENTAL and VENDOR are no SBI extensions. They switch the forwarding
 * of the reserved EID ranges 0x08000000-0x08ffffff (experimental) and
 * 0x09000000-0x09ffffff (vendor) to the VMM. They are listed separately.
 *
 * DBCN, MPXY and SUSP are default disabled and, like the EID ranges, are
 * completed by the VMM and not by KVM.
 */
static const struct ext_desc sbi_exts[KVM_RISCV_SBI_EXT_MAX] = {
	[KVM_RISCV_SBI_EXT_V01] = {"V01", 0, NULL},
	[KVM_RISCV_SBI_EXT_TIME] = {"TIME", 0, NULL},
	[KVM_RISCV_SBI_EXT_IPI] = {"IPI", 0, NULL},
	[KVM_RISCV_SBI_EXT_RFENCE] = {"RFENCE", 0, NULL},
	[KVM_RISCV_SBI_EXT_SRST] = {"SRST", 0, NULL},
	[KVM_RISCV_SBI_EXT_HSM] = {"HSM", 0, NULL},
	[KVM_RISCV_SBI_EXT_PMU] = {"PMU", 0, NULL},
	[KVM_RISCV_SBI_EXT_EXPERIMENTAL] = {"experimental", 0,
					    "0x08000000-0x08ffffff"},
	[KVM_RISCV_SBI_EXT_VENDOR] = {"vendor", 0, "0x09000000-0x09ffffff"},
	[KVM_RISCV_SBI_EXT_DBCN] = {"DBCN", 0, NULL},
	[KVM_RISCV_SBI_EXT_STA] = {"STA", 0, NULL},
	[KVM_RISCV_SBI_EXT_SUSP] = {"SUSP", 0, NULL},
	[KVM_RISCV_SBI_EXT_FWFT] = {"FWFT", 0, NULL},
	[KVM_RISCV_SBI_EXT_MPXY] = {"MPXY", 0, NULL},
};

_Static_assert((int)KVM_RISCV_SBI_EXT_MAX <= (int)KVM_RISCV_ISA_EXT_MAX,
	       "name buffers are sized for the ISA extensions");


enum cfg_id {
	CFG_MVENDORID,
	CFG_MARCHID,
	CFG_MIMPID,
	CFG_SATP_MODE,
	CFG_ZICBOM_BLOCK_SIZE,
	CFG_ZICBOZ_BLOCK_SIZE,
	CFG_ZICBOP_BLOCK_SIZE,
	CFG_COUNT,
};

struct cfg_desc {
	const char *name;
	unsigned long reg;
};

static const struct cfg_desc cfg_regs[CFG_COUNT] = {
	[CFG_MVENDORID] = {"mvendorid", KVM_REG_RISCV_CONFIG_REG(mvendorid)},
	[CFG_MARCHID] = {"marchid", KVM_REG_RISCV_CONFIG_REG(marchid)},
	[CFG_MIMPID] = {"mimpid", KVM_REG_RISCV_CONFIG_REG(mimpid)},
	[CFG_SATP_MODE] = {"satp_mode", KVM_REG_RISCV_CONFIG_REG(satp_mode)},
	[CFG_ZICBOM_BLOCK_SIZE] = {"zicbom_block_size",
				   KVM_REG_RISCV_CONFIG_REG(zicbom_block_size)},
	[CFG_ZICBOZ_BLOCK_SIZE] = {"zicboz_block_size",
				   KVM_REG_RISCV_CONFIG_REG(zicboz_block_size)},
	[CFG_ZICBOP_BLOCK_SIZE] = {"zicbop_block_size",
				   KVM_REG_RISCV_CONFIG_REG(zicbop_block_size)},
};

enum aia_id {
	AIA_MODE,
	AIA_IDS,
	AIA_SRCS,
	AIA_GROUP_BITS,
	AIA_GROUP_SHIFT,
	AIA_HART_BITS,
	AIA_GUEST_BITS,
	AIA_COUNT,
};

struct aia_desc {
	const char *name;
	unsigned long attr;
};

static const struct aia_desc aia_attrs[AIA_COUNT] = {
	[AIA_MODE] = {"mode", KVM_DEV_RISCV_AIA_CONFIG_MODE},
	[AIA_IDS] = {"identities", KVM_DEV_RISCV_AIA_CONFIG_IDS},
	[AIA_SRCS] = {"wired sources", KVM_DEV_RISCV_AIA_CONFIG_SRCS},
	[AIA_GROUP_BITS] = {"group bits",
			    KVM_DEV_RISCV_AIA_CONFIG_GROUP_BITS},
	[AIA_GROUP_SHIFT] = {"group shift",
			     KVM_DEV_RISCV_AIA_CONFIG_GROUP_SHIFT},
	[AIA_HART_BITS] = {"hart bits", KVM_DEV_RISCV_AIA_CONFIG_HART_BITS},
	[AIA_GUEST_BITS] = {"guest bits",
			    KVM_DEV_RISCV_AIA_CONFIG_GUEST_BITS},
};

struct reg_value {
	int valid;
	unsigned long value;
};

struct aia_info {
	int created;		/* in-kernel AIA irqchip device was created */
	int errnum;		/* errno of the failed device creation */
	int valid[AIA_COUNT];
	unsigned int value[AIA_COUNT];
};

struct kvmprobe_info {
	int api_version;
	enum ext_state isa[KVM_RISCV_ISA_EXT_MAX];
	enum ext_state sbi[KVM_RISCV_SBI_EXT_MAX];
	struct reg_value cfg[CFG_COUNT];
	struct aia_info aia;
};

/* System interface, replaceable by the unit tests */
struct kvm_ops {
	int (*open)(const char *path, int flags);
	int (*ioctl)(int fd, unsigned long request, void *arg);
	int (*close)(int fd);
};

struct missing_list {
	size_t count;
	const char *names[KVM_RISCV_ISA_EXT_MAX + 2];
};

static int sys_open(const char *path, int flags)
{
	return open(path, flags);
}

static int sys_ioctl(int fd, unsigned long request, void *arg)
{
	return ioctl(fd, request, arg);
}

static int sys_close(int fd)
{
	return close(fd);
}

static const struct kvm_ops sys_ops = {
	.open = sys_open,
	.ioctl = sys_ioctl,
	.close = sys_close,
};

static void print_usage(FILE *stream, const char *prog)
{
	fprintf(stream,
		"Usage: %s [-v|--verbose] [-h|--help]\n"
		"\n"
		"Create a KVM vcpu on this RISC-V system, read the ISA and SBI\n"
		"extensions and further features (AIA, IMSIC) it offers to a\n"
		"guest, destroy the vcpu, and check whether it is RVA23 ready.\n"
		"The exit status reflects the RVA23U64 verdict.\n"
		"\n"
		"Options:\n"
		"  -v, --verbose  list every extension with its state and the\n"
		"                 profiles requiring it\n"
		"  -h, --help     display this help and exit\n",
		prog);
}

static int is_riscv64(const char *machine)
{
	return strcmp(machine, "riscv64") == 0;
}

static void *int_arg(unsigned long value)
{
	return (void *)(uintptr_t)value;
}

static __u64 reg_id(__u64 type, unsigned long index)
{
	return KVM_REG_RISCV | KVM_REG_SIZE_ULONG | type | index;
}

static int get_reg(const struct kvm_ops *ops, int vcpu_fd, __u64 id,
		   unsigned long *value)
{
	struct kvm_one_reg reg = {
		.id = id,
		.addr = (__u64)(uintptr_t)value,
	};

	return ops->ioctl(vcpu_fd, KVM_GET_ONE_REG, &reg);
}

/* Read the single-extension registers of one extension class */
static int read_ext_states(const struct kvm_ops *ops, int vcpu_fd,
			   __u64 type, size_t count, enum ext_state *states)
{
	for (size_t i = 0; i < count; ++i) {
		unsigned long value = 0;

		if (get_reg(ops, vcpu_fd, reg_id(type, i), &value) < 0) {
			if (errno != ENOENT) {
				perror("KVM_GET_ONE_REG failed");
				return -1;
			}
			states[i] = EXT_UNSUPPORTED;
			continue;
		}
		states[i] = value ? EXT_ENABLED : EXT_DISABLED;
	}

	return 0;
}

static int read_cfg_regs(const struct kvm_ops *ops, int vcpu_fd,
			 struct reg_value *cfg)
{
	for (size_t i = 0; i < CFG_COUNT; ++i) {
		__u64 id = reg_id(KVM_REG_RISCV_CONFIG, cfg_regs[i].reg);

		cfg[i].valid = 0;
		if (get_reg(ops, vcpu_fd, id, &cfg[i].value) < 0) {
			/* Older kernels do not know all registers */
			if (errno != ENOENT) {
				perror("KVM_GET_ONE_REG failed");
				return -1;
			}
			continue;
		}
		cfg[i].valid = 1;
	}

	return 0;
}

static void probe_aia(const struct kvm_ops *ops, int vm_fd,
		      struct aia_info *aia)
{
	struct kvm_create_device dev = {
		.type = KVM_DEV_TYPE_RISCV_AIA,
	};

	if (ops->ioctl(vm_fd, KVM_CREATE_DEVICE, &dev) < 0) {
		aia->errnum = errno;
		return;
	}
	aia->created = 1;

	for (size_t i = 0; i < AIA_COUNT; ++i) {
		__u32 value = 0;
		struct kvm_device_attr attr = {
			.group = KVM_DEV_RISCV_AIA_GRP_CONFIG,
			.attr = aia_attrs[i].attr,
			.addr = (__u64)(uintptr_t)&value,
		};

		if (ops->ioctl(dev.fd, KVM_GET_DEVICE_ATTR, &attr) < 0)
			continue;
		aia->valid[i] = 1;
		aia->value[i] = value;
	}

	ops->close(dev.fd);
}

static int probe_vcpu(const struct kvm_ops *ops, int vm_fd, int vcpu_fd,
		      struct kvmprobe_info *info)
{
	if (read_ext_states(ops, vcpu_fd,
			    KVM_REG_RISCV_ISA_EXT | KVM_REG_RISCV_ISA_SINGLE,
			    KVM_RISCV_ISA_EXT_MAX, info->isa))
		return -1;
	if (read_ext_states(ops, vcpu_fd,
			    KVM_REG_RISCV_SBI_EXT | KVM_REG_RISCV_SBI_SINGLE,
			    KVM_RISCV_SBI_EXT_MAX, info->sbi))
		return -1;
	if (read_cfg_regs(ops, vcpu_fd, info->cfg))
		return -1;

	/* The AIA device can only be created while no vcpu has run */
	probe_aia(ops, vm_fd, &info->aia);

	return 0;
}

/*
 * Create a VM and a vcpu, collect the information, and release everything.
 * Closing the vcpu and VM file descriptors destroys them.
 */
static int kvmprobe_run(const struct kvm_ops *ops, struct kvmprobe_info *info)
{
	int kvm_fd, vm_fd, vcpu_fd;
	int ret = -1;

	memset(info, 0, sizeof(*info));

	kvm_fd = ops->open("/dev/kvm", O_RDWR | O_CLOEXEC);
	if (kvm_fd < 0) {
		perror("open /dev/kvm failed");
		return -1;
	}

	info->api_version = ops->ioctl(kvm_fd, KVM_GET_API_VERSION, NULL);
	if (info->api_version < 0) {
		perror("KVM_GET_API_VERSION failed");
		goto out_kvm;
	}
	if (info->api_version != KVM_API_VERSION) {
		fprintf(stderr, "Unsupported KVM API version %d\n",
			info->api_version);
		goto out_kvm;
	}

	if (ops->ioctl(kvm_fd, KVM_CHECK_EXTENSION,
		       int_arg(KVM_CAP_ONE_REG)) <= 0) {
		fprintf(stderr, "KVM_CAP_ONE_REG is not supported\n");
		goto out_kvm;
	}

	vm_fd = ops->ioctl(kvm_fd, KVM_CREATE_VM, int_arg(0));
	if (vm_fd < 0) {
		perror("KVM_CREATE_VM failed");
		goto out_kvm;
	}

	vcpu_fd = ops->ioctl(vm_fd, KVM_CREATE_VCPU, int_arg(0));
	if (vcpu_fd < 0) {
		perror("KVM_CREATE_VCPU failed");
		goto out_vm;
	}

	ret = probe_vcpu(ops, vm_fd, vcpu_fd, info);

	ops->close(vcpu_fd);
out_vm:
	ops->close(vm_fd);
out_kvm:
	ops->close(kvm_fd);

	return ret;
}


static const char *satp_mode_name(unsigned long mode)
{
	switch (mode) {
	case 0:
		return "Bare";
	case 1:
		return "Sv32";
	case SATP_MODE_SV39:
		return "Sv39";
	case SATP_MODE_SV48:
		return "Sv48";
	case 10:
		return "Sv57";
	case 11:
		return "Sv64";
	default:
		return "unknown";
	}
}

static const char *aia_mode_name(unsigned int mode)
{
	switch (mode) {
	case KVM_DEV_RISCV_AIA_MODE_EMUL:
		return "emulation";
	case KVM_DEV_RISCV_AIA_MODE_HWACCEL:
		return "hardware acceleration";
	case KVM_DEV_RISCV_AIA_MODE_AUTO:
		return "automatic";
	default:
		return "unknown";
	}
}

static const char *state_name(enum ext_state state)
{
	switch (state) {
	case EXT_ENABLED:
		return "enabled";
	case EXT_DISABLED:
		return "disabled";
	default:
		return "unsupported";
	}
}

/* The block sizes read as 0 if the cache block extension is unavailable */
static int block_sizes_ok(const struct kvmprobe_info *info)
{
	for (size_t i = CFG_ZICBOM_BLOCK_SIZE; i <= CFG_ZICBOP_BLOCK_SIZE; ++i) {
		if (info->cfg[i].valid && info->cfg[i].value &&
		    info->cfg[i].value != ZIC64B_BLOCK_SIZE)
			return 0;
	}

	return 1;
}

static int satp_sv39_ok(const struct kvmprobe_info *info)
{
	return !info->cfg[CFG_SATP_MODE].valid ||
	       info->cfg[CFG_SATP_MODE].value >= SATP_MODE_SV39;
}

/* List the mandatory features of a profile that the vcpu lacks */
static void find_missing(const struct kvmprobe_info *info,
			 unsigned int profile, struct missing_list *list)
{
	list->count = 0;

	for (size_t i = 0; i < KVM_RISCV_ISA_EXT_MAX; ++i) {
		if ((isa_exts[i].profiles & profile) &&
		    info->isa[i] != EXT_ENABLED)
			list->names[list->count++] = isa_exts[i].name;
	}
	if (!block_sizes_ok(info))
		list->names[list->count++] = "Zic64b";
	if ((profile & PROFILE_S) && !satp_sv39_ok(info))
		list->names[list->count++] = "Sv39";
}

static void print_names(const char *label, const char *const *names,
			size_t count)
{
	size_t col;

	if (!count)
		return;

	col = (size_t)printf("%s", label);
	for (size_t i = 0; i < count; ++i) {
		size_t len = strlen(names[i]);

		if (col + 1 + len > WRAP_COLUMN) {
			printf("\n%*s", WRAP_INDENT, "");
			col = WRAP_INDENT;
		}
		printf(" %s", names[i]);
		col += 1 + len;
	}
	printf("\n");
}

static void print_state_list(const char *label, const struct ext_desc *descs,
			     const enum ext_state *states, size_t count,
			     enum ext_state want)
{
	const char *names[KVM_RISCV_ISA_EXT_MAX];
	size_t n = 0;

	for (size_t i = 0; i < count; ++i) {
		/* EID range gates are no extensions */
		if (states[i] == want && !descs[i].eid_range)
			names[n++] = descs[i].name;
	}
	print_names(label, names, n);
}

static void print_ext_lists(const char *prefix, const char *disabled_text,
			    const struct ext_desc *descs,
			    const enum ext_state *states, size_t count,
			    int verbose)
{
	char label[96];

	snprintf(label, sizeof(label), "%s enabled:", prefix);
	print_state_list(label, descs, states, count, EXT_ENABLED);
	snprintf(label, sizeof(label), "%s %s:", prefix, disabled_text);
	print_state_list(label, descs, states, count, EXT_DISABLED);
	if (verbose) {
		snprintf(label, sizeof(label), "%s not supported:", prefix);
		print_state_list(label, descs, states, count,
				 EXT_UNSUPPORTED);
	}
}

/* The EID ranges forwarded to the VMM and the SBI extensions it completes */
static void print_sbi_forwarding(const struct kvmprobe_info *info)
{
	printf("SBI EID ranges forwarded to the VMM (no extensions):\n");
	for (size_t i = 0; i < KVM_RISCV_SBI_EXT_MAX; ++i) {
		if (!sbi_exts[i].eid_range)
			continue;
		printf("  %-12s %s: %s\n", sbi_exts[i].name,
		       sbi_exts[i].eid_range, state_name(info->sbi[i]));
	}
	printf("Calls to these ranges and to DBCN, MPXY and SUSP exit to the\n"
	       "VMM. Enabled only means that KVM forwards the call, not that\n"
	       "the VMM implements it.\n");
}

static void print_isa_table(const struct kvmprobe_info *info)
{
	printf("%-14s %-12s %-9s %s\n", "Extension", "State", "RVA23U64",
	       "RVA23S64");
	for (size_t i = 0; i < KVM_RISCV_ISA_EXT_MAX; ++i) {
		printf("%-14s %-12s %-9s %s\n", isa_exts[i].name,
		       state_name(info->isa[i]),
		       (isa_exts[i].profiles & PROFILE_U) ? "yes" : "no",
		       (isa_exts[i].profiles & PROFILE_S) ? "yes" : "no");
	}
}

static void print_cfg(const struct kvmprobe_info *info)
{
	const struct reg_value *cfg = info->cfg;

	if (cfg[CFG_MVENDORID].valid && cfg[CFG_MARCHID].valid &&
	    cfg[CFG_MIMPID].valid)
		printf("vcpu ids: mvendorid 0x%lx marchid 0x%lx mimpid 0x%lx\n",
		       cfg[CFG_MVENDORID].value, cfg[CFG_MARCHID].value,
		       cfg[CFG_MIMPID].value);
	if (cfg[CFG_SATP_MODE].valid)
		printf("Maximum satp mode: %s (%lu)\n",
		       satp_mode_name(cfg[CFG_SATP_MODE].value),
		       cfg[CFG_SATP_MODE].value);
	for (size_t i = CFG_ZICBOM_BLOCK_SIZE; i <= CFG_ZICBOP_BLOCK_SIZE; ++i) {
		if (cfg[i].valid)
			printf("%s: %lu\n", cfg_regs[i].name, cfg[i].value);
	}
}

static void print_aia(const struct kvmprobe_info *info)
{
	const struct aia_info *aia = &info->aia;

	printf("AIA:\n");
	printf("  Ssaia extension: %s\n",
	       state_name(info->isa[KVM_RISCV_ISA_EXT_SSAIA]));
	if (!aia->created) {
		printf("  In-kernel AIA irqchip: not available (%s)\n",
		       strerror(aia->errnum));
		return;
	}
	printf("  In-kernel AIA irqchip: available\n");
	if (aia->valid[AIA_MODE]) {
		if (aia->value[AIA_MODE] == KVM_DEV_RISCV_AIA_MODE_EMUL)
			printf("  IMSIC: trap-and-emulate, no guest interrupt files\n");
		else
			printf("  IMSIC: guest interrupt files, hardware accelerated\n");
	}
	for (size_t i = 0; i < AIA_COUNT; ++i) {
		if (!aia->valid[i])
			continue;
		if (i == AIA_MODE)
			printf("  default %s: %s\n", aia_attrs[i].name,
			       aia_mode_name(aia->value[i]));
		else
			printf("  default %s: %u\n", aia_attrs[i].name,
			       aia->value[i]);
	}
}

static int print_verdict(const struct kvmprobe_info *info, const char *name,
			 unsigned int profile)
{
	struct missing_list list;

	find_missing(info, profile, &list);
	for (size_t i = 0; i < list.count; ++i)
		printf("%s NOT enabled\n", list.names[i]);
	printf("Verdict: %s %s\n", name,
	       list.count ? "NOT supported" : "supported");

	return (int)list.count;
}

enum server_result {
	SRV_PASS,
	SRV_FAIL,	/* MUST rule not met */
	SRV_WARN,	/* SHOULD rule not met */
	SRV_NA,		/* not observable via KVM */
};

struct server_row {
	const char *id;
	const char *item;
	int must;
	enum server_result result;
	const char *detail;
};

static enum server_result server_result(int ok, int must)
{
	if (ok)
		return SRV_PASS;

	return must ? SRV_FAIL : SRV_WARN;
}

static size_t add_server_row(struct server_row *rows, size_t n,
			     const char *id, const char *item, int must,
			     enum server_result result, const char *detail)
{
	rows[n] = (struct server_row){id, item, must, result, detail};

	return n + 1;
}

static size_t add_server_isa_row(const struct kvmprobe_info *info,
				 struct server_row *rows, size_t n,
				 const char *id, int ext, int must)
{
	enum ext_state state = info->isa[ext];

	return add_server_row(rows, n, id, isa_exts[ext].name, must,
			      server_result(state == EXT_ENABLED, must),
			      state_name(state));
}

static size_t add_server_sv48_row(const struct kvmprobe_info *info,
				  struct server_row *rows, size_t n)
{
	const struct reg_value *satp = &info->cfg[CFG_SATP_MODE];

	if (!satp->valid)
		return add_server_row(rows, n, "RVA_020", "Sv48", 1, SRV_NA,
				      "satp_mode not reported");

	return add_server_row(rows, n, "RVA_020", "Sv48", 1,
			      server_result(satp->value >= SATP_MODE_SV48, 1),
			      satp_mode_name(satp->value));
}

/*
 * Check the hart rules (RVA_010 to RVA_040) of the RISC-V Server Platform
 * Requirements that KVM can show. Returns the number of rows filled.
 */
static size_t check_server(const struct kvmprobe_info *info,
			   struct server_row *rows)
{
	static const char *const unobservable[] = {
		"Sdtrig", "Sdext", "Ssccfg", "Ssstrict",
	};
	struct missing_list list;
	size_t n = 0;

	find_missing(info, PROFILE_S, &list);
	n = add_server_row(rows, n, "RVA_010", "RVA23S64", 1,
			   server_result(!list.count, 1),
			   list.count ? "see RVA23S64 verdict" :
					"all mandatory extensions enabled");
	n = add_server_sv48_row(info, rows, n);
	n = add_server_isa_row(info, rows, n, "RVA_020",
			       KVM_RISCV_ISA_EXT_ZKR, 1);
	n = add_server_isa_row(info, rows, n, "RVA_020",
			       KVM_RISCV_ISA_EXT_SSAIA, 1);
	for (size_t i = 0; i < ARRAY_SIZE(unobservable); ++i)
		n = add_server_row(rows, n, "RVA_020", unobservable[i], 1,
				   SRV_NA, "not reported by KVM");
	n = add_server_isa_row(info, rows, n, "RVA_030",
			       KVM_RISCV_ISA_EXT_SVADU, 0);
	n = add_server_row(rows, n, "RVA_040", "Ssctr", 0, SRV_NA,
			   "not reported by KVM");

	return n;
}

static const char *server_result_name(enum server_result result)
{
	switch (result) {
	case SRV_PASS:
		return "PASS";
	case SRV_FAIL:
		return "FAIL";
	case SRV_WARN:
		return "WARN";
	default:
		return "N/A";
	}
}

/* Returns the number of MUST rules not met */
static int print_server(const struct kvmprobe_info *info)
{
	struct server_row rows[SERVER_ROWS_MAX];
	size_t n = check_server(info, rows);
	int failed = 0;

	printf("RISC-V server platform requirements (harts):\n");
	printf("%-8s %-10s %-7s %-6s %s\n", "Rule", "Item", "Level", "Result",
	       "Detail");
	for (size_t i = 0; i < n; ++i) {
		printf("%-8s %-10s %-7s %-6s %s\n", rows[i].id, rows[i].item,
		       rows[i].must ? "MUST" : "SHOULD",
		       server_result_name(rows[i].result), rows[i].detail);
		if (rows[i].result == SRV_FAIL)
			++failed;
	}
	printf("Not observable via KVM: RVA_050, RVA_060, SEE_010, SEE_020,\n"
	       "FIRM_020 (SBI DBTR), FIRM_030 (SBI SSE), FIRM_140 (RIMT).\n");
	printf("Verdict: server platform (observable rules) %s\n",
	       failed ? "NOT supported" : "supported");

	return failed;
}

static void print_report(const struct kvmprobe_info *info, int verbose)
{
	printf("KVM API version %d\n", info->api_version);
	print_cfg(info);
	printf("\n");

	if (verbose)
		print_isa_table(info);
	else
		print_ext_lists("ISA extensions", "available, not enabled",
				isa_exts, info->isa, KVM_RISCV_ISA_EXT_MAX, 0);
	printf("\n");
	print_ext_lists("SBI extensions",
			"available, off by default (VMM must enable)",
			sbi_exts, info->sbi, KVM_RISCV_SBI_EXT_MAX, verbose);
	printf("\n");
	print_sbi_forwarding(info);
	printf("\n");
	print_aia(info);
	printf("\n");
}


#ifndef KVMPROBE_TEST
int main(int argc, char *argv[])
{
	static const struct option long_opts[] = {
		{"verbose", no_argument, NULL, 'v'},
		{"help",    no_argument, NULL, 'h'},
		{NULL,      0,           NULL, 0},
	};
	struct kvmprobe_info info;
	struct utsname uts;
	int verbose = 0;
	int opt;
	int missing_u;

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

	if (uname(&uts)) {
		perror("uname failed");
		return EXIT_FAILURE;
	}
	if (!is_riscv64(uts.machine)) {
		fprintf(stderr, "%s requires a riscv64 system, found %s\n",
			argv[0], uts.machine);
		return EXIT_FAILURE;
	}

	if (kvmprobe_run(&sys_ops, &info))
		return EXIT_FAILURE;

	print_report(&info, verbose);
	missing_u = print_verdict(&info, "RVA23U64", PROFILE_U);
	print_verdict(&info, "RVA23S64", PROFILE_S);
	printf("\n");
	print_server(&info);

	if (verbose)
		printf("\nNot checked, KVM does not report them: Ssccptr, Sstvecd,\n"
		       "Sstvala, Sscounterenw, Ssu64xl, Svbare, Shcounterenw,\n"
		       "Shvstvala, Shtvala, Shvstvecd, Shvsatpa, Shgatpa.\n");

	return missing_u ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
