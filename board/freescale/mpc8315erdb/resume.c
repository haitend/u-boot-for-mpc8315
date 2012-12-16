/*
 * Copyright (C) 2008 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Description: PCI and PCIe deep sleep resume.
 *
 * Author: Dave Liu <daveliu@freescale.com>
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
#include <mpc83xx.h>
#include <pci.h>
#include <asm/io.h>
#include <asm/fsl_serdes.h>

static struct pci_region pcie_reg_0[] = {
	{
		bus_start: CONFIG_SYS_PCIE1_MEM_BASE,
		phys_start: CONFIG_SYS_PCIE1_MEM_PHYS,
		size: CONFIG_SYS_PCIE1_MEM_SIZE,
		flags: PCI_REGION_MEM
	},
	{
		bus_start: CONFIG_SYS_PCIE1_IO_BASE,
		phys_start: CONFIG_SYS_PCIE1_IO_PHYS,
		size: CONFIG_SYS_PCIE1_IO_SIZE,
		flags: PCI_REGION_IO
	}
};

static struct pci_region pcie_reg_1[] = {
	{
		bus_start: CONFIG_SYS_PCIE2_MEM_BASE,
		phys_start: CONFIG_SYS_PCIE2_MEM_PHYS,
		size: CONFIG_SYS_PCIE2_MEM_SIZE,
		flags: PCI_REGION_MEM
	},
	{
		bus_start: CONFIG_SYS_PCIE2_IO_BASE,
		phys_start: CONFIG_SYS_PCIE2_IO_PHYS,
		size: CONFIG_SYS_PCIE2_IO_SIZE,
		flags: PCI_REGION_IO
	}
};

static void restore_pci_config(void)
{
	volatile immap_t *immr = (volatile immap_t *)CONFIG_SYS_IMMR;
	volatile law83xx_t *pci_law = immr->sysconf.pcilaw;
	volatile pot83xx_t *pot = immr->ios.pot;
	volatile pcictrl83xx_t *pci_ctrl = &immr->pci_ctrl;

	/* PCI LAW */
	pci_law[0].bar =  CONFIG_SYS_PCI_MEM_PHYS & LAWBAR_BAR;
	pci_law[0].ar = LBLAWAR_EN | LBLAWAR_512MB;
	pci_law[1].bar =  CONFIG_SYS_PCI_IO_PHYS & LAWBAR_BAR;
	pci_law[1].ar = LBLAWAR_EN | LBLAWAR_1MB;

	/* outbound */
	pot->potar = 0x00080000;
	pot->pobar = 0x00080000;
	pot->pocmr = 0x800F0000;
	pot++;

	pot->potar = 0x00090000;
	pot->pobar = 0x00090000;
	pot->pocmr = 0x800F0000;
	pot++;

	pot->potar = 0x00000000;
	pot->pobar = 0x000E0300;
	pot->pocmr = 0xC00FFF00;

	/* inbound */
	pci_ctrl->pitar1 = 0;
	pci_ctrl->pibar1 = 0;
	pci_ctrl->piebar1 = 0;
	pci_ctrl->piwar1 = 0xA005501A;

	asm ("sync");
	/* out of PCI reset */
	pci_ctrl->gcr = 1;
	asm ("sync");
}

static void restore_serdes_config(void)
{
	volatile immap_t *immr = (volatile immap_t *) CONFIG_SYS_IMMR;
	volatile serdes83xx_t *sd = &immr->serdes[1]; /* should be [0] */
	sd->srdscr1 = 0x00000040;
	sd->srdscr2 = 0x00800000;
	sd->srdscr3 = 0x01010000;
	sd->srdscr4 = 0x00000101;
#define FSL_SRDSRSTCTL_RST		0x80000000
	sd->srdsrstctl |= FSL_SRDSRSTCTL_RST;
	asm("sync");
}

static void restore_pcie_config(int bus)
{
	volatile immap_t *immr = (volatile immap_t *) CONFIG_SYS_IMMR;
	volatile law83xx_t *pcie_law = immr->sysconf.pcielaw;
	volatile pex83xx_t *pex = &immr->pciexp[bus];
	volatile struct pex_outbound_window *out_win;
	volatile struct pex_inbound_window *in_win;
	volatile void *hose_cfg_base;
	struct pci_region *pcie_reg[] = { pcie_reg_0, pcie_reg_1 };
	struct pci_region *reg;
	unsigned int ram_sz, barl, tar;
	u16 reg16;
	int i, j;

	if (!bus) {
		/* out of reset */
		immr->sysconf.pecr1 = 0xE0008000;
		/* PEX LAW */
		pcie_law[0].bar =  CONFIG_SYS_PCIE1_BASE & LAWBAR_BAR;
		pcie_law[0].ar = LBLAWAR_EN | LBLAWAR_512MB;
	} else {
		immr->sysconf.pecr2 = 0xE0008000;
		pcie_law[1].bar =  CONFIG_SYS_PCIE2_BASE & LAWBAR_BAR;
		pcie_law[1].ar = LBLAWAR_EN | LBLAWAR_512MB;
	}

	/* Enable in/outbound PIO */
	out_le32(&pex->bridge.pex_csb_ctrl,
		in_le32(&pex->bridge.pex_csb_ctrl) | PEX_CSB_CTRL_OBPIOE |
			PEX_CSB_CTRL_IBPIOE);

	/* Enable outbound */
	out_le32(&pex->bridge.pex_csb_obctrl, PEX_CSB_OBCTRL_PIOE |
		PEX_CSB_OBCTRL_MEMWE | PEX_CSB_OBCTRL_IOWE |
		PEX_CSB_OBCTRL_CFGWE);

	/* outbound win -cfg */
	out_win = &pex->bridge.pex_outbound_win[0];
	if (bus) {
		out_le32(&out_win->ar, PEX_OWAR_EN | PEX_OWAR_TYPE_CFG |
			 CONFIG_SYS_PCIE2_CFG_SIZE);
		out_le32(&out_win->bar,  CONFIG_SYS_PCIE2_CFG_BASE);
	} else {
		out_le32(&out_win->ar, PEX_OWAR_EN | PEX_OWAR_TYPE_CFG |
			 CONFIG_SYS_PCIE1_CFG_SIZE);
		out_le32(&out_win->bar,  CONFIG_SYS_PCIE1_CFG_BASE);
	}
	out_le32(&out_win->tarl, 0);
	out_le32(&out_win->tarh, 0);

	reg = pcie_reg[bus];
	/* outbound win */
	for (i = 0; i < 2; i++, reg++) {
		u32 ar;
		if (reg->size == 0)
			break;

		out_win = &pex->bridge.pex_outbound_win[i + 1];
		out_le32(&out_win->bar, reg->phys_start);
		out_le32(&out_win->tarl, reg->bus_start);
		out_le32(&out_win->tarh, 0);
		ar = PEX_OWAR_EN | (reg->size & PEX_OWAR_SIZE);
		if (reg->flags & PCI_REGION_IO)
			ar |= PEX_OWAR_TYPE_IO;
		else
			ar |= PEX_OWAR_TYPE_MEM;
		out_le32(&out_win->ar, ar);
	}

	/* Enable inbound win */
	out_le32(&pex->bridge.pex_csb_ibctrl, PEX_CSB_IBCTRL_PIOE);

	/* inbound win */
	ram_sz = CONFIG_SYS_DDR_SIZE << 20;
	barl = 0;
	tar = 0;
	j = 0;
	while (ram_sz > 0) {
		in_win = &pex->bridge.pex_inbound_win[j];
		out_le32(&in_win->barl, barl);
		out_le32(&in_win->barh, 0x0);
		out_le32(&in_win->tar, tar);
		if (ram_sz >= 0x10000000) {
			out_le32(&in_win->ar, PEX_IWAR_EN | PEX_IWAR_NSOV |
				PEX_IWAR_TYPE_PF | 0x0FFFF000);
			barl += 0x10000000;
			tar += 0x10000000;
			ram_sz -= 0x10000000;
		} else {
			/* The UM  is not clear here.
			 * So, round up to even Mb boundary */
			ram_sz = ram_sz >> 20 +
					((ram_sz & 0xFFFFF) ? 1 : 0);
			if (!(ram_sz % 2))
				ram_sz -= 1;
			out_le32(&in_win->ar, PEX_IWAR_EN | PEX_IWAR_NSOV |
				PEX_IWAR_TYPE_PF | (ram_sz << 20) | 0xFF000);
			ram_sz = 0;
		}
		j++;
	}

	in_win = &pex->bridge.pex_inbound_win[j];
	out_le32(&in_win->barl, CONFIG_SYS_IMMR);
	out_le32(&in_win->barh, 0);
	out_le32(&in_win->tar, CONFIG_SYS_IMMR);
	out_le32(&in_win->ar, PEX_IWAR_EN |
		PEX_IWAR_TYPE_NO_PF | PEX_IWAR_SIZE_1M);

	/* Enable the host virtual INTX interrupts */
	out_le32(&pex->bridge.pex_int_axi_misc_enb,
		in_le32(&pex->bridge.pex_int_axi_misc_enb) | 0x1E0);

	/* Hose configure header is memory-mapped */
	hose_cfg_base = (void *)pex;

	/* Configure the PCIE controller core clock ratio */
	out_le32(hose_cfg_base + PEX_GCLK_RATIO, 0x00000006);

	/* Do Type 1 bridge configuration */
	out_8(hose_cfg_base + PCI_PRIMARY_BUS, 0);
	out_8(hose_cfg_base + PCI_SECONDARY_BUS, 1);
	out_8(hose_cfg_base + PCI_SUBORDINATE_BUS, 255);

	/* Write to Command register */
	reg16 = in_le16(hose_cfg_base + PCI_COMMAND);
	reg16 |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO |
			PCI_COMMAND_SERR | PCI_COMMAND_PARITY;
	out_le16(hose_cfg_base + PCI_COMMAND, reg16);
}

void resume_system_config(void)
{
	volatile immap_t *immr = (volatile immap_t *)CONFIG_SYS_IMMR;

	restore_pci_config();

	/* reset PEX controller */
	immr->sysconf.pecr1 = 0;
	immr->sysconf.pecr2 = 0;
	asm("sync");

	restore_serdes_config();
	restore_pcie_config(0);
	restore_pcie_config(1);
}
