/*
 * Copyright 2007,2009 Freescale Semiconductor, Inc.
 *
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <asm/processor.h>

extern void ft_qe_setup(void *blob);

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_BOOTCOUNT_LIMIT) && defined(CONFIG_MPC8360)
#include <asm/immap_qe.h>

void fdt_fixup_muram (void *blob)
{
	ulong data[2];

	data[0] = 0;
	data[1] = QE_MURAM_SIZE - 2 * sizeof(unsigned long);
	do_fixup_by_path(blob, "/qe/muram/data-only", "reg",
		      data, sizeof (data), 0);
}
#endif

void ft_cpu_setup(void *blob, bd_t *bd)
{
	immap_t *immr = (immap_t *)CONFIG_SYS_IMMR;
	int spridr = immr->sysconf.spridr;
	int is_837xe_rev21 = 0;
	int is_8315e_rev12 = 0;
	int is_8313e_rev21 = 0;

	/*
	 * delete crypto node if not on an E-processor
	 * initial revisions of the MPC834xE/6xE have the original SEC 2.0.
	 * EA revisions got the SEC uprevved to 2.4 but since the default device
	 * tree contains SEC 2.0 properties we uprev them here.
	 */
	if (!IS_E_PROCESSOR(spridr))
		fdt_fixup_crypto_node(blob, 0);
	else if (IS_E_PROCESSOR(spridr) &&
		 (SPR_FAMILY(spridr) == SPR_834X_FAMILY ||
		  SPR_FAMILY(spridr) == SPR_836X_FAMILY) &&
		 REVID_MAJOR(spridr) >= 2)
		fdt_fixup_crypto_node(blob, 0x0204);

	/* LFC for MPC8313E Rev2.1, MPC8315E Rev1.2 and MPC837xE Rev2.1 */
	is_837xe_rev21 = (SPR_FAMILY(spridr) == SPR_837X_FAMILY) &&
			IS_E_PROCESSOR(spridr) &&
			(REVID_MAJOR(spridr) == 2) &&
			(REVID_MINOR(spridr) == 1);
	is_8313e_rev21 = (((spridr & SPRIDR_PARTID) >> 16) == SPR_8313) &&
			IS_E_PROCESSOR(spridr) &&
			(REVID_MAJOR(spridr) == 2) &&
			(REVID_MINOR(spridr) == 1);
	is_8315e_rev12 = (((spridr & SPRIDR_PARTID) >> 16) == SPR_8315) &&
			IS_E_PROCESSOR(spridr) &&
			(REVID_MAJOR(spridr) == 1) &&
			(REVID_MINOR(spridr) == 2);
	if (is_837xe_rev21 || is_8313e_rev21 || is_8315e_rev12)
		do_fixup_by_compat_u32(blob, "gianfar", "fsl,lossless-flow-ctrl", 1, 1);

#if defined(CONFIG_HAS_ETH0) || defined(CONFIG_HAS_ETH1) ||\
    defined(CONFIG_HAS_ETH2) || defined(CONFIG_HAS_ETH3) ||\
    defined(CONFIG_HAS_ETH4) || defined(CONFIG_HAS_ETH5)
	fdt_fixup_ethernet(blob);
#endif

	do_fixup_by_prop_u32(blob, "device_type", "cpu", 4,
		"timebase-frequency", (bd->bi_busfreq / 4), 1);
	do_fixup_by_prop_u32(blob, "device_type", "cpu", 4,
		"bus-frequency", bd->bi_busfreq, 1);
	do_fixup_by_prop_u32(blob, "device_type", "cpu", 4,
		"clock-frequency", gd->core_clk, 1);
	do_fixup_by_prop_u32(blob, "device_type", "soc", 4,
		"bus-frequency", bd->bi_busfreq, 1);
	do_fixup_by_compat_u32(blob, "fsl,soc",
		"bus-frequency", bd->bi_busfreq, 1);
	do_fixup_by_compat_u32(blob, "fsl,soc",
		"clock-frequency", bd->bi_busfreq, 1);
	do_fixup_by_compat_u32(blob, "fsl,immr",
		"bus-frequency", bd->bi_busfreq, 1);
	do_fixup_by_compat_u32(blob, "fsl,immr",
		"clock-frequency", bd->bi_busfreq, 1);
#ifdef CONFIG_QE
	ft_qe_setup(blob);
#endif

#ifdef CONFIG_SYS_NS16550
	do_fixup_by_compat_u32(blob, "ns16550",
		"clock-frequency", CONFIG_SYS_NS16550_CLK, 1);
#endif

	fdt_fixup_memory(blob, (u64)bd->bi_memstart, (u64)bd->bi_memsize);

#if defined(CONFIG_BOOTCOUNT_LIMIT)
	fdt_fixup_muram (blob);
#endif
}
