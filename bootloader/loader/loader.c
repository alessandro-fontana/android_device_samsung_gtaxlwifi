/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * gtaxlwifi loader, started by S-BOOT from BOOT and RECOVERY. S-BOOT keeps
 * Samsung's boot image format, limits the ramdisk to 14 MiB and ignores the
 * image's command line, so the image carries this loader as its "kernel",
 * followed by the real kernel (LZ4), the DTB, the ramdisk and the command
 * line. The loader starts Linux with them:
 *
 *   - INFORM3 back to "normal boot": S-BOOT remembers the last mode, and
 *     after one "reboot recovery" would start RECOVERY forever;
 *   - INFORM2 to "normal reset": on a warm reset S-BOOT checks it, and
 *     without it takes the reset for a charger start, going to its charger
 *     mode with the cable in and powering off without it. Any later warm
 *     reset (reboot, panic, watchdog) then starts normally; a cold start
 *     clears it;
 *   - androidboot.serialno from S-BOOT's command line;
 *   - androidboot.mode=charger on a cold start (RST_STAT) with the charger
 *     as the PMIC's power-on source. S-BOOT's own "charger" also covers a
 *     power-key start with the cable in and warm reboots after it;
 *   - the Wi-Fi and Bluetooth addresses of this tablet, read-only from
 *     Samsung's EFS, into the DTB, so one image serves every tablet;
 *   - an entropy seed: the one Android keeps in the vendor space of misc
 *     plus timing jitter, hashed;
 *   - the kernel inflated to 0x50000000 with the MMU on, the DTB at
 *     0x4a000000 with /chosen filled in, the ramdisk left in place;
 *   - MMU off, jump, as the arm64 boot protocol says.
 *
 * A bad payload sends a BOOT image to the recovery and stops a RECOVERY
 * one; with an unreadable payload the image is told apart by INFORM3, so a
 * broken RECOVERY stops instead of restarting itself. A partition table
 * without "super" (the factory table, or BOOT and RECOVERY flashed over
 * stock firmware) also goes to the recovery: Android cannot start there,
 * and the recovery installs it.
 */

#include <libfdt.h>
#include "loader.h"

#define PMU_BASE	0x10480000UL
#define PMU_SWRESET	(PMU_BASE + 0x400)
#define PMU_INFORM2	(PMU_BASE + 0x808)
#define NORMAL_RESET	0x12345678u
#define PMU_INFORM3	(PMU_BASE + 0x80c)
#define MODE_NORMAL	0x12345670u
#define MODE_RECOVERY	0x12345674u
#define PMU_RST_STAT	(PMU_BASE + 0x404)
#define RST_STAT_COLD	0x00010000u
#define PMIC_ADDR	0x66
#define PMIC_PWRONSRC	0x09
#define PWRONSRC_CHARGER	0x04
#define SEED_MAGIC	"GXSEED01"
#define SEED_LEN	32
#define SEED_SECTOR	4	/* misc vendor space, 2 KiB in (bootloader_message.h) */

#define KERNEL_ADDR	0x50000000UL
#define KERNEL_MAX	0x02a00000UL	/* tima-log is reserved at 0x52a00000 */
#define DTB_ADDR	0x4a000000UL
#define DTB_MAX		0x00100000UL
#define CMDLINE_MAX	4096

#ifndef LOADER_VERSION
#define LOADER_VERSION "gtaxl-loader"
#endif

extern char _start[];

static char cmdline[CMDLINE_MAX];
static char log_buf[512];

void loader_log(const char *tag, unsigned long v)
{
	size_t n = strlen(log_buf), t = strlen(tag);
	int i;

	if (n + t + 20 > sizeof(log_buf))
		return;
	if (n)
		log_buf[n++] = ' ';
	memcpy(log_buf + n, tag, t);
	n += t;
	log_buf[n++] = '=';
	for (i = 60; i >= 0; i -= 4) {
		if ((v >> i) == 0 && i)
			continue;
		log_buf[n++] = "0123456789abcdef"[(v >> i) & 15];
	}
	log_buf[n] = 0;
}

static inline void writel(unsigned int v, unsigned long addr)
{
	*(volatile unsigned int *)addr = v;
}

/* INFORM3 at entry asked S-BOOT for the recovery: this is RECOVERY */
static int asked_recovery;

static void __attribute__((noreturn)) fail(const struct payload *p)
{
	mmu_disable();
	if (p ? !(p->flags & PAYLOAD_RECOVERY) : !asked_recovery) {
		writel(MODE_RECOVERY, PMU_INFORM3);
		writel(1, PMU_SWRESET);
	}
	for (;;)
		asm volatile("wfi");
}

/* value of key=... in a space-separated command line, NUL-terminated copy */
static int cmdline_get(const char *line, const char *key, char *out, size_t outlen)
{
	size_t klen = strlen(key);
	const char *p = line;

	while (p && *p) {
		while (*p == ' ')
			p++;
		if (!strncmp(p, key, klen) && p[klen] == '=') {
			const char *v = p + klen + 1;
			size_t n = 0;

			while (v[n] && v[n] != ' ' && n + 1 < outlen) {
				out[n] = v[n];
				n++;
			}
			out[n] = 0;
			return n;
		}
		p = strchr(p, ' ');
	}
	return -1;
}

static const char *sboot_bootargs(const void *fdt)
{
	int node, len;

	if (!fdt || fdt_check_header(fdt))
		return NULL;
	node = fdt_path_offset(fdt, "/chosen");
	if (node < 0)
		return NULL;
	return fdt_getprop(fdt, node, "bootargs", &len);
}

/* Cold start with the charger as power-on source */
static int charger_boot(void)
{
	unsigned char v = 0;
	int err;

	if (*(volatile unsigned int *)PMU_RST_STAT != RST_STAT_COLD)
		return 0;
	err = pmic_read(PMIC_ADDR, PMIC_PWRONSRC, &v);
	return !err && v == PWRONSRC_CHARGER;
}

static unsigned long ticks(void)
{
	unsigned long v;
	asm volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(v));
	return v;
}

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	c |= 0x20;
	return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/*
 * "XX:XX:XX:XX:XX:XX" from the EFS: network order for local-mac-address,
 * reversed (little-endian) for local-bd-address.
 */
static int efs_addr(unsigned long efs, const char *path, unsigned char addr[6], int reverse)
{
	char buf[17];
	int i;

	if (ext4_read_file(efs, path, buf, sizeof(buf)) != sizeof(buf))
		return -1;
	for (i = 0; i < 6; i++) {
		int hi = hexval(buf[3 * i]), lo = hexval(buf[3 * i + 1]);

		if (hi < 0 || lo < 0 || (i < 5 && buf[3 * i + 2] != ':'))
			return -1;
		addr[reverse ? 5 - i : i] = hi << 4 | lo;
	}
	return 0;
}

static void set_addresses(void *fdt)
{
	unsigned long efs, count;
	unsigned char a[6];
	int node;

	if (gpt_find("EFS", &efs, &count)) {
		loader_log("efs", 0);
		return;
	}
	node = fdt_path_offset(fdt, "/soc@0/serial@13810000/bluetooth");
	if (node >= 0 && !efs_addr(efs, "/bluetooth/bt_addr", a, 1))
		fdt_setprop(fdt, node, "local-bd-address", a, 6);
	node = fdt_path_offset(fdt, "mmc1");
	if (node >= 0)
		node = fdt_subnode_offset(fdt, node, "wifi@1");
	if (node >= 0 && !efs_addr(efs, "/wifi/.mac.info", a, 0))
		fdt_setprop(fdt, node, "local-mac-address", a, 6);
}

/*
 * No cryptographic entropy here except the stored seed, which Android wrote
 * from its mature pool on the previous boot: the counter, the jitter of 32
 * memory walks and fixed data only tell this tablet apart.
 */
static void set_seed(void *fdt, unsigned long t_entry, const char *serial)
{
	static uint64_t pool[64];
	static unsigned char sec[512] __attribute__((aligned(8)));
	unsigned char seed[32];
	unsigned long misc, count;
	unsigned int n = 0, i, j;
	int node;

	pool[n++] = ticks();
	pool[n++] = t_entry;
	if (!gpt_find("misc", &misc, &count) && count > SEED_SECTOR &&
	    !emmc_read(misc + SEED_SECTOR, 1, sec) &&
	    !memcmp(sec, SEED_MAGIC, 8)) {
		memcpy(&pool[n], sec + 8, SEED_LEN);
		n += SEED_LEN / 8;
	}
	for (i = 0; i < 32; i++) {
		const volatile uint32_t *mem = (const volatile uint32_t *)(0x40000000UL + (i << 16));
		unsigned long t0 = ticks();
		uint32_t acc = 0;

		for (j = 0; j < 128; j++)
			acc += mem[j * 17];
		pool[n++] = (ticks() - t0) ^ ((uint64_t)acc << 32);
	}
	if (serial && *serial) {
		size_t l = strlen(serial);

		if (l > (64 - n - 1) * 8)
			l = (64 - n - 1) * 8;
		memcpy(&pool[n], serial, l);
		n += (l + 7) / 8;
	}
	pool[n++] = ticks();
	sha256(pool, n * 8, seed);

	node = fdt_path_offset(fdt, "/chosen");
	if (node >= 0)
		fdt_setprop(fdt, node, "rng-seed", seed, sizeof(seed));
	memset(seed, 0, sizeof(seed));
	memset(pool, 0, sizeof(pool));
}

static void append(const char *s)
{
	size_t have = strlen(cmdline), add = strlen(s);

	if (have + add + 2 > sizeof(cmdline))
		return;
	if (have)
		cmdline[have++] = ' ';
	memcpy(cmdline + have, s, add + 1);
}

/* The command line of the image: one line of text */
static void cmdline_from_payload(const char *text, size_t len)
{
	size_t n;

	if (len >= sizeof(cmdline))
		len = sizeof(cmdline) - 1;
	for (n = 0; n < len && text[n] != '\n' && text[n]; n++)
		cmdline[n] = text[n];
	while (n && cmdline[n - 1] == ' ')
		n--;
	cmdline[n] = 0;
}

void __attribute__((noreturn)) loader_main(const void *sboot_fdt)
{
	const struct payload *p = (const void *)(_start + PAYLOAD_OFFSET);
	const char *base = (const char *)p;
	const char *sargs;
	char val[128];
	void *fdt = (void *)DTB_ADDR;
	unsigned long t_entry = ticks();
	int chosen, err;
	long n;

	asked_recovery = *(volatile unsigned int *)PMU_INFORM3 == MODE_RECOVERY;
	writel(MODE_NORMAL, PMU_INFORM3);
	writel(NORMAL_RESET, PMU_INFORM2);

	if (memcmp(p->magic, PAYLOAD_MAGIC, 8) || p->kernel_raw_size > KERNEL_MAX ||
	    p->dtb_size > DTB_MAX / 2)
		fail(NULL);

	mmu_enable();

	if (!(p->flags & PAYLOAD_RECOVERY)) {
		unsigned long first, count;

		if (gpt_find("super", &first, &count) == -2)
			fail(p);
	}

	n = lz4_legacy_decompress(base + p->kernel_off, p->kernel_size,
				  (void *)KERNEL_ADDR, KERNEL_MAX);
	/* the arm64 Image header: image_size covers the BSS too */
	if (n != (long)p->kernel_raw_size ||
	    *(volatile unsigned int *)(KERNEL_ADDR + 56) != 0x644d5241 /* "ARM\x64" */ ||
	    *(volatile unsigned long *)(KERNEL_ADDR + 16) > KERNEL_MAX)
		fail(p);

	/* the DTB, with room for /chosen */
	err = fdt_open_into(base + p->dtb_off, fdt, DTB_MAX);
	if (err)
		fail(p);

	cmdline_from_payload(base + p->cmdline_off, p->cmdline_size);
	sargs = sboot_bootargs(sboot_fdt);
	val[0] = 0;
	if (sargs && cmdline_get(sargs, "androidboot.serialno", val, sizeof(val)) > 0) {
		char opt[160] = "androidboot.serialno=";

		memcpy(opt + strlen(opt), val, strlen(val) + 1);
		append(opt);
		fdt_setprop_string(fdt, 0, "serial-number", val);
	}
	if (!(p->flags & PAYLOAD_RECOVERY) && charger_boot())
		append("androidboot.mode=charger");
	append("androidboot.bootloader=" LOADER_VERSION);

	chosen = fdt_path_offset(fdt, "/chosen");
	if (chosen < 0)
		chosen = fdt_add_subnode(fdt, 0, "chosen");
	if (chosen < 0)
		fail(p);
	err = fdt_setprop_string(fdt, chosen, "bootargs", cmdline);
	err |= fdt_setprop_u64(fdt, chosen, "linux,initrd-start",
			       (unsigned long)(base + p->ramdisk_off));
	err |= fdt_setprop_u64(fdt, chosen, "linux,initrd-end",
			       (unsigned long)(base + p->ramdisk_off + p->ramdisk_size));
	if (err)
		fail(p);
	set_addresses(fdt);
	set_seed(fdt, t_entry, val);
	/* the properties added above move the nodes: look /chosen up again */
	chosen = fdt_path_offset(fdt, "/chosen");
	if (log_buf[0] && chosen >= 0)
		fdt_setprop_string(fdt, chosen, "gtaxl,loader-log", log_buf);
	fdt_pack(fdt);

	mmu_disable();
	jump_to_kernel(KERNEL_ADDR, DTB_ADDR);
}
