#include <lightrec.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#include "cdrom.h"
#include "gteR.h"
#include "mdec.h"
#include "psxdma.h"
#include "psxhw.h"
#include "psxmem.h"
#include "r3000a.h"
#include "Gamecube/MEM2.h"

#define ARRAY_SIZE(x) (sizeof(x) ? sizeof(x) / sizeof((x)[0]) : 0)

#ifdef __GNUC__
#	define likely(x)       __builtin_expect(!!(x),1)
#	define unlikely(x)     __builtin_expect(!!(x),0)
#else
#	define likely(x)       (x)
#	define unlikely(x)     (x)
#endif

static s8 code_buffer [0x400000] __attribute__((aligned(32))); // 4 MiB code buffer for Lightrec
static struct lightrec_state *lightrec_state;

static char *name = "sd:/WiiStation/WiiStation.elf";

/* Unused for now */
u32 event_cycles[PSXINT_COUNT];
u32 next_interupt;

static bool use_lightrec_interpreter = false;
static bool use_pcsx_interpreter = false;
static bool booting;

enum my_cp2_opcodes {
	OP_CP2_RTPS		= 0x01,
	OP_CP2_NCLIP		= 0x06,
	OP_CP2_OP		= 0x0c,
	OP_CP2_DPCS		= 0x10,
	OP_CP2_INTPL		= 0x11,
	OP_CP2_MVMVA		= 0x12,
	OP_CP2_NCDS		= 0x13,
	OP_CP2_CDP		= 0x14,
	OP_CP2_NCDT		= 0x16,
	OP_CP2_NCCS		= 0x1b,
	OP_CP2_CC		= 0x1c,
	OP_CP2_NCS		= 0x1e,
	OP_CP2_NCT		= 0x20,
	OP_CP2_SQR		= 0x28,
	OP_CP2_DCPL		= 0x29,
	OP_CP2_DPCT		= 0x2a,
	OP_CP2_AVSZ3		= 0x2d,
	OP_CP2_AVSZ4		= 0x2e,
	OP_CP2_RTPT		= 0x30,
	OP_CP2_GPF		= 0x3d,
	OP_CP2_GPL		= 0x3e,
	OP_CP2_NCCT		= 0x3f,
};

static void (*cp2_ops[])(struct psxCP2Regs *) = {
	[OP_CP2_RTPS] = gteRTPS_R,
	[OP_CP2_RTPS] = gteRTPS_R,
	[OP_CP2_NCLIP] = gteNCLIP_R,
	[OP_CP2_OP] = gteOP_R,
	[OP_CP2_DPCS] = gteDPCS_R,
	[OP_CP2_INTPL] = gteINTPL_R,
	[OP_CP2_MVMVA] = gteMVMVA_R,
	[OP_CP2_NCDS] = gteNCDS_R,
	[OP_CP2_CDP] = gteCDP_R,
	[OP_CP2_NCDT] = gteNCDT_R,
	[OP_CP2_NCCS] = gteNCCS_R,
	[OP_CP2_CC] = gteCC_R,
	[OP_CP2_NCS] = gteNCS_R,
	[OP_CP2_NCT] = gteNCT_R,
	[OP_CP2_SQR] = gteSQR_R,
	[OP_CP2_DCPL] = gteDCPL_R,
	[OP_CP2_DPCT] = gteDPCT_R,
	[OP_CP2_AVSZ3] = gteAVSZ3_R,
	[OP_CP2_AVSZ4] = gteAVSZ4_R,
	[OP_CP2_RTPT] = gteRTPT_R,
	[OP_CP2_GPF] = gteGPF_R,
	[OP_CP2_GPL] = gteGPL_R,
	[OP_CP2_NCCT] = gteNCCT_R,
};

static char cache_buf[64 * 1024];

static void cop2_op(struct lightrec_state *state, u32 func)
{
	struct lightrec_registers *regs = lightrec_get_registers(state);

	psxRegs.code = func;

	if (unlikely(!cp2_ops[func & 0x3f])) {
		#ifdef SHOW_DEBUG
		fprintf(stderr, "Invalid CP2 function %u\n", func);
		#endif // SHOW_DEBUG
	} else {
		/* This works because regs->cp2c comes right after regs->cp2d,
		 * so it can be cast to a pcsxCP2Regs pointer. */
		cp2_ops[func & 0x3f]((psxCP2Regs *) regs->cp2d);
	}
}

static bool has_interrupt(void)
{
	struct lightrec_registers *regs = lightrec_get_registers(lightrec_state);

	return ((psxHu32(0x1070) & psxHu32(0x1074)) &&
		(regs->cp0[12] & 0x401) == 0x401) ||
		(regs->cp0[12] & regs->cp0[13] & 0x0300);
}

static void lightrec_restore_state(struct lightrec_state *state)
{
	lightrec_reset_cycle_count(state, psxRegs.cycle);

	if (booting || has_interrupt())
		lightrec_set_exit_flags(state, LIGHTREC_EXIT_CHECK_INTERRUPT);
	else
		lightrec_set_target_cycle_count(state, next_interupt);
}

static void hw_write_byte(struct lightrec_state *state,
			  u32 op, void *host, u32 mem, u8 val)
{
	psxRegs.cycle = lightrec_current_cycle_count(state);

	psxHwWrite8(mem, val);

	lightrec_restore_state(state);
}

static void hw_write_half(struct lightrec_state *state,
			  u32 op, void *host, u32 mem, u16 val)
{
	psxRegs.cycle = lightrec_current_cycle_count(state);

	psxHwWrite16(mem, val);

	lightrec_restore_state(state);
}

static void hw_write_word(struct lightrec_state *state,
			  u32 op, void *host, u32 mem, u32 val)
{
	psxRegs.cycle = lightrec_current_cycle_count(state);

	psxHwWrite32(mem, val);

	lightrec_restore_state(state);
}

static u8 hw_read_byte(struct lightrec_state *state, u32 op, void *host, u32 mem)
{
	u8 val;

	psxRegs.cycle = lightrec_current_cycle_count(state);

	val = psxHwRead8(mem);

	lightrec_restore_state(state);

	return val;
}

static u16 hw_read_half(struct lightrec_state *state,
			u32 op, void *host, u32 mem)
{
	u16 val;

	psxRegs.cycle = lightrec_current_cycle_count(state);

	val = psxHwRead16(mem);

	lightrec_restore_state(state);

	return val;
}

static u32 hw_read_word(struct lightrec_state *state,
			u32 op, void *host, u32 mem)
{
	u32 val;

	psxRegs.cycle = lightrec_current_cycle_count(state);

	val = psxHwRead32(mem);

	lightrec_restore_state(state);

	return val;
}

static struct lightrec_mem_map_ops hw_regs_ops = {
	.sb = hw_write_byte,
	.sh = hw_write_half,
	.sw = hw_write_word,
	.lb = hw_read_byte,
	.lh = hw_read_half,
	.lw = hw_read_word,
};

static u32 cache_ctrl;

static void cache_ctrl_write_word(struct lightrec_state *state,
				  u32 op, void *host, u32 mem, u32 val)
{
	cache_ctrl = val;
}

static u32 cache_ctrl_read_word(struct lightrec_state *state,
				u32 op, void *host, u32 mem)
{
	return cache_ctrl;
}

static struct lightrec_mem_map_ops cache_ctrl_ops = {
	.sw = cache_ctrl_write_word,
	.lw = cache_ctrl_read_word,
};

static struct lightrec_mem_map lightrec_map[] = {
	[PSX_MAP_KERNEL_USER_RAM] = {
		/* Kernel and user memory */
		.pc = 0x00000000,
		.length = 0x200000,
	},
	[PSX_MAP_BIOS] = {
		/* BIOS */
		.pc = 0x1fc00000,
		.length = 0x80000,
	},
	[PSX_MAP_SCRATCH_PAD] = {
		/* Scratch pad */
		.pc = 0x1f800000,
		.length = 0x400,
	},
	[PSX_MAP_PARALLEL_PORT] = {
		/* Parallel port */
		.pc = 0x1f000000,
		.length = 0x10000,
	},
	[PSX_MAP_HW_REGISTERS] = {
		/* Hardware registers */
		.pc = 0x1f801000,
		.length = 0x2000,
		.ops = &hw_regs_ops,
	},
	[PSX_MAP_CACHE_CONTROL] = {
		/* Cache control */
		.pc = 0x5ffe0130,
		.length = 4,
		.ops = &cache_ctrl_ops,
	},

	/* Mirrors of the kernel/user memory */
	[PSX_MAP_MIRROR1] = {
		.pc = 0x00200000,
		.length = 0x200000,
		.mirror_of = &lightrec_map[PSX_MAP_KERNEL_USER_RAM],
	},
	[PSX_MAP_MIRROR2] = {
		.pc = 0x00400000,
		.length = 0x200000,
		.mirror_of = &lightrec_map[PSX_MAP_KERNEL_USER_RAM],
	},
	[PSX_MAP_MIRROR3] = {
		.pc = 0x00600000,
		.length = 0x200000,
		.mirror_of = &lightrec_map[PSX_MAP_KERNEL_USER_RAM],
	},
	[PSX_MAP_CODE_BUFFER] = {
		.length = sizeof(code_buffer), // RECMEM2_SIZE,
	},
};

static void lightrec_enable_ram(struct lightrec_state *state, bool enable)
{
	if (enable)
		memcpy(psxM, cache_buf, sizeof(cache_buf));
	else
		memcpy(cache_buf, psxM, sizeof(cache_buf));
}

static bool lightrec_can_hw_direct(u32 kaddr, bool is_write, u8 size)
{
	switch (size) {
	case 8:
		switch (kaddr) {
		case 0x1f801040:
		case 0x1f801050:
		case 0x1f801800:
		case 0x1f801801:
		case 0x1f801802:
		case 0x1f801803:
			return false;
		default:
			return true;
		}
	case 16:
		switch (kaddr) {
		case 0x1f801040:
		case 0x1f801044:
		case 0x1f801048:
		case 0x1f80104a:
		case 0x1f80104e:
		case 0x1f801050:
		case 0x1f801054:
		case 0x1f80105a:
		case 0x1f80105e:
		case 0x1f801100:
		case 0x1f801104:
		case 0x1f801108:
		case 0x1f801110:
		case 0x1f801114:
		case 0x1f801118:
		case 0x1f801120:
		case 0x1f801124:
		case 0x1f801128:
			return false;
		case 0x1f801070:
		case 0x1f801074:
			return !is_write;
		default:
			return kaddr < 0x1f801c00 || kaddr >= 0x1f801e00;
		}
	default:
		switch (kaddr) {
		case 0x1f801040:
		case 0x1f801050:
		case 0x1f801100:
		case 0x1f801104:
		case 0x1f801108:
		case 0x1f801110:
		case 0x1f801114:
		case 0x1f801118:
		case 0x1f801120:
		case 0x1f801124:
		case 0x1f801128:
		case 0x1f801810:
		case 0x1f801814:
		case 0x1f801820:
		case 0x1f801824:
			return false;
		case 0x1f801070:
		case 0x1f801074:
		case 0x1f801088:
		case 0x1f801098:
		case 0x1f8010a8:
		case 0x1f8010b8:
		case 0x1f8010c8:
		case 0x1f8010e8:
		case 0x1f8010f4:
			return !is_write;
		default:
			return !is_write || kaddr < 0x1f801c00 || kaddr >= 0x1f801e00;
		}
	}
}

extern void DCFlushRange(void *ptr, u32 len);
extern void ICInvalidateRange(void *ptr, u32 len);

static void lightrec_code_inv(void *ptr, uint32_t len)
{
	DCFlushRange(ptr, len);
	ICInvalidateRange(ptr, len);
}

static const struct lightrec_ops lightrec_ops = {
	.cop2_op = cop2_op,
	.enable_ram = lightrec_enable_ram,
	.hw_direct = lightrec_can_hw_direct,
	.code_inv = lightrec_code_inv,
};

static int lightrec_plugin_init(void)
{
#if 0
	lightrec_map[PSX_MAP_KERNEL_USER_RAM].address = psxM;
	lightrec_map[PSX_MAP_BIOS].address = psxR;
	lightrec_map[PSX_MAP_SCRATCH_PAD].address = psxH;
	lightrec_map[PSX_MAP_PARALLEL_PORT].address = psxP;
	lightrec_map[PSX_MAP_HW_REGISTERS].address = psxH + 0x1000;
	lightrec_map[PSX_MAP_CODE_BUFFER].address = code_buffer;
	/*
	lightrec_map[PSX_MAP_MIRROR1].address = psxM + 0x200000;
	lightrec_map[PSX_MAP_MIRROR2].address = psxM + 0x400000;
	lightrec_map[PSX_MAP_MIRROR3].address = psxM + 0x600000;
	*/
#else
	lightrec_map[PSX_MAP_KERNEL_USER_RAM].address = (void *)0x0;
	lightrec_map[PSX_MAP_MIRROR1].address = (void *)0x200000;
	lightrec_map[PSX_MAP_MIRROR2].address = (void *)0x400000;
	lightrec_map[PSX_MAP_MIRROR3].address = (void *)0x600000;

	lightrec_map[PSX_MAP_BIOS].address = (void *)0x1fc00000;
	lightrec_map[PSX_MAP_SCRATCH_PAD].address = (void *)0x1f800000;
	lightrec_map[PSX_MAP_HW_REGISTERS].address = (void *)0x1f801000;
	lightrec_map[PSX_MAP_CODE_BUFFER].address = code_buffer; // RECMEM2_LO;
#endif

	lightrec_state = lightrec_init(name,
			lightrec_map, ARRAY_SIZE(lightrec_map),
			&lightrec_ops);

	signal(SIGPIPE, exit);

	return 0;
}

static void lightrec_dump_regs(struct lightrec_state *state)
{
	struct lightrec_registers *regs = lightrec_get_registers(state);

	if (unlikely(booting))
		memcpy(&psxRegs.GPR, regs->gpr, sizeof(regs->gpr));
	psxRegs.CP0.n.Status = regs->cp0[12];
	psxRegs.CP0.n.Cause = regs->cp0[13];
}

static void lightrec_restore_regs(struct lightrec_state *state)
{
	struct lightrec_registers *regs = lightrec_get_registers(state);

	if (unlikely(booting))
		memcpy(regs->gpr, &psxRegs.GPR, sizeof(regs->gpr));
	regs->cp0[12] = psxRegs.CP0.n.Status;
	regs->cp0[13] = psxRegs.CP0.n.Cause;
	regs->cp0[14] = psxRegs.CP0.n.EPC;
}

static void schedule_timeslice(void)
{
	u32 i, c = psxRegs.cycle;
	u32 irqs = psxRegs.interrupt;
	s32 min, dif;

	min = PSXCLK;
	for (i = 0; irqs != 0; i++, irqs >>= 1) {
		if (!(irqs & 1))
			continue;
		dif = event_cycles[i] - c;
		if (0 < dif && dif < min)
			min = dif;
	}
	next_interupt = c + min;
}

typedef void (irq_func)();
static void unusedInterrupt()
{
}

static irq_func * const irq_funcs[] = {
	[PSXINT_SIO]	= sioInterrupt,
	[PSXINT_CDR]	= cdrInterrupt,
	[PSXINT_CDREAD]	= cdrReadInterrupt,
	[PSXINT_GPUDMA]	= gpuInterrupt,
	[PSXINT_MDECOUTDMA] = mdec1Interrupt,
	[PSXINT_SPUDMA]	= spuInterrupt,
	//[PSXINT_MDECINDMA] = mdec0Interrupt,
	[PSXINT_GPUOTCDMA] = gpuotcInterrupt,
	[PSXINT_CDRDMA] = cdrDmaInterrupt,
	[PSXINT_CDRLID] = cdrLidSeekInterrupt,
    [PSXINT_CDRPLAY] = cdrPlayInterrupt,
	[PSXINT_SPU_UPDATE] = spuUpdate,
	[PSXINT_RCNT] = psxRcntUpdate,
};

/* local dupe of psxBranchTest, using event_cycles */
static void irq_test(void)
{
	u32 irqs = psxRegs.interrupt;
	u32 cycle = psxRegs.cycle;
	u32 irq, irq_bits;

	// irq_funcs() may queue more irqs
	psxRegs.interrupt = 0;

	for (irq = 0, irq_bits = irqs; irq_bits != 0; irq++, irq_bits >>= 1) {
		if (!(irq_bits & 1))
			continue;
		if ((s32)(cycle - event_cycles[irq]) >= 0) {
			irqs &= ~(1 << irq);
			irq_funcs[irq]();
		}
	}
	psxRegs.interrupt |= irqs;

	if ((psxHu32(0x1070) & psxHu32(0x1074)) && (psxRegs.CP0.n.Status & 0x401) == 0x401) {
		psxException(0x400, 0);
	}
}

void gen_interupt()
{
	//printf("%08x, %u->%u (%d)\r\n", psxRegs.pc, psxRegs.cycle,
	//	next_interupt, next_interupt - psxRegs.cycle);
	irq_test();
	schedule_timeslice();
}

static void lightrec_plugin_execute_internal(bool block_only)
{
	u32 flags;

	gen_interupt();

	// step during early boot so that 0x80030000 fastboot hack works
	booting = block_only;
	if (block_only)
		next_interupt = psxRegs.cycle;

	lightrec_reset_cycle_count(lightrec_state, psxRegs.cycle);
	lightrec_restore_regs(lightrec_state);

	if (unlikely(use_lightrec_interpreter)) {
		psxRegs.pc = lightrec_run_interpreter(lightrec_state,
						      psxRegs.pc, next_interupt);
	} else {
		//if(!booting)
		//	printf("Cycles %08X next_interupt %08X\r\n", psxRegs.pc, next_interupt);
		psxRegs.pc = lightrec_execute(lightrec_state,
					      psxRegs.pc, next_interupt);
	}

	psxRegs.cycle = lightrec_current_cycle_count(lightrec_state);
	lightrec_dump_regs(lightrec_state);

	flags = lightrec_exit_flags(lightrec_state);

	if (flags & LIGHTREC_EXIT_SEGFAULT) {
		#ifdef SHOW_DEBUG
		fprintf(stderr, "Exiting at cycle 0x%08x\n",
				psxRegs.cycle);
		#endif // SHOW_DEBUG
		exit(1);
	}

	if (flags & LIGHTREC_EXIT_NOMEM) {
		#ifdef SHOW_DEBUG
		fprintf(stderr, "Out of memory!\n");
		#endif // SHOW_DEBUG
		exit(1);
	}

	if (flags & LIGHTREC_EXIT_SYSCALL)
		psxException(0x20, 0);

	//if (booting && (psxRegs.pc & 0xff800000) == 0x80000000)
	//	booting = false;

	if ((psxRegs.CP0.n.Cause & psxRegs.CP0.n.Status & 0x300) &&
			(psxRegs.CP0.n.Status & 0x1)) {
		/* Handle software interrupts */
		psxRegs.CP0.n.Cause &= ~0x7c;
		psxException(psxRegs.CP0.n.Cause, 0);
	}
}

static void lightrec_plugin_execute(void)
{
	extern int stop;

	if (!booting)
		lightrec_plugin_sync_regs_from_pcsx();

	while (!stop)
		lightrec_plugin_execute_internal(false);

	lightrec_plugin_sync_regs_to_pcsx();
}

static void lightrec_plugin_execute_block(void)
{
	lightrec_plugin_execute_internal(true);
}

static void lightrec_plugin_clear(u32 addr, u32 size)
{
	//if (addr == 0 && size == UINT32_MAX)
	//	lightrec_invalidate_all(lightrec_state);
	//else
		/* size * 4: PCSX uses DMA units */
		lightrec_invalidate(lightrec_state, addr, size * 4);
}


static void lightrec_plugin_shutdown(void)
{
	lightrec_destroy(lightrec_state);
}

static void lightrec_plugin_reset(void)
{
	struct lightrec_registers *regs;

	regs = lightrec_get_registers(lightrec_state);

	/* Invalidate all blocks */
	lightrec_invalidate_all(lightrec_state);

	/* Reset registers */
	memset(regs, 0, sizeof(*regs));

	regs->cp0[12] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	regs->cp0[15] = 0x00000002; // PRevID = Revision ID, same as R3000A

	//booting = true;
}

void lightrec_plugin_sync_regs_from_pcsx(void)
{
	struct lightrec_registers *regs;

	regs = lightrec_get_registers(lightrec_state);
	memcpy(regs->cp2d, &psxRegs.CP2, sizeof(regs->cp2d) + sizeof(regs->cp2c));
	memcpy(regs->cp0, &psxRegs.CP0, sizeof(regs->cp0));
	memcpy(regs->gpr, &psxRegs.GPR, sizeof(regs->gpr));

	lightrec_invalidate_all(lightrec_state);
}

void lightrec_plugin_sync_regs_to_pcsx(void)
{
	struct lightrec_registers *regs;

	regs = lightrec_get_registers(lightrec_state);
	memcpy(&psxRegs.CP2, regs->cp2d, sizeof(regs->cp2d) + sizeof(regs->cp2c));
	memcpy(&psxRegs.CP0, regs->cp0, sizeof(regs->cp0));
	memcpy(&psxRegs.GPR, regs->gpr, sizeof(regs->gpr));
}
static void lightrec_plugin_notify(int note, void *data)
{
	/*
	To change once proper icache emulation is emulated
	switch (note)
	{
		case R3000ACPU_NOTIFY_CACHE_UNISOLATED:
			lightrec_plugin_clear(0, 0x200000/4);
			break;
		case R3000ACPU_NOTIFY_CACHE_ISOLATED:
		// Sent from psxDma3().
		case R3000ACPU_NOTIFY_DMA3_EXE_LOAD:
		default:
			break;
	}*/
}

static void lightrec_plugin_apply_config()
{
}

void new_dyna_before_save(void)
{
	psxRegs.interrupt &= ~(1 << PSXINT_RCNT); // old savestate compat

	// psxRegs.intCycle is always maintained, no need to convert
}

void new_dyna_after_save(void)
{
	psxRegs.interrupt |= 1 << PSXINT_RCNT;
}

R3000Acpu psxLightrec =
{
	lightrec_plugin_init,
	lightrec_plugin_reset,
	lightrec_plugin_execute,
	lightrec_plugin_execute_block,
	lightrec_plugin_clear,
	//lightrec_plugin_notify,
	lightrec_plugin_shutdown,
};

/* Implement the sysconf() symbol which is needed by GNU Lightning */
long sysconf(int name)
{
	switch (name) {
	case _SC_PAGE_SIZE:
		return 4096;
	default:
		return -EINVAL;
	}
}

mode_t umask(mode_t mask)
{
	return mask;
}