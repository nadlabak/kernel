/*
 * linux/arch/arm/mach-omap2/board-mapphone-camera.c
 *
 * Copyright (C) 2009 Motorola, Inc.
 *
 * Derived from mach-omap3/board-3430sdp.c
 *
 * Copyright (C) 2007 Texas Instruments
 *
 * Modified from mach-omap2/board-generic.c
 *
 * Initial code: Syed Mohammed Khasim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <plat/mux.h>
#include <plat/board-mapphone.h>
#include <plat/omap-pm.h>
#include <plat/control.h>
#include <linux/string.h>
#include <linux/gpio_mapping.h>
#include <plat/resource.h>

#if defined(CONFIG_VIDEO_OMAP3)
#include <media/v4l2-int-device.h>
#include <../drivers/media/video/omap34xxcam.h>
#include <../drivers/media/video/isp/ispreg.h>
#include <../drivers/media/video/isp/isp.h>
#include <../drivers/media/video/isp/ispcsi2.h>
#if defined(CONFIG_VIDEO_MT9P012) || defined(CONFIG_VIDEO_MT9P012_MODULE)
#include <media/mt9p012.h>
#endif

#if defined(CONFIG_VIDEO_OV8810) || defined(CONFIG_VIDEO_OV8810_MODULE)
#include <media/ov8810.h>
#if defined(CONFIG_LEDS_FLASH_RESET)
#include <linux/spi/cpcap.h>
#include <linux/spi/cpcap-regbits.h>
#endif

////////////////////////////////////////////////////////////////////////
// Adding by no change device tree
/* BD7885 Node */
#define DT_PATH_BD7885			"/System@0/I2C@0/XENONBD7885@0"
/* Feature Node */
#define DT_HIGH_LEVEL_FEATURE	"/System@0/Feature@0"
////////////////////////////////////////////////////////////////////////

#define OV8810_CSI2_CLOCK_POLARITY	0	/* +/- pin order */
#define OV8810_CSI2_DATA0_POLARITY	0	/* +/- pin order */
#define OV8810_CSI2_DATA1_POLARITY	0	/* +/- pin order */
#define OV8810_CSI2_CLOCK_LANE		1	 /* Clock lane position: 1 */
#define OV8810_CSI2_DATA0_LANE		2	 /* Data0 lane position: 2 */
#define OV8810_CSI2_DATA1_LANE		3	 /* Data1 lane position: 3 */
#define OV8810_CSI2_PHY_THS_TERM	1  /* GVH */
#define OV8810_CSI2_PHY_THS_SETTLE	21  /* GVH */
#define OV8810_CSI2_PHY_TCLK_TERM	0
#define OV8810_CSI2_PHY_TCLK_MISS	1
#define OV8810_CSI2_PHY_TCLK_SETTLE	14
#define CPUCLK_LOCK_VAL                 0x5
#endif
#ifdef CONFIG_VIDEO_OMAP3_HPLENS
#include <../drivers/media/video/hplens.h>
#endif
#endif



#define CAM_IOMUX_SAFE_MODE (OMAP343X_PADCONF_PULL_UP | \
				OMAP343X_PADCONF_PUD_ENABLED | \
				OMAP343X_PADCONF_MUXMODE7)
#define CAM_IOMUX_SAFE_MODE_INPUT (OMAP343X_PADCONF_INPUT_ENABLED | \
				OMAP343X_PADCONF_PULL_UP | \
				OMAP343X_PADCONF_PUD_ENABLED | \
				OMAP343X_PADCONF_MUXMODE7)
#define CAM_IOMUX_FUNC_MODE (OMAP343X_PADCONF_INPUT_ENABLED | \
				OMAP343X_PADCONF_MUXMODE0)

#define CAM_MAX_REGS 5
#define CAM_MAX_REG_NAME_LEN 8

static void mapphone_camera_lines_safe_mode(void);
static void mapphone_camera_lines_func_mode(void);
/* devtree regulator support */

#if defined(EMERADLD_HIGH_FEATURE)
static void mapphone_init_reg_list(void);
static void mapphone_init_flash_list(void);
#endif

static char regulator_list[CAM_MAX_REGS][CAM_MAX_REG_NAME_LEN];
/* devtree flash */
static u8 bd7885_available;
static enum v4l2_power previous_power = V4L2_POWER_OFF;

#ifdef CONFIG_VIDEO_OMAP3_HPLENS
static int hplens_power_set(enum v4l2_power power)
{
	(void)power;

	return 0;
}

static int hplens_set_prv_data(void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;

	hwc->dev_index = 0;
	hwc->dev_minor = 0;
	hwc->dev_type = OMAP34XXCAM_SLAVE_LENS;

	return 0;
}

struct hplens_platform_data mapphone_hplens_platform_data = {
	.power_set = hplens_power_set,
	.priv_data_set = hplens_set_prv_data,
};
#endif

#if defined(CONFIG_VIDEO_MT9P012) || defined(CONFIG_VIDEO_MT9P012_MODULE)
static struct omap34xxcam_sensor_config mt9p012_cam_hwc = {
	.sensor_isp = 0,
	.xclk = OMAP34XXCAM_XCLK_A,
	.capture_mem = PAGE_ALIGN(2592 * 1944 * 2) * 4,
};

static int mt9p012_sensor_set_prv_data(void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;

	hwc->u.sensor.xclk = mt9p012_cam_hwc.xclk;
	hwc->u.sensor.sensor_isp = mt9p012_cam_hwc.sensor_isp;
	hwc->u.sensor.capture_mem = mt9p012_cam_hwc.capture_mem;
	hwc->dev_index = 0;
	hwc->dev_minor = 0;
	hwc->dev_type = OMAP34XXCAM_SLAVE_SENSOR;
	return 0;
}

static struct isp_interface_config mt9p012_if_config = {
	.ccdc_par_ser = ISP_PARLL,
	.dataline_shift = 0x1,
	.hsvs_syncdetect = ISPCTRL_SYNC_DETECT_VSRISE,
	.strobe = 0x0,
	.prestrobe = 0x0,
	.shutter = 0x0,
	.wenlog = ISPCCDC_CFG_WENLOG_OR,
	.wait_bayer_frame = 0,
	.wait_yuv_frame = 1,
	.dcsub = 42,
	.cam_mclk = 432000000,
	.cam_mclk_src_div = OMAP_MCAM_SRC_DIV,
	.raw_fmt_in = ISPCCDC_INPUT_FMT_GR_BG,
	.u.par.par_bridge = 0x0,
	.u.par.par_clk_pol = 0x0,
};

static int mt9p012_sensor_power_set(struct device* dev, enum v4l2_power power)
{
	static struct regulator *regulator;
	int error = 0;

	switch (power) {
	case V4L2_POWER_OFF:
		/* Power Down Sequence */
		gpio_direction_output(GPIO_MT9P012_RESET, 0);
		gpio_free(GPIO_MT9P012_RESET);

		/* Turn off power */
		if (regulator != NULL) {
			regulator_disable(regulator);
			regulator_put(regulator);
			regulator = NULL;
		} else {
			mapphone_camera_lines_safe_mode();
			pr_err("%s: Regulator for vcam is not "\
					"initialized\n", __func__);
			return -EIO;
		}

		/* Release pm constraints */
		omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, 0);
		omap_pm_set_max_mpu_wakeup_lat(dev, -1);
		mapphone_camera_lines_safe_mode();
	break;
	case V4L2_POWER_ON:
		if (previous_power == V4L2_POWER_OFF) {
			/* Power Up Sequence */
			mapphone_camera_lines_func_mode();
			/* Set min throughput to:
			 *  2592 x 1944 x 2bpp x 30fps x 3 L3 accesses */
			omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, 885735);
			/* Hold a constraint to keep MPU in C1 */
			omap_pm_set_max_mpu_wakeup_lat(dev, MPU_LATENCY_C1);

			/* Configure ISP */
			isp_configure_interface(&mt9p012_if_config);

			/* Request and configure gpio pins */
			if (gpio_request(GPIO_MT9P012_RESET,
						"mt9p012 camera reset") != 0) {
				error = -EIO;
				goto out;
			}

			/* set to output mode */
			gpio_direction_output(GPIO_MT9P012_RESET, 0);

			/* nRESET is active LOW. set HIGH to release reset */
			gpio_set_value(GPIO_MT9P012_RESET, 1);

			/* turn on digital power */
			if (regulator != NULL) {
				pr_warning("%s: Already have "\
						"regulator\n", __func__);
			} else {
				regulator = regulator_get(NULL, "vcam");
				if (IS_ERR(regulator)) {
					pr_err("%s: Cannot get vcam "\
						"regulator, err=%ld\n",
						__func__, PTR_ERR(regulator));
					error = PTR_ERR(regulator);
					goto out;
				}
			}

			if (regulator_enable(regulator) != 0) {
				pr_err("%s: Cannot enable vcam regulator\n",
						__func__);
				error = -EIO;
				goto out;
			}
		}

		udelay(1000);

		if (previous_power == V4L2_POWER_OFF) {
			/* trigger reset */
			gpio_direction_output(GPIO_MT9P012_RESET, 0);

			udelay(1500);

			/* nRESET is active LOW. set HIGH to release reset */
			gpio_set_value(GPIO_MT9P012_RESET, 1);

			/* give sensor sometime to get out of the reset.
			 * Datasheet says 2400 xclks. At 6 MHz, 400 usec is
			 * enough
			 */
			udelay(300);
		}
		break;
out:
		omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, 0);
		omap_pm_set_max_mpu_wakeup_lat(dev, -1);
		mapphone_camera_lines_safe_mode();
		return error;
	case V4L2_POWER_STANDBY:
		/* Stand By Sequence */
		break;
	}
	/* Save powerstate to know what was before calling POWER_ON. */
	previous_power = power;
	return 0;
}

u32 mt9p012_set_xclk(u32 xclkfreq)
{
	return isp_set_xclk(xclkfreq, OMAP34XXCAM_XCLK_A);
}


struct mt9p012_platform_data mapphone_mt9p012_platform_data = {
	.power_set = mt9p012_sensor_power_set,
	.priv_data_set = mt9p012_sensor_set_prv_data,
	.set_xclk = isp_set_xclk,
	.csi2_lane_count = isp_csi2_complexio_lanes_count,
	.csi2_cfg_vp_out_ctrl = isp_csi2_ctrl_config_vp_out_ctrl,
	.csi2_ctrl_update = isp_csi2_ctrl_update,
	.csi2_cfg_virtual_id = isp_csi2_ctx_config_virtual_id,
	.csi2_ctx_update = isp_csi2_ctx_update,
	.csi2_calc_phy_cfg0  = isp_csi2_calc_phy_cfg0,
};

#endif /* #ifdef CONFIG_VIDEO_MT9P012 || CONFIG_VIDEO_MT9P012_MODULE */


/* We can't change the IOMUX config after bootup
 * with the current pad configuration architecture,
 * the next two functions are hack to configure the
 * camera pads at runtime to save power in standby.
 * For phones don't have MIPI camera support, like
 * Ruth, Tablet P2,P3 */

void mapphone_camera_lines_safe_mode(void)
{
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x011a);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x011c);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x011e);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x0120);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0122);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0124);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0126);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0128);
#if defined(CONFIG_VIDEO_OLDOMAP3)
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x012a);
	omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x012c);
#endif
}

void mapphone_camera_lines_func_mode(void)
{
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x011a);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x011c);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x011e);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0120);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0122);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0124);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0126);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0128);
#if defined(CONFIG_VIDEO_OLDOMAP3)
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x012a);
	omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x012c);
#endif

}

#if defined(EMERADLD_HIGH_FEATURE)
void mapphone_init_reg_list()
{
#ifdef CONFIG_ARM_OF
	struct device_node *feat_node;
	const void *feat_prop;
	char *prop_name;
	char reg_name[CAM_MAX_REG_NAME_LEN];
	int reg_entry;
	int feature_name_len, i, j;

	/* clear the regulator list */
	memset(regulator_list, 0x0, sizeof(regulator_list));

	/* get regulator info for this device */
	feat_node = of_find_node_by_path(DT_HIGH_LEVEL_FEATURE);
	if (NULL == feat_node)
		return;

	feat_prop = of_get_property(feat_node,
				"feature_cam_regulators", NULL);
	if (NULL != feat_prop) {
		prop_name = (char *)feat_prop;
		printk(KERN_INFO \
			"Regulators for device: %s\n", prop_name);
		feature_name_len = strlen(prop_name);

		memset(reg_name, 0x0, CAM_MAX_REG_NAME_LEN);

		for (i = 0; i < feature_name_len; i++) {

			if (prop_name[i] != '\0' && prop_name[i] != ',')
				reg_name[j++] = prop_name[i];

			if (prop_name[i] == ',' ||\
				 (i == feature_name_len-1)) {
				printk(KERN_INFO \
					"Adding %s to camera \
						regulator list\n",\
					reg_name);
				if (reg_entry < CAM_MAX_REGS) {
					strncpy(\
						regulator_list[reg_entry++],\
						reg_name,\
						CAM_MAX_REG_NAME_LEN);
					memset(reg_name, 0x0, \
						CAM_MAX_REG_NAME_LEN);
					j = 0;
				} else {
					break;
				}
			}

		}
	}
#endif
	return;
}

static void mapphone_init_flash_list(void)
{
#ifdef CONFIG_ARM_OF
	struct device_node *node;
	int len = 0;
	const uint32_t *val;

	node = of_find_node_by_path(DT_PATH_BD7885);
	if (node != NULL) {
		val =
			of_get_property(node, "device_available", &len);
		if (val && len)
			bd7885_available =	*(u8 *)val;
	}
#endif
}
#endif

void __init mapphone_camera_init(void)
{
#if defined(EMERADLD_HIGH_FEATURE)
	mapphone_init_reg_list();
	mapphone_init_flash_list();
#endif
    printk(KERN_INFO "mapphone_camera_init: conventional camera\n");
    omap_cfg_reg(A24_34XX_CAM_HS);
    omap_cfg_reg(A23_34XX_CAM_VS);
    omap_cfg_reg(C27_34XX_CAM_PCLK);
    omap_cfg_reg(B24_34XX_CAM_D2);
    omap_cfg_reg(C24_34XX_CAM_D3);
    omap_cfg_reg(D24_34XX_CAM_D4);
    omap_cfg_reg(A25_34XX_CAM_D5);
    omap_cfg_reg(K28_34XX_CAM_D6);
    omap_cfg_reg(L28_34XX_CAM_D7);
    omap_cfg_reg(K27_34XX_CAM_D8);
    omap_cfg_reg(L27_34XX_CAM_D9);
    omap_cfg_reg(B25_34XX_CAM_D10);
    omap_cfg_reg(C26_34XX_CAM_D11);
    omap_cfg_reg(B23_34XX_CAM_WEN);
    omap_cfg_reg(D25_34XX_CAM_STROBE);
    omap_cfg_reg(K8_34XX_GPMC_WAIT2);
	omap_cfg_reg(C25_34XX_CAM_XCLKA);
	omap_cfg_reg(C23_34XX_CAM_FLD);
#if defined(CONFIG_VIDEO_OLDOMAP3)
	omap_cfg_reg(AG17_34XX_CAM_D0);
	omap_cfg_reg(AH17_34XX_CAM_D1);
#else
	omap_cfg_reg(AG17_34XX_CAM_D0_S);
	omap_cfg_reg(AH17_34XX_CAM_D1_S);
#endif

    mapphone_camera_lines_safe_mode();
}
