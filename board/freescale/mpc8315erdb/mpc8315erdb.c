/*
 * Copyright (C) 2007, 2009 Freescale Semiconductor, Inc.
 *
 * Author: Scott Wood <scottwood@freescale.com>
 *         Dave Liu <daveliu@freescale.com>
 *         Jerry Huang <Chang-Ming.Huang@freescale.com>
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
 * MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <i2c.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <pci.h>
#include <mpc83xx.h>
#include <netdev.h>
#include <asm/io.h>
#include <ns16550.h>
#include <nand.h>
#include <common.h>

DECLARE_GLOBAL_DATA_PTR;

int board_early_init_f(void)
{
	volatile immap_t *im = (immap_t *)CONFIG_SYS_IMMR;

	if (im->pmc.pmccr1 & PMCCR1_POWER_OFF)
		gd->flags |= GD_FLG_SILENT;

	return 0;
}

#ifndef CONFIG_NAND_SPL
static u8 read_board_info(void)
{
	u8 val8;
	i2c_set_bus_num(0);

	if (i2c_read(CONFIG_SYS_I2C_PCF8574A_ADDR, 0, 0, &val8, 1) == 0)
		return val8;
	else
		return 0;
}

int checkboard(void)
{
	static const char * const rev_str[] = {
		"0.0",
		"0.1",
		"1.0",
		"1.1",
		"<unknown>",
	};
	u8 info;
	int i;

	info = read_board_info();
	i = (!info) ? 4: info & 0x03;

	printf("Board: Freescale MPC8315ERDB Rev %s\n", rev_str[i]);

	return 0;
}

static struct pci_region pci_regions[] = {
	{
		bus_start: CONFIG_SYS_PCI_MEM_BASE,
		phys_start: CONFIG_SYS_PCI_MEM_PHYS,
		size: CONFIG_SYS_PCI_MEM_SIZE,
		flags: PCI_REGION_MEM | PCI_REGION_PREFETCH
	},
	{
		bus_start: CONFIG_SYS_PCI_MMIO_BASE,
		phys_start: CONFIG_SYS_PCI_MMIO_PHYS,
		size: CONFIG_SYS_PCI_MMIO_SIZE,
		flags: PCI_REGION_MEM
	},
	{
		bus_start: CONFIG_SYS_PCI_IO_BASE,
		phys_start: CONFIG_SYS_PCI_IO_PHYS,
		size: CONFIG_SYS_PCI_IO_SIZE,
		flags: PCI_REGION_IO
	}
};

static struct pci_region pcie_regions_0[] = {
	{
		.bus_start = CONFIG_SYS_PCIE1_MEM_BASE,
		.phys_start = CONFIG_SYS_PCIE1_MEM_PHYS,
		.size = CONFIG_SYS_PCIE1_MEM_SIZE,
		.flags = PCI_REGION_MEM,
	},
	{
		.bus_start = CONFIG_SYS_PCIE1_IO_BASE,
		.phys_start = CONFIG_SYS_PCIE1_IO_PHYS,
		.size = CONFIG_SYS_PCIE1_IO_SIZE,
		.flags = PCI_REGION_IO,
	},
};

static struct pci_region pcie_regions_1[] = {
	{
		.bus_start = CONFIG_SYS_PCIE2_MEM_BASE,
		.phys_start = CONFIG_SYS_PCIE2_MEM_PHYS,
		.size = CONFIG_SYS_PCIE2_MEM_SIZE,
		.flags = PCI_REGION_MEM,
	},
	{
		.bus_start = CONFIG_SYS_PCIE2_IO_BASE,
		.phys_start = CONFIG_SYS_PCIE2_IO_PHYS,
		.size = CONFIG_SYS_PCIE2_IO_SIZE,
		.flags = PCI_REGION_IO,
	},
};

void pci_init_board(void)
{
#ifdef CONFIG_PCISLAVE

#define PEX_CFG_BAR0 0x010
#define PEX_CFG_BAR1 0x014
#define PEX_CFG_READY 0x4B0
#define PEX_CFG_READY_OK	0x00000001
#define PEX_CFG_READY_CRS	0x00000000
#define PEX_BAR_SIZEL 0x4D8
#define PEX_BAR_SEL 0x4E0
#define PEX_EPIWTAR0 0xDE0
#define PEX_EPIWTAR1 0xDE4
#define PEX_SSVID_UPDATE 0x478

	immap_t *immr = (immap_t *)CONFIG_SYS_IMMR;
	pex83xx_t *pex;
	volatile law83xx_t *pcie_law = &(immr->sysconf.pcielaw);
	volatile sysconf83xx_t *sysconf = &immr->sysconf;
	volatile clk83xx_t *clk = (volatile clk83xx_t *)&immr->clk;

	/* Configure the clock for PCIE 2 controller */
	clrsetbits_be32(&clk->sccr, SCCR_PCIEXP2CM,SCCR_PCIEXP2CM_1);

	/* Deassert the resets in the control register */
	out_be32(&sysconf->pecr2, 0xE0000000);
	udelay(2000);

	/* Configure PCI Express 2 Local Access Windows */
	out_be32(&pcie_law[1].bar, CONFIG_SYS_PCIE2_BASE & LAWBAR_BAR);
	out_be32(&pcie_law[1].ar, LBLAWAR_EN | LBLAWAR_512MB);

	pex = &immr->pciexp[1];
	/* Enable pex csb bridge inbound & outbound transactions */
	out_le32(&pex->bridge.pex_csb_ctrl,
		in_le32(&pex->bridge.pex_csb_ctrl) | PEX_CSB_CTRL_IBPIOE | PEX_CSB_CTRL_OBPIOE);
	/* Enable bridge inbound 0 and 1*/
	out_le32(&pex->bridge.pex_csb_ibctrl, PEX_CSB_IBCTRL_PIOE);
	out_le32((void *)pex + PEX_CFG_BAR0, CONFIG_SYS_PCIE2_BASE & 0xfffff001);
	out_le32((void *)pex + PEX_BAR_SEL, 0x00000000);
	out_le32((void *)pex + PEX_BAR_SIZEL, 0xfffff000);
	out_le32((void *)pex + PEX_EPIWTAR0, 0x00000001);
	out_le32((void *)pex + PEX_CFG_BAR1, (CONFIG_SYS_PCIE2_BASE + 0x100000) & 0xfffff001);
	out_le32((void *)pex + PEX_BAR_SEL, 0x00000001);
	out_le32((void *)pex + PEX_BAR_SIZEL, 0xfff00000);
	out_le32((void *)pex + PEX_EPIWTAR1, CONFIG_SYS_IMMR | 0x00000001);

	out_le32((void *)pex + PEX_SSVID_UPDATE,
			(PCI_DEVICE_ID_MPC8315E << 16) |
			PCI_VENDOR_ID_FREESCALE);
	out_le32((void *)pex + PEX_CFG_READY, PEX_CFG_READY_OK);
#else
	volatile immap_t *immr = (volatile immap_t *)CONFIG_SYS_IMMR;
	volatile sysconf83xx_t *sysconf = &immr->sysconf;
	volatile clk83xx_t *clk = (volatile clk83xx_t *)&immr->clk;
	volatile law83xx_t *pci_law = immr->sysconf.pcilaw;
	volatile law83xx_t *pcie_law = sysconf->pcielaw;
	struct pci_region *reg[] = { pci_regions };
	struct pci_region *pcie_reg[] = { pcie_regions_0, pcie_regions_1, };
	int warmboot;

	/* Enable all 3 PCI_CLK_OUTPUTs. */
	clk->occr |= 0xe0000000;

	/*
	 * Configure PCI Local Access Windows
	 */
	pci_law[0].bar = CONFIG_SYS_PCI_MEM_PHYS & LAWBAR_BAR;
	pci_law[0].ar = LBLAWAR_EN | LBLAWAR_512MB;

	pci_law[1].bar = CONFIG_SYS_PCI_IO_PHYS & LAWBAR_BAR;
	pci_law[1].ar = LBLAWAR_EN | LBLAWAR_1MB;

	warmboot = gd->bd->bi_bootflags & BOOTFLAG_WARM;
	warmboot |= immr->pmc.pmccr1 & PMCCR1_POWER_OFF;

	mpc83xx_pci_init(1, reg, warmboot);

	/* Configure the clock for PCIE controller */
	clrsetbits_be32(&clk->sccr, SCCR_PCIEXP1CM | SCCR_PCIEXP2CM,
				    SCCR_PCIEXP1CM_1 | SCCR_PCIEXP2CM_1);

	/* Deassert the resets in the control register */
	out_be32(&sysconf->pecr1, 0xE0008000);
	out_be32(&sysconf->pecr2, 0xE0008000);
	udelay(2000);

	/* Configure PCI Express Local Access Windows */
	out_be32(&pcie_law[0].bar, CONFIG_SYS_PCIE1_BASE & LAWBAR_BAR);
	out_be32(&pcie_law[0].ar, LBLAWAR_EN | LBLAWAR_512MB);

	out_be32(&pcie_law[1].bar, CONFIG_SYS_PCIE2_BASE & LAWBAR_BAR);
	out_be32(&pcie_law[1].ar, LBLAWAR_EN | LBLAWAR_512MB);

	mpc83xx_pcie_init(2, pcie_reg, warmboot);
#endif
}

#if defined(CONFIG_OF_BOARD_SETUP)
void fdt_tsec1_fixup(void *fdt, bd_t *bd)
{
	char *mpc8315erdb = getenv("mpc8315erdb");
	const char disabled[] = "disabled";
	const char *path;
	int ret;

	if (!mpc8315erdb)
		return;

	if (!strcmp(mpc8315erdb, "tsec1")) {
		return;
	} else if (strcmp(mpc8315erdb, "ulpi")) {
		printf("WARNING: wrong `mpc8315erdb' environment "
		       "variable specified: `%s'. Should be `ulpi' "
		       "or `tsec1'.\n", mpc8315erdb);
		return;
	}

	ret = fdt_path_offset(fdt, "/aliases");
	if (ret < 0) {
		printf("WARNING: can't find /aliases node\n");
		return;
	}

	path = fdt_getprop(fdt, ret, "ethernet0", NULL);
	if (!path) {
		printf("WARNING: can't find ethernet0 alias\n");
		return;
	}

	do_fixup_by_path(fdt, path, "status", disabled, sizeof(disabled), 1);
}

void ft_board_setup(void *blob, bd_t *bd)
{
	ft_cpu_setup(blob, bd);
#ifdef CONFIG_PCI
	ft_pci_setup(blob, bd);
#endif
	fdt_fixup_dr_usb(blob, bd);
	fdt_tsec1_fixup(blob, bd);
}
#endif

int board_eth_init(bd_t *bis)
{
	cpu_eth_init(bis);	/* Initialize TSECs first */
	return pci_eth_init(bis);
}
#else
void board_init_f(ulong bootflag)
{
	volatile immap_t *immr = (volatile immap_t *)CONFIG_SYS_IMMR;
	u32 csb_clk;
	u8 clkin_div;
	u8 spmf;

	clkin_div = ((immr->clk.spmr & SPMR_CKID) >> SPMR_CKID_SHIFT);
	spmf = ((immr->reset.rcwl & HRCWL_SPMF) >> HRCWL_SPMF_SHIFT);

	if (immr->reset.rcwh & HRCWH_PCI_HOST) {
#if defined(CONFIG_83XX_CLKIN)
		csb_clk = CONFIG_83XX_CLKIN * spmf;
#else
		csb_clk = 0;
#endif /* CONFIG_83XX_CLKIN */
	} else {
#if defined(CONFIG_83XX_PCICLK)
		csb_clk = CONFIG_83XX_PCICLK * spmf * (1 + clkin_div);
#else
		csb_clk = 0;
#endif /* CONFIG_83XX_PCICLK */
	}

	board_early_init_f();
	NS16550_init((NS16550_t)CONFIG_SYS_NS16550_COM1,
	             csb_clk / 16 / CONFIG_BAUDRATE);
	puts("\nNAND boot...... ");
	init_timebase();
	initdram(0);
	relocate_code(CONFIG_SYS_NAND_U_BOOT_RELOC + 0x10000, (gd_t *)gd,
	              CONFIG_SYS_NAND_U_BOOT_RELOC);
}

void board_init_r(gd_t *gd, ulong dest_addr)
{
	nand_boot();
}

void putc(char c)
{
	if (gd->flags & GD_FLG_SILENT)
		return;

	if (c == '\n')
		NS16550_putc((NS16550_t)CONFIG_SYS_NS16550_COM1, '\r');

	NS16550_putc((NS16550_t)CONFIG_SYS_NS16550_COM1, c);
}
#endif
