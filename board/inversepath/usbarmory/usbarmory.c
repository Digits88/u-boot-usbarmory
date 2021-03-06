/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc.
 * Jason Liu <r64343@freescale.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * Adapted from freescale/mx53loco/mx53loco.c by Andrej Rosano <andrej@inversepath.com>
 * for USB armory (inversepath.com/usbarmory) hardware
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/iomux-mx53.h>
#include <asm/errno.h>
#include <asm/imx-common/mx5_video.h>
#include <netdev.h>
#include <i2c.h>
#include <mmc.h>
#include <fsl_esdhc.h>
#include <asm/gpio.h>
#include <power/pmic.h>
#include <linux/fb.h>
#include <ipu_pixfmt.h>

DECLARE_GLOBAL_DATA_PTR;

static uint32_t mx53_dram_size[2];

phys_size_t get_effective_memsize(void)
{
	/*
	 * WARNING: We must override get_effective_memsize() function here
	 * to report only the size of the first DRAM bank. This is to make
	 * U-Boot relocator place U-Boot into valid memory, that is, at the
	 * end of the first DRAM bank. If we did not override this function
	 * like so, U-Boot would be placed at the address of the first DRAM
	 * bank + total DRAM size - sizeof(uboot), which in the setup where
	 * each DRAM bank contains 512MiB of DRAM would result in placing
	 * U-Boot into invalid memory area close to the end of the first
	 * DRAM bank.
	 */
	return mx53_dram_size[0];
}

int dram_init(void)
{
	mx53_dram_size[0] = get_ram_size((void *)PHYS_SDRAM_1, 1 << 30);

	gd->ram_size = mx53_dram_size[0] + mx53_dram_size[1];

	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = mx53_dram_size[0];
}

u32 get_board_rev(void)
{
	struct iim_regs *iim = (struct iim_regs *)IMX_IIM_BASE;
	struct fuse_bank *bank = &iim->bank[0];
	struct fuse_bank0_regs *fuse =
		(struct fuse_bank0_regs *)bank->fuse_regs;

	int rev = readl(&fuse->gp[6]);

	return (get_cpu_rev() & ~(0xF << 8)) | (rev & 0xF) << 8;
}

#define UART_PAD_CTRL	(PAD_CTL_HYS | PAD_CTL_DSE_HIGH | \
			 PAD_CTL_PUS_100K_UP | PAD_CTL_ODE)

static void setup_iomux_uart(void)
{
	static const iomux_v3_cfg_t uart_pads[] = {
		NEW_PAD_CTRL(MX53_PAD_CSI0_DAT11__UART1_RXD_MUX, UART_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_CSI0_DAT10__UART1_TXD_MUX, UART_PAD_CTRL),
	};

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
}

#ifdef CONFIG_USB_EHCI_MX5
int board_ehci_hcd_init(int port)
{
	/* request VBUS power enable pin, GPIO7_8 */
	imx_iomux_v3_setup_pad(MX53_PAD_PATA_DA_2__GPIO7_8);
	gpio_direction_output(IMX_GPIO_NR(7, 8), 1);
	return 0;
}
#endif

#ifdef CONFIG_FSL_ESDHC
struct fsl_esdhc_cfg esdhc_cfg[2] = {
	{MMC_SDHC1_BASE_ADDR},
	{MMC_SDHC3_BASE_ADDR},
};

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret;

	imx_iomux_v3_setup_pad(MX53_PAD_EIM_DA11__GPIO3_11);
	gpio_direction_input(IMX_GPIO_NR(3, 11));
	imx_iomux_v3_setup_pad(MX53_PAD_EIM_DA13__GPIO3_13);
	gpio_direction_input(IMX_GPIO_NR(3, 13));

	if (cfg->esdhc_base == MMC_SDHC1_BASE_ADDR)
		ret = !gpio_get_value(IMX_GPIO_NR(3, 13));
	else
		ret = !gpio_get_value(IMX_GPIO_NR(3, 11));

	/* return ret; */
	return 1; /* FIXME */
}

#define SD_CMD_PAD_CTRL		(PAD_CTL_HYS | PAD_CTL_DSE_HIGH | \
				 PAD_CTL_PUS_100K_UP)
#define SD_PAD_CTRL		(PAD_CTL_HYS | PAD_CTL_PUS_47K_UP | \
				 PAD_CTL_DSE_HIGH)

int board_mmc_init(bd_t *bis)
{
	static const iomux_v3_cfg_t sd1_pads[] = {
		NEW_PAD_CTRL(MX53_PAD_SD1_CMD__ESDHC1_CMD, SD_CMD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_SD1_CLK__ESDHC1_CLK, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_SD1_DATA0__ESDHC1_DAT0, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_SD1_DATA1__ESDHC1_DAT1, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_SD1_DATA2__ESDHC1_DAT2, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_SD1_DATA3__ESDHC1_DAT3, SD_PAD_CTRL),
		MX53_PAD_EIM_DA13__GPIO3_13,
	};

	static const iomux_v3_cfg_t sd2_pads[] = {
		NEW_PAD_CTRL(MX53_PAD_PATA_RESET_B__ESDHC3_CMD,
				SD_CMD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_IORDY__ESDHC3_CLK, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA8__ESDHC3_DAT0, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA9__ESDHC3_DAT1, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA10__ESDHC3_DAT2, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA11__ESDHC3_DAT3, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA0__ESDHC3_DAT4, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA1__ESDHC3_DAT5, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA2__ESDHC3_DAT6, SD_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_PATA_DATA3__ESDHC3_DAT7, SD_PAD_CTRL),
		MX53_PAD_EIM_DA11__GPIO3_11,
	};

	u32 index;
	s32 status = 0;

	esdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
	esdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);

	for (index = 0; index < CONFIG_SYS_FSL_ESDHC_NUM; index++) {
		switch (index) {
		case 0:
			imx_iomux_v3_setup_multiple_pads(sd1_pads,
							 ARRAY_SIZE(sd1_pads));
			break;
		case 1:
			imx_iomux_v3_setup_multiple_pads(sd2_pads,
							 ARRAY_SIZE(sd2_pads));
			break;
		default:
			printf("Warning: you configured more ESDHC controller"
				"(%d) as supported by the board(2)\n",
				CONFIG_SYS_FSL_ESDHC_NUM);
			return status;
		}
		status |= fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
	}

	return status;
}
#endif

#define I2C_PAD_CTRL	(PAD_CTL_SRE_FAST | PAD_CTL_DSE_HIGH | \
			 PAD_CTL_PUS_100K_UP | PAD_CTL_ODE)

static void setup_iomux_i2c(void)
{
	static const iomux_v3_cfg_t i2c1_pads[] = {
		NEW_PAD_CTRL(MX53_PAD_EIM_D28__I2C1_SDA, I2C_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_EIM_D21__I2C1_SCL, I2C_PAD_CTRL),
	};

	imx_iomux_v3_setup_multiple_pads(i2c1_pads, ARRAY_SIZE(i2c1_pads));
}

static void clock_1GHz(void)
{
	int ret;
	u32 ref_clk = MXC_HCLK;
	/*
	 * After increasing voltage to 1.25V, we can switch
	 * CPU clock to 1GHz and DDR to 400MHz safely
	 */
	ret = mxc_set_clock(ref_clk, 1000, MXC_ARM_CLK);
	if (ret)
		printf("CPU:   Switch CPU clock to 1GHZ failed\n");

	ret = mxc_set_clock(ref_clk, 400, MXC_PERIPH_CLK);
	ret |= mxc_set_clock(ref_clk, 400, MXC_DDR_CLK);
	if (ret)
		printf("CPU:   Switch DDR clock to 400MHz failed\n");
}

int board_early_init_f(void)
{
	setup_iomux_uart();
	return 0;
}

#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	u32 cpurev;

	cpurev = get_cpu_rev();
	printf("CPU:   Freescale i.MX%x family rev%d.%d at %d MHz\n",
		(cpurev & 0xFF000) >> 12,
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		mxc_get_clock(MXC_ARM_CLK) / 1000000);
	printf("Reset cause: %s\n", get_reset_cause());
	return 0;
}
#endif

/*
 * Do not overwrite the console
 * Use always serial for U-Boot console
 */
int overwrite_console(void)
{
	return 1;
}

int board_init(void)
{
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;
	setup_iomux_i2c();

	return 0;
}

int board_late_init(void)
{
	print_cpuinfo();

	return 0;
}

int checkboard(void)
{
	puts("Board: USB armory\n");

	return 0;
}
