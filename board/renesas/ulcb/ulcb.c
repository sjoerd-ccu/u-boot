/*
 * board/renesas/ulcb/ulcb.c
 *     This file is ULCB board support.
 *
 * Copyright (C) 2016 Renesas Electronics Corporation
 * Copyright (C) 2016 Cogent Embedded, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <netdev.h>
#include <dm.h>
#include <dm/platform_data/serial_sh.h>
#include <asm/processor.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/arch/prr_depend.h>
#include <asm/arch/gpio.h>
#include <asm/arch/rcar_gen3.h>
#include <asm/arch/rcar-mstp.h>
#include <asm/arch/sh_sdhi.h>
#include <i2c.h>
#include <mmc.h>

DECLARE_GLOBAL_DATA_PTR;

#define SCIF2_MSTP310	(1 << 10)
#define ETHERAVB_MSTP812	(1 << 12)
#define DVFS_MSTP926	(1 << 26)
#define SD0_MSTP314	(1 << 14)
#define SD1_MSTP313	(1 << 13)
#define SD2_MSTP312	(1 << 12)
#define SD3_MSTP311	(1 << 11)

#define SD0CKCR		0xE6150074
#define SD1CKCR		0xE6150078
#define SD2CKCR		0xE6150268
#define SD3CKCR		0xE615026C

int board_early_init_f(void)
{
	int freq;

	rcar_prr_init();

	/* SCIF2 */
	mstp_clrbits_le32(MSTPSR3, SMSTPCR3, SCIF2_MSTP310);
	/* EHTERAVB */
	mstp_clrbits_le32(MSTPSR8, SMSTPCR8, ETHERAVB_MSTP812);
	/* eMMC */
	mstp_clrbits_le32(MSTPSR3, SMSTPCR3, SD1_MSTP313 | SD2_MSTP312);
	/* SDHI0 */
	mstp_clrbits_le32(MSTPSR3, SMSTPCR3, SD0_MSTP314);

	freq = rcar_get_sdhi_config_clk();
	writel(freq, SD0CKCR);
	writel(freq, SD1CKCR);
	writel(freq, SD2CKCR);
	writel(freq, SD3CKCR);

#if defined(CONFIG_SYS_I2C) && defined(CONFIG_SYS_I2C_SH)
	/* DVFS for reset */
	mstp_clrbits_le32(MSTPSR9, SMSTPCR9, DVFS_MSTP926);
#endif
	return 0;
}

/* PFC */
#define PFC_PMMR	0xE6060000
#define PFC_DRVCTRL2	0xE6060308
#define PFC_DRVCTRL3	0xE606030C
#define PFC_DRVCTRL5	0xE6060314
#define PFC_DRVCTRL6	0xE6060318
#define PFC_DRVCTRL7	0xE606031C
#define PFC_DRVCTRL10	0xE6060328
#define PFC_DRVCTRL11	0xE606032C
#define PFC_DRVCTRL13	0xE6060334
#define PFC_DRVCTRL15	0xE606033C
#define PFC_DRVCTRL16	0xE6060340

DECLARE_GLOBAL_DATA_PTR;

static void write_drvctrl(u32 value, u32 modify_bit, void *reg)
{
	u32 val;

	val = readl(reg);
	val &= ~modify_bit;
	val |= value;
	writel(~val, PFC_PMMR);
	writel(val, reg);
}

int board_init(void)
{
	u32 val;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_TEXT_BASE + 0x50000;

	/* Init PFC controller */
	pinmux_init();

	/* USB1 pull-up */
	val = readl(PFC_PUEN6) | PUEN_USB1_OVC | PUEN_USB1_PWEN;
	writel(val, PFC_PUEN6);

	/* SD(SD0/SD3) driveability */
	if (rcar_need_reconfig_sdhi_drvctrl()) {
		write_drvctrl(0x00222222, 0x00777777, (void *)PFC_DRVCTRL13);
		write_drvctrl(0x00000222, 0x00000777, (void *)PFC_DRVCTRL15);
		write_drvctrl(0x22200000, 0x77700000, (void *)PFC_DRVCTRL16);
	}

#ifdef CONFIG_RAVB
	/* GPSR2 */
	gpio_request(GPIO_GFN_AVB_AVTP_CAPTURE_A, NULL);
	gpio_request(GPIO_GFN_AVB_AVTP_MATCH_A, NULL);
	gpio_request(GPIO_GFN_AVB_LINK, NULL);
	gpio_request(GPIO_GFN_AVB_PHY_INT, NULL);
	gpio_request(GPIO_GFN_AVB_MAGIC, NULL);
	gpio_request(GPIO_GFN_AVB_MDC, NULL);

	/* IPSR0 */
	gpio_request(GPIO_IFN_AVB_MDC, NULL);
	gpio_request(GPIO_IFN_AVB_MAGIC, NULL);
	gpio_request(GPIO_IFN_AVB_PHY_INT, NULL);
	gpio_request(GPIO_IFN_AVB_LINK, NULL);
	gpio_request(GPIO_IFN_AVB_AVTP_MATCH_A, NULL);
	gpio_request(GPIO_IFN_AVB_AVTP_CAPTURE_A, NULL);
	/* IPSR1 */
	gpio_request(GPIO_FN_AVB_AVTP_PPS, NULL);
	/* IPSR2 */
	gpio_request(GPIO_FN_AVB_AVTP_MATCH_B, NULL);
	/* IPSR3 */
	gpio_request(GPIO_FN_AVB_AVTP_CAPTURE_B, NULL);

	/* EtherAVB */
	write_drvctrl(0x00000333, 0x00000777, (void *)PFC_DRVCTRL2);
	write_drvctrl(0x33300000, 0x77700000, (void *)PFC_DRVCTRL3);

	/* AVB_PHY_RST */
	gpio_request(GPIO_GP_2_10, NULL);
	gpio_direction_output(GPIO_GP_2_10, 0);
	mdelay(20);
	gpio_set_value(GPIO_GP_2_10, 1);
	udelay(1);
#endif

	return 0;
}

#define MAHR	0xE68005C0
#define MALR	0xE68005C8
int board_eth_init(bd_t *bis)
{
	int ret = -ENODEV;
	u32 val;
	unsigned char enetaddr[6];

	if (!eth_getenv_enetaddr("ethaddr", enetaddr))
		return ret;

	/* Set Mac address */
	val = enetaddr[0] << 24 | enetaddr[1] << 16 |
	    enetaddr[2] << 8 | enetaddr[3];
	writel(val, MAHR);

	val = enetaddr[4] << 8 | enetaddr[5];
	writel(val, MALR);

#ifdef CONFIG_RAVB
	ret = ravb_initialize(bis);
#endif
	return ret;
}

/* ULCB has KSZ9031RNX */
int board_phy_config(struct phy_device *phydev)
{
	return 0;
}

int board_mmc_init(bd_t *bis)
{
	int ret = -ENODEV;

#ifdef CONFIG_SH_SDHI
	/* SDHI0 */
	gpio_request(GPIO_GFN_SD0_DAT0, NULL);
	gpio_request(GPIO_GFN_SD0_DAT1, NULL);
	gpio_request(GPIO_GFN_SD0_DAT2, NULL);
	gpio_request(GPIO_GFN_SD0_DAT3, NULL);
	gpio_request(GPIO_GFN_SD0_CLK, NULL);
	gpio_request(GPIO_GFN_SD0_CMD, NULL);
	gpio_request(GPIO_GFN_SD0_CD, NULL);
	gpio_request(GPIO_GFN_SD0_WP, NULL);

	gpio_request(GPIO_GP_5_2, NULL);
	gpio_request(GPIO_GP_5_1, NULL);
	gpio_direction_output(GPIO_GP_5_2, 1); /* power on */
	gpio_direction_output(GPIO_GP_5_1, 1); /* 1: 3.3V, 0: 1.8V */

	ret = sh_sdhi_init(CONFIG_SYS_SH_SDHI0_BASE, 0,
			   SH_SDHI_QUIRK_64BIT_BUF);
	if (ret)
		return ret;

	/* SDHI1/SDHI2, eMMC */
	gpio_request(GPIO_GFN_SD1_DAT0, NULL);
	gpio_request(GPIO_GFN_SD1_DAT1, NULL);
	gpio_request(GPIO_GFN_SD1_DAT2, NULL);
	gpio_request(GPIO_GFN_SD1_DAT3, NULL);
	gpio_request(GPIO_GFN_SD2_DAT0, NULL);
	gpio_request(GPIO_GFN_SD2_DAT1, NULL);
	gpio_request(GPIO_GFN_SD2_DAT2, NULL);
	gpio_request(GPIO_GFN_SD2_DAT3, NULL);
	gpio_request(GPIO_GFN_SD2_CLK, NULL);
	gpio_request(GPIO_FN_SD2_CMD, NULL);

	gpio_request(GPIO_GP_5_3, NULL);
	gpio_request(GPIO_GP_5_9, NULL);
	gpio_direction_output(GPIO_GP_5_3, 0); /* 1: 3.3V, 0: 1.8V */
	gpio_direction_output(GPIO_GP_5_9, 0); /* 1: 3.3V, 0: 1.8V */

	ret = sh_sdhi_init(CONFIG_SYS_SH_SDHI2_BASE, 1,
			   SH_SDHI_QUIRK_64BIT_BUF);
	if (ret)
		return ret;
#endif
	return ret;
}


int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
#if (CONFIG_NR_DRAM_BANKS >= 2)
	gd->ram_size += PHYS_SDRAM_2_SIZE;
#endif
#if (CONFIG_NR_DRAM_BANKS >= 3)
	gd->ram_size += PHYS_SDRAM_3_SIZE;
#endif
#if (CONFIG_NR_DRAM_BANKS >= 4)
	gd->ram_size += PHYS_SDRAM_4_SIZE;
#endif

	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
#if (CONFIG_NR_DRAM_BANKS >= 2)
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
#endif
#if (CONFIG_NR_DRAM_BANKS >= 3)
	gd->bd->bi_dram[2].start = PHYS_SDRAM_3;
	gd->bd->bi_dram[2].size = PHYS_SDRAM_3_SIZE;
#endif
#if (CONFIG_NR_DRAM_BANKS >= 4)
	gd->bd->bi_dram[3].start = PHYS_SDRAM_4;
	gd->bd->bi_dram[3].size = PHYS_SDRAM_4_SIZE;
#endif
}

const struct rcar_sysinfo sysinfo = {
	CONFIG_RCAR_BOARD_STRING
};

void reset_cpu(ulong addr)
{
#if defined(CONFIG_SYS_I2C) && defined(CONFIG_SYS_I2C_SH)
	i2c_reg_write(CONFIG_SYS_I2C_POWERIC_ADDR, 0x20, 0x80);
#endif
}

#if defined(CONFIG_DISPLAY_BOARDINFO)
int checkboard(void)
{
	printf("Board: %s\n", sysinfo.board_string);
	return 0;
}
#endif