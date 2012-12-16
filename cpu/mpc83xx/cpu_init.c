/*
 * Copyright (C) 2004-2007 Freescale Semiconductor, Inc.
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
#include <mpc83xx.h>
#include <ioports.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_QE
extern qe_iop_conf_t qe_iop_conf_tab[];
extern void qe_config_iopin(u8 port, u8 pin, int dir,
			 int open_drain, int assign);
extern void qe_init(uint qe_base);
extern void qe_reset(void);

static void config_qe_ioports(void)
{
	u8	port, pin;
	int	dir, open_drain, assign;
	int	i;

	for (i = 0; qe_iop_conf_tab[i].assign != QE_IOP_TAB_END; i++) {
		port		= qe_iop_conf_tab[i].port;
		pin		= qe_iop_conf_tab[i].pin;
		dir		= qe_iop_conf_tab[i].dir;
		open_drain	= qe_iop_conf_tab[i].open_drain;
		assign		= qe_iop_conf_tab[i].assign;
		qe_config_iopin(port, pin, dir, open_drain, assign);
	}
}
#endif

/*
 * Breathe some life into the CPU...
 *
 * Set up the memory map,
 * initialize a bunch of registers,
 * initialize the UPM's
 */
void cpu_init_f (volatile immap_t * im)
{
	/* Pointer is writable since we allocated a register for it */
	gd = (gd_t *) (CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_GBL_DATA_OFFSET);

	/* Clear initial global data */
	memset ((void *) gd, 0, sizeof (gd_t));

	/* system performance tweaking */

#ifdef CONFIG_SYS_ACR_PIPE_DEP
	/* Arbiter pipeline depth */
	im->arbiter.acr = (im->arbiter.acr & ~ACR_PIPE_DEP) |
			  (CONFIG_SYS_ACR_PIPE_DEP << ACR_PIPE_DEP_SHIFT);
#endif

#ifdef CONFIG_SYS_ACR_RPTCNT
	/* Arbiter repeat count */
	im->arbiter.acr = (im->arbiter.acr & ~(ACR_RPTCNT)) |
			  (CONFIG_SYS_ACR_RPTCNT << ACR_RPTCNT_SHIFT);
#endif


#ifdef CONFIG_SYS_SPCR_TSECEP
	/* all eTSEC's Emergency priority */
	im->sysconf.spcr = (im->sysconf.spcr & ~SPCR_TSECEP) |
			   (CONFIG_SYS_SPCR_TSECEP << SPCR_TSECEP_SHIFT);
#endif
	/* RSR - Reset Status Register - clear all status (4.6.1.3) */
	gd->reset_status = im->reset.rsr;
	im->reset.rsr = ~(RSR_RES);

	/* AER - Arbiter Event Register - store status */
	gd->arbiter_event_attributes = im->arbiter.aeatr;
	gd->arbiter_event_address = im->arbiter.aeadr;

	/*
	 * RMR - Reset Mode Register
	 * contains checkstop reset enable (4.6.1.4)
	 */
	im->reset.rmr = (RMR_CSRE & (1<<RMR_CSRE_SHIFT));

	/* LCRR - Clock Ratio Register (10.3.1.16) */
	im->lbus.lcrr = CONFIG_SYS_LCRR;

	/* Enable Time Base & Decrimenter ( so we will have udelay() )*/
	im->sysconf.spcr |= SPCR_TBEN;

	/* System General Purpose Register */
#ifdef CONFIG_SYS_SICRH
	im->sysconf.sicrh = CONFIG_SYS_SICRH;
#endif
#ifdef CONFIG_SYS_SICRL
	im->sysconf.sicrl = CONFIG_SYS_SICRL;
#endif
	/* DDR control driver register */
#ifdef CONFIG_SYS_DDRCDR
	im->sysconf.ddrcdr = CONFIG_SYS_DDRCDR;
#endif
	/* Output buffer impedance register */
#ifdef CONFIG_SYS_OBIR
	im->sysconf.obir = CONFIG_SYS_OBIR;
#endif

#ifdef CONFIG_QE
	/* Config QE ioports */
	config_qe_ioports();
#endif

	/*
	 * Memory Controller:
	 */

	/* Map banks 0 and 1 to the FLASH banks 0 and 1 at preliminary
	 * addresses - these have to be modified later when FLASH size
	 * has been determined
	 */

#if defined(CONFIG_SYS_BR0_PRELIM)  \
	&& defined(CONFIG_SYS_OR0_PRELIM) \
	&& defined(CONFIG_SYS_LBLAWBAR0_PRELIM) \
	&& defined(CONFIG_SYS_LBLAWAR0_PRELIM)
	im->lbus.bank[0].br = CONFIG_SYS_BR0_PRELIM;
	im->lbus.bank[0].or = CONFIG_SYS_OR0_PRELIM;
	im->sysconf.lblaw[0].bar = CONFIG_SYS_LBLAWBAR0_PRELIM;
	im->sysconf.lblaw[0].ar = CONFIG_SYS_LBLAWAR0_PRELIM;
#else
#error	CONFIG_SYS_BR0_PRELIM, CONFIG_SYS_OR0_PRELIM, CONFIG_SYS_LBLAWBAR0_PRELIM & CONFIG_SYS_LBLAWAR0_PRELIM must be defined
#endif

#if defined(CONFIG_SYS_BR1_PRELIM) && defined(CONFIG_SYS_OR1_PRELIM)
	im->lbus.bank[1].br = CONFIG_SYS_BR1_PRELIM;
	im->lbus.bank[1].or = CONFIG_SYS_OR1_PRELIM;
#endif
#if defined(CONFIG_SYS_LBLAWBAR1_PRELIM) && defined(CONFIG_SYS_LBLAWAR1_PRELIM)
	im->sysconf.lblaw[1].bar = CONFIG_SYS_LBLAWBAR1_PRELIM;
	im->sysconf.lblaw[1].ar = CONFIG_SYS_LBLAWAR1_PRELIM;
#endif
}

int cpu_init_r (void)
{
#ifdef CONFIG_QE
	uint qe_base = CONFIG_SYS_IMMR + 0x00100000; /* QE immr base */
	qe_init(qe_base);
	qe_reset();
#endif
	return 0;
}

/*
 * Print out the bus arbiter event
 */
#if defined(CONFIG_DISPLAY_AER_FULL)
static int print_83xx_arb_event(int force)
{
	static char* event[] = {
		"Address Time Out",
		"Data Time Out",
		"Address Only Transfer Type",
		"External Control Word Transfer Type",
		"Reserved Transfer Type",
		"Transfer Error",
		"reserved",
		"reserved"
	};
	static char* master[] = {
		"e300 Core Data Transaction",
		"reserved",
		"e300 Core Instruction Fetch",
		"reserved",
		"TSEC1",
		"TSEC2",
		"USB MPH",
		"USB DR",
		"Encryption Core",
		"I2C Boot Sequencer",
		"JTAG",
		"reserved",
		"eSDHC",
		"PCI1",
		"PCI2",
		"DMA",
		"QUICC Engine 00",
		"QUICC Engine 01",
		"QUICC Engine 10",
		"QUICC Engine 11",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"SATA1",
		"SATA2",
		"SATA3",
		"SATA4",
		"reserved",
		"PCI Express 1",
		"PCI Express 2",
		"TDM-DMAC"
	};
	static char *transfer[] = {
		"Address-only, Clean Block",
		"Address-only, lwarx reservation set",
		"Single-beat or Burst write",
		"reserved",
		"Address-only, Flush Block",
		"reserved",
		"Burst write",
		"reserved",
		"Address-only, sync",
		"Address-only, tlbsync",
		"Single-beat or Burst read",
		"Single-beat or Burst read",
		"Address-only, Kill Block",
		"Address-only, icbi",
		"Burst read",
		"reserved",
		"Address-only, eieio",
		"reserved",
		"Single-beat write",
		"reserved",
		"ecowx - Illegal single-beat write",
		"reserved",
		"reserved",
		"reserved",
		"Address-only, TLB Invalidate",
		"reserved",
		"Single-beat or Burst read",
		"reserved",
		"eciwx - Illegal single-beat read",
		"reserved",
		"Burst read",
		"reserved"
	};

	int etype = (gd->arbiter_event_attributes & AEATR_EVENT)
	            >> AEATR_EVENT_SHIFT;
	int mstr_id = (gd->arbiter_event_attributes & AEATR_MSTR_ID)
	              >> AEATR_MSTR_ID_SHIFT;
	int tbst = (gd->arbiter_event_attributes & AEATR_TBST)
	           >> AEATR_TBST_SHIFT;
	int tsize = (gd->arbiter_event_attributes & AEATR_TSIZE)
	            >> AEATR_TSIZE_SHIFT;
	int ttype = (gd->arbiter_event_attributes & AEATR_TTYPE)
	            >> AEATR_TTYPE_SHIFT;

	if (!force && !gd->arbiter_event_address)
		return 0;

	puts("Arbiter Event Status:\n");
	printf("       Event Address: 0x%08lX\n", gd->arbiter_event_address);
	printf("       Event Type:    0x%1x  = %s\n", etype, event[etype]);
	printf("       Master ID:     0x%02x = %s\n", mstr_id, master[mstr_id]);
	printf("       Transfer Size: 0x%1x  = %d bytes\n", (tbst<<3) | tsize,
				tbst ? (tsize ? tsize : 8) : 16 + 8 * tsize);
	printf("       Transfer Type: 0x%02x = %s\n", ttype, transfer[ttype]);

	return gd->arbiter_event_address;
}

#elif defined(CONFIG_DISPLAY_AER_BRIEF)

static int print_83xx_arb_event(int force)
{
	if (!force && !gd->arbiter_event_address)
		return 0;

	printf("Arbiter Event Status: AEATR=0x%08lX, AEADR=0x%08lX\n",
		gd->arbiter_event_attributes, gd->arbiter_event_address);

	return gd->arbiter_event_address;
}
#endif /* CONFIG_DISPLAY_AER_xxxx */

/*
 * Figure out the cause of the reset
 */
int prt_83xx_rsr(void)
{
	static struct {
		ulong mask;
		char *desc;
	} bits[] = {
		{
		RSR_SWSR, "Software Soft"}, {
		RSR_SWHR, "Software Hard"}, {
		RSR_JSRS, "JTAG Soft"}, {
		RSR_CSHR, "Check Stop"}, {
		RSR_SWRS, "Software Watchdog"}, {
		RSR_BMRS, "Bus Monitor"}, {
		RSR_SRS,  "External/Internal Soft"}, {
		RSR_HRS,  "External/Internal Hard"}
	};
	static int n = sizeof bits / sizeof bits[0];
	ulong rsr = gd->reset_status;
	int i;
	char *sep;

	puts("Reset Status:");
	puts("83xx:");
	sep = " ";
	for (i = 0; i < n; i++)
		if (rsr & bits[i].mask) {
			printf("%s%s", sep, bits[i].desc);
			sep = ", ";
		}
	puts("\n");

#if defined(CONFIG_DISPLAY_AER_FULL) || defined(CONFIG_DISPLAY_AER_BRIEF)
	print_83xx_arb_event(rsr & RSR_BMRS);
#endif
	puts("\n");

	return 0;
}