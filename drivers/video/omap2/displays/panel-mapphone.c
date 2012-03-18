#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

#include <plat/display.h>
#include <plat/dma.h>
#include <asm/atomic.h>

#include <mach/dt_path.h>
#include <asm/prom.h>

#include <plat/panel.h>

#ifndef CONFIG_ARM_OF
#error CONFIG_ARM_OF must be defined for Mapphone to compile
#endif
#ifndef CONFIG_USER_PANEL_DRIVER
#error CONFIG_USER_PANEL_DRIVER must be defined for Mapphone to compile
#endif

#ifdef DEBUG
#define DBG(format, ...) (printk(KERN_DEBUG "mapphone-panel: " format, \
				## __VA_ARGS__))
#else
#define DBG(format, ...)
#endif

#define EDISCO_CMD_SOFT_RESET		0x01
#define EDISCO_CMD_ENTER_SLEEP_MODE	0x10
#define EDISCO_CMD_EXIT_SLEEP_MODE	0x11
#define EDISCO_CMD_SET_DISPLAY_ON	0x29
#define EDISCO_CMD_SET_DISPLAY_OFF	0x28
#define EDISCO_CMD_SET_COLUMN_ADDRESS	0x2A
#define EDISCO_CMD_SET_PAGE_ADDRESS	0x2B
#define EDISCO_CMD_SET_TEAR_OFF		0x34
#define EDISCO_CMD_SET_TEAR_ON		0x35
#define EDISCO_CMD_SET_TEAR_SCANLINE	0x44
#define EDISCO_CMD_SET_MCS		0xB2
#define EDISCO_CMD_DATA_LANE_CONFIG	0xB5

#define EDISCO_CMD_DATA_LANE_ONE	0x0
#define EDISCO_CMD_DATA_LANE_TWO	0x1

#define EDISCO_LONG_WRITE	0x29
#define EDISCO_SHORT_WRITE_1	0x23
#define EDISCO_SHORT_WRITE_0	0x13

#define EDISCO_CMD_READ_DDB_START	0xA1
#define EDISCO_CMD_VC   0
#define EDISCO_VIDEO_VC 1

#define PANEL_OFF     0x0
#define PANEL_ON      0x1

#define SUPPLIER_ID_AUO 0x0186
#define SUPPLIER_ID_TMD 0x0126
#define SUPPLIER_ID_INVALID 0xFFFF

/* this must be match with schema.xml section "device-id-value" */
#define MOT_DISP_MIPI_480_854_CM   	0x000a0001
#define MOT_DISP_430_MIPI_480_854_CM	0x001a0000
#define MOT_DISP_370_MIPI_480_854_CM	0x001a0001
#define MOT_DISP_248_MIPI_320_240_VM	0x00090002
#define MOT_DISP_280_MIPI_320_240_VM	0x00090003

static bool mapphone_panel_device_read_dt;

/* these enum must be matched with MOT DT */
enum omap_dss_device_disp_pxl_fmt {
	OMAP_DSS_DISP_PXL_FMT_RGB565	= 1,
	OMAP_DSS_DISP_PXL_FMT_RGB888	= 5
};

static struct omap_video_timings mapphone_panel_timings = {
	.x_res          = 480,
	.y_res          = 854,
	/*.pixel_clock  = 25000,*/
	.dsi1_pll_fclk	= 100000,
	.dsi2_pll_fclk  = 100000,
	.hfp            = 0,
	.hsw            = 2,
	.hbp            = 2,
	.vfp            = 0,
	.vsw            = 1,
	.vbp            = 1,
};

struct mapphone_data {
	struct omap_dss_device *dssdev;
	atomic_t state;
	void *panel_handle;
	unsigned long disp_init_delay;
};

static void mapphone_panel_disable_local(struct omap_dss_device *dssdev);

static void set_delay_timer(struct omap_dss_device *dssdev, unsigned long delay)
{
	struct mapphone_data *map_data = (struct mapphone_data *) dssdev->data;
	map_data->disp_init_delay = jiffies + msecs_to_jiffies(delay);
}

static void check_delay_timer(struct omap_dss_device *dssdev)
{
	struct mapphone_data *map_data = (struct mapphone_data *) dssdev->data;
	unsigned long jiff_time = 0;

	/*
	 * Delay if necessary, before calling EDISCO commands after
	 * EDISCO_CMD_EXIT_SLEEP
	 */
	if (map_data->disp_init_delay) {
		jiff_time = jiffies;
		if (map_data->disp_init_delay > jiff_time)
			mdelay(jiffies_to_msecs(map_data->disp_init_delay -
					jiff_time));
		map_data->disp_init_delay = 0;
	}
}

static int dsi_mipi_vm_panel_on(struct omap_dss_device *dssdev)
{
	u8 data = EDISCO_CMD_SET_DISPLAY_ON;
	int ret;

	dsi_disable_vid_vc_enable_cmd_vc();
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, &data, 1);
	dsi_disable_cmd_vc_enable_vid_vc();

	return ret;
}

static int dsi_mipi_cm_panel_on(struct omap_dss_device *dssdev)
{
	u8 data = EDISCO_CMD_SET_DISPLAY_ON;

	check_delay_timer(dssdev);

	return dsi_vc_dcs_write(EDISCO_CMD_VC, &data, 1);
}


static int mapphone_panel_display_on(struct omap_dss_device *dssdev)
{
	int ret = 0;

	struct mapphone_data *map_data = (struct mapphone_data *) dssdev->data;

	if (atomic_cmpxchg(&map_data->state, PANEL_OFF, PANEL_ON) ==
						PANEL_OFF) {
		switch (dssdev->panel.panel_id) {
		case MOT_DISP_248_MIPI_320_240_VM:
		case MOT_DISP_280_MIPI_320_240_VM:
			ret = dsi_mipi_vm_panel_on(dssdev);
			break;
		case MOT_DISP_MIPI_480_854_CM:
		case MOT_DISP_370_MIPI_480_854_CM:
		case MOT_DISP_430_MIPI_480_854_CM:
			ret = dsi_mipi_cm_panel_on(dssdev);
			break;
		default:
			printk(KERN_ERR "unsupport panel =0x%lx \n",
			dssdev->panel.panel_id);
			ret = -EINVAL;
		}

		if (ret == 0)
			printk(KERN_INFO "Panel is turned on \n");
	}

	return ret;
}

static int mapphone_panel_dt_panel_probe(int *pixel_size)
{

	if (mapphone_panel_device_read_dt == true)
		printk("\nmapphone_panel_device_read_dt =true");

	DBG("dt_panel_probe\n");

	/* Retrieve the panel DSI timing */
	mapphone_panel_timings.x_res = 480;

	mapphone_panel_timings.y_res = 854;

	mapphone_panel_timings.dsi1_pll_fclk = 136000;

	mapphone_panel_timings.dsi2_pll_fclk = 136000;

	mapphone_panel_timings.hfp = 0;

	mapphone_panel_timings.hsw = 2;

	mapphone_panel_timings.hbp = 2;

	mapphone_panel_timings.vfp = 0;

	mapphone_panel_timings.vsw = 1;

	mapphone_panel_timings.vbp = 1;

	*pixel_size = 24;

	DBG("DT:width=%d height=%d dsi1_pll_fclk=%d dsi2_pll_fclk=%d\n",
		mapphone_panel_timings.x_res,
		mapphone_panel_timings.y_res,
		mapphone_panel_timings.dsi1_pll_fclk,
		mapphone_panel_timings.dsi2_pll_fclk);

	DBG(" DT: hfp= %d hsw= %d hbp= %d vfp= %d vsw= %d vbp= %d\n",
		mapphone_panel_timings.hfp,
		mapphone_panel_timings.hsw,
		mapphone_panel_timings.hbp,
		mapphone_panel_timings.vfp,
		mapphone_panel_timings.vsw,
		mapphone_panel_timings.vbp);

		mapphone_panel_device_read_dt = true;

	return 0;

}


void panel_print_dt(void)
{
	printk(KERN_INFO "DT: width= %d height= %d\n",
		mapphone_panel_timings.x_res, mapphone_panel_timings.y_res);

	printk(KERN_INFO "DT: hfp= %d hsw= %d hbp= %d vfp= %d vsw= %d vbp= %d\n",
		mapphone_panel_timings.hfp, mapphone_panel_timings.hsw,
		mapphone_panel_timings.hbp, mapphone_panel_timings.vfp,
		mapphone_panel_timings.vsw, mapphone_panel_timings.vbp);
}

static int mapphone_panel_probe(struct omap_dss_device *dssdev)
{
	int pixel_size = 24;
	struct mapphone_data *data;
	struct omap_panel_device panel_dev;

	DBG("probe\n");

	if (mapphone_panel_dt_panel_probe(&pixel_size))
		printk(KERN_INFO "panel: using non-dt configuration\n");

	panel_print_dt();
	dssdev->ctrl.pixel_size = pixel_size;
	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	dssdev->panel.timings = mapphone_panel_timings;

	data = kmalloc(sizeof(struct mapphone_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	memset(data, 0, sizeof(data));

	strncpy(panel_dev.name, dssdev->name, OMAP_PANEL_MAX_NAME_SIZE);
	panel_dev.fod_disable = mapphone_panel_disable_local;
	panel_dev.dssdev = dssdev;
	data->panel_handle = omap_panel_register(&panel_dev);
	if (data->panel_handle == NULL) {
		printk(KERN_ERR "Panel Register Failed\n");
		kfree(data);
		data = NULL;
		return -ENODEV;
	}

	atomic_set(&data->state, PANEL_OFF);
	data->dssdev = dssdev;
	dssdev->data = data;

	return 0;
}

static void mapphone_panel_remove(struct omap_dss_device *dssdev)
{
	void *handle;
	struct mapphone_data *data = (struct mapphone_data *) dssdev->data;

	handle = data->panel_handle;
	omap_panel_unregister(handle);

	kfree(dssdev->data);
	return;
}

static u16 mapphone_panel_read_supplier_id(struct omap_dss_device *dssdev)
{
	static u16 id = SUPPLIER_ID_INVALID;
	u8 data[2];

	if (id == SUPPLIER_ID_AUO || id == SUPPLIER_ID_TMD)
		goto end;

	if (dsi_vc_set_max_rx_packet_size(EDISCO_CMD_VC, 2))
		goto end;

	if (dsi_vc_dcs_read(EDISCO_CMD_VC,
			    EDISCO_CMD_READ_DDB_START, data, 2) != 2)
		goto end;

	if (dsi_vc_set_max_rx_packet_size(EDISCO_CMD_VC, 1))
		goto end;

	id = (data[0] << 8) | data[1];

	if (id != SUPPLIER_ID_AUO && id != SUPPLIER_ID_TMD)
		id = SUPPLIER_ID_INVALID;
end:
	DBG("dsi_read_supplier_id() - supplier id [%hu]\n", id);
	return id;
}

static int dsi_mipi_248_vm_320_240_panel_enable(struct omap_dss_device *dssdev)
{
	u8 data[7];
	int ret;

	DBG(" dsi_mipi_248_vm_320_240_panel_enable() \n");

	/* turn it on */
	data[0] = EDISCO_CMD_EXIT_SLEEP_MODE;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 1);
	if (ret)
		goto error;

	mdelay(10);

	printk(KERN_INFO "done EDISCO CTRL ENABLE\n");
	return 0;
error:
	return -EINVAL;
}

static int dsi_mipi_280_vm_320_240_panel_enable(struct omap_dss_device *dssdev)
{
	u8 data[10];
	int ret;

	DBG(" dsi_mipi_280_vm_320_240_panel_enable() \n");

	/* turn off mcs register acces protection */
	data[0] = EDISCO_CMD_SET_MCS;
	data[1] = 0x00;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_SHORT_WRITE_1, data, 2);

	/* Internal display set up */
	data[0] = 0xC0;
	data[1] = 0x11;
	data[2] = 0x04;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_LONG_WRITE, data, 3);

	/* Internal voltage set up */
	data[0] = 0xD3;
	data[1] = 0x1F;
	data[2] = 0x01;
	data[3] = 0x02;
	data[4] = 0x15;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_LONG_WRITE, data, 5);

	/* Internal voltage set up */
	data[0] = 0xD4;
	data[1] = 0x62;
	data[2] = 0x1E;
	data[3] = 0x00;
	data[4] = 0xB7;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_LONG_WRITE, data, 5);

	/* Internal display set up */
	data[0] = 0xC5;
	data[1] = 0x01;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_SHORT_WRITE_1, data, 2);

	/* Load optimized red gamma (+) settings*/
	data[0] = 0xE9;
	data[1] = 0x01;
	data[2] = 0x0B;
	data[3] = 0x05;
	data[4] = 0x21;
	data[5] = 0x05;
	data[6] = 0x0D;
	data[7] = 0x01;
	data[8] = 0x0B;
	data[9] = 0x04;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 10);

	/* Load optimized red gamma (-) settings*/
	data[0] = 0xEA;
	data[1] = 0x04;
	data[2] = 0x0B;
	data[3] = 0x05;
	data[4] = 0x21;
	data[5] = 0x05;
	data[6] = 0x0D;
	data[7] = 0x01;
	data[8] = 0x08;
	data[9] = 0x04;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 10);

	/* Load optimized green gamma (+) settings*/
	data[0] = 0xEB;
	data[1] = 0x02;
	data[2] = 0x0B;
	data[3] = 0x05;
	data[4] = 0x21;
	data[5] = 0x05;
	data[6] = 0x0D;
	data[7] = 0x01;
	data[8] = 0x0B;
	data[9] = 0x04;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 10);

	/* Load optimized green gamma (-) settings*/
	data[0] = 0xEC;
	data[1] = 0x05;
	data[2] = 0x0B;
	data[3] = 0x05;
	data[4] = 0x21;
	data[5] = 0x05;
	data[6] = 0x0D;
	data[7] = 0x01;
	data[8] = 0x08;
	data[9] = 0x04;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 10);

	/* Load optimized blue gamma (+) settings*/
	data[0] = 0xED;
	data[1] = 0x04;
	data[2] = 0x0B;
	data[3] = 0x05;
	data[4] = 0x21;
	data[5] = 0x05;
	data[6] = 0x0D;
	data[7] = 0x01;
	data[8] = 0x0B;
	data[9] = 0x04;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 10);

	/* Load optimized blue gamma (-) settings*/
	data[0] = 0xEE;
	data[1] = 0x07;
	data[2] = 0x0B;
	data[3] = 0x05;
	data[4] = 0x21;
	data[5] = 0x05;
	data[6] = 0x0D;
	data[7] = 0x01;
	data[8] = 0x08;
	data[9] = 0x04;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 10);

	/* turn on mcs register acces protection */
	data[0] = EDISCO_CMD_SET_MCS;
	data[1] = 0x03;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_SHORT_WRITE_1, data, 2);

	/* turn it on */
	data[0] = EDISCO_CMD_EXIT_SLEEP_MODE;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 1);
	if (ret)
		goto error;

	mdelay(10);

	printk(KERN_INFO "done EDISCO CTRL ENABLE\n");
	return 0;
error:
	return -EINVAL;
}

static int dsi_mipi_cm_480_854_panel_enable(struct omap_dss_device *dssdev)
{
	u8 data[7];
	int ret;

	DBG("dsi_mipi_cm_480_854_panel_enable() \n");

	/* Check if the display we are using is actually a TMD display */
	if (dssdev->panel.panel_id == MOT_DISP_370_MIPI_480_854_CM) {
		if (mapphone_panel_read_supplier_id(dssdev)
						== SUPPLIER_ID_TMD) {
			DBG("dsi_mipi_cm_480_854_panel_enable() - TMD panel\n");
			dssdev->panel.panel_id = MOT_DISP_MIPI_480_854_CM;
		}
	}

	/* turn off mcs register acces protection */
	data[0] = EDISCO_CMD_SET_MCS;
	data[1] = 0x00;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_SHORT_WRITE_1, data, 2);

	/* enable lane setting and test registers*/
	data[0] = 0xef;
	data[1] = 0x01;
	data[2] = 0x01;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_LONG_WRITE, data, 3);

	/* 2nd param 61 = 1 line; 63 = 2 lanes */
	data[0] = 0xef;
	data[1] = 0x60;
	data[2] = 0x63;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 3);

	/* 2nd param 0 = WVGA; 1 = WQVGA */
	data[0] = 0xb3;
	data[1] = 0x00;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 2);

	/* Set dynamic backlight control and PWM; D[7:4] = PWM_DIV[3:0];*/
	/* D[3]=0 (PWM OFF);
	 * D[2]=0 (auto BL control OFF);
	 * D[1]=0 (Grama correction On);
	 * D[0]=0 (Enhanced Image Correction OFF) */
	data[0] = 0xb4;
	/* AUO displays require a different setting */
	if (dssdev->panel.panel_id == MOT_DISP_370_MIPI_480_854_CM)
		data[1] = 0x0f;
	else
		data[1] = 0x1f;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 2);

	/* set page, column address */
	data[0] = EDISCO_CMD_SET_PAGE_ADDRESS;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = (dssdev->panel.timings.y_res - 1) >> 8;
	data[4] = (dssdev->panel.timings.y_res - 1) & 0xff;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 5);
	if (ret)
		goto error;

	data[0] = EDISCO_CMD_SET_COLUMN_ADDRESS;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = (dssdev->panel.timings.x_res - 1) >> 8;
	data[4] = (dssdev->panel.timings.x_res - 1) & 0xff;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 5);
	if (ret)
		goto error;

	data[0] = EDISCO_CMD_EXIT_SLEEP_MODE;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 1);

	/*
	 * 200ms delay for internal block stabilization required before panel
	 * turns on after EDISCO_CMD_SLEEP_MODE_OUT command
	 */
	set_delay_timer(dssdev, 200);

	/*
	 * Allow the OTP setting to  load
	 */
	msleep(5);

	printk(KERN_INFO "done EDISCO CTRL ENABLE\n");

	return 0;
error:
	return -EINVAL;
}

static int dsi_mipi_430_cm_480_854_panel_enable(struct omap_dss_device *dssdev)
{
	u8 data[7];
	int ret;

	DBG("dsi_mipi_430_cm_480_854_panel_enable() \n");

	/* turn off mcs register acces protection */
	data[0] = EDISCO_CMD_SET_MCS;
	data[1] = 0x00;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_SHORT_WRITE_1, data, 2);

	/* Enable 2 data lanes */
	data[0] = EDISCO_CMD_DATA_LANE_CONFIG;
	data[1] = EDISCO_CMD_DATA_LANE_TWO;
	ret = dsi_vc_write(EDISCO_CMD_VC, EDISCO_SHORT_WRITE_1, data, 2);

	msleep(10);

	/* Set dynamic backlight control and PWM; D[7:4] = PWM_DIV[3:0];*/
	/* D[3]=0 (PWM OFF);
	 * D[2]=0 (auto BL control OFF);
	 * D[1]=0 (Grama correction On);
	 * D[0]=0 (Enhanced Image Correction OFF) */
	data[0] = 0xb4;
	data[1] = 0xdf;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 2);

	/* set page, column address */
	data[0] = EDISCO_CMD_SET_COLUMN_ADDRESS;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = (dssdev->panel.timings.x_res - 1) >> 8;
	data[4] = (dssdev->panel.timings.x_res - 1) & 0xff;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 5);
	if (ret)
		goto error;

	data[0] = EDISCO_CMD_SET_PAGE_ADDRESS;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = (dssdev->panel.timings.y_res - 1) >> 8;
	data[4] = (dssdev->panel.timings.y_res - 1) & 0xff;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 5);
	if (ret)
		goto error;


	/* Exit sleep mode */
	data[0] = EDISCO_CMD_EXIT_SLEEP_MODE;
		ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 1);
	if (ret) {
		printk(KERN_ERR "failed to send EXIT_SLEEP_MODE \n");
		goto error;
	}

	/*
	 * 120ms delay for internal block stabilization required before panel
	 * turns on after EDISCO_CMD_SLEEP_MODE_OUT command
	 */
	set_delay_timer(dssdev, 120);

	/*
	 * Allow the OTP setting to  load
	 */
	msleep(10);

	return 0;
error:
	return -EINVAL;
}

static int mapphone_panel_enable(struct omap_dss_device *dssdev)
{
	struct mapphone_data *map_data = (struct mapphone_data *) dssdev->data;
	int ret;
	void *handle;

	DBG("mapphone_panel_enable\n");
	if (dssdev->platform_enable) {
		ret = dssdev->platform_enable(dssdev);
		if (ret)
			return ret;
	}

	handle = map_data->panel_handle;
	if (omap_panel_fod_enabled(handle)) {
		atomic_set(&map_data->state, PANEL_OFF);
	}
	omap_panel_fod_dss_state(handle, 1);
	omap_panel_fod_panel_state(handle, 1);

	switch (dssdev->panel.panel_id) {
	case MOT_DISP_MIPI_480_854_CM:
	case MOT_DISP_370_MIPI_480_854_CM:
		ret = dsi_mipi_cm_480_854_panel_enable(dssdev);
		break;
	case MOT_DISP_430_MIPI_480_854_CM:
		ret = dsi_mipi_430_cm_480_854_panel_enable(dssdev);
		break;
	case MOT_DISP_248_MIPI_320_240_VM:
		ret = dsi_mipi_248_vm_320_240_panel_enable(dssdev) ;
		break;
	case MOT_DISP_280_MIPI_320_240_VM:
		ret = dsi_mipi_280_vm_320_240_panel_enable(dssdev) ;
		break;
	default:
		printk(KERN_ERR "unsupport panel =0x%lx \n",
			dssdev->panel.panel_id);
		goto error;
	}

	if (ret)
		goto error;

	return 0;
error:
	return -EINVAL;
}

static void mapphone_panel_disable_local(struct omap_dss_device *dssdev)
{
	u8 data[1];
	struct mapphone_data *map_data = (struct mapphone_data *) dssdev->data;

	atomic_set(&map_data->state, PANEL_OFF);

	data[0] = EDISCO_CMD_SET_DISPLAY_OFF;
	dsi_vc_dcs_write_nosync(EDISCO_CMD_VC, data, 1);

	data[0] = EDISCO_CMD_ENTER_SLEEP_MODE;
	dsi_vc_dcs_write_nosync(EDISCO_CMD_VC, data, 1);
	msleep(120);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

}

static void mapphone_panel_disable(struct omap_dss_device *dssdev)
{
	void *handle;

	DBG("mapphone_panel_disable\n");

	handle = ((struct mapphone_data *)dssdev->data)->panel_handle;
	omap_panel_fod_dss_state(handle, 0);
	if (omap_panel_fod_enabled(handle)) {
		DBG("Freezing the last frame on the display\n");
		return;
	}

	omap_panel_fod_panel_state(handle, 0);

	mapphone_panel_disable_local(dssdev);
}

static void mapphone_panel_setup_update(struct omap_dss_device *dssdev,
				      u16 x, u16 y, u16 w, u16 h)
{
	u8 data[5];
	int ret;

	/* set page, column address */
	data[0] = EDISCO_CMD_SET_PAGE_ADDRESS;
	data[1] = y >> 8;
	data[2] = y & 0xff;
	data[3] = (y + h - 1) >> 8;
	data[4] = (y + h - 1) & 0xff;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 5);
	if (ret)
		return;

	data[0] = EDISCO_CMD_SET_COLUMN_ADDRESS;
	data[1] = x >> 8;
	data[2] = x & 0xff;
	data[3] = (x + w - 1) >> 8;
	data[4] = (x + w - 1) & 0xff;
	ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 5);
	if (ret)
		return;
}

static int mapphone_panel_enable_te(struct omap_dss_device *dssdev, bool enable)
{
	u8 data[3];
	int ret;

	if (enable == true) {
		data[0] = EDISCO_CMD_SET_TEAR_ON;
		data[1] = 0x00;
		ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 2);
		if (ret)
			goto error;

		data[0] = EDISCO_CMD_SET_TEAR_SCANLINE;
		data[1] = 0x03;
		data[2] = 0x00;
		ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 3);
		if (ret)
			goto error;

	} else {
		data[0] = EDISCO_CMD_SET_TEAR_OFF;
		data[1] = 0x00;
		ret = dsi_vc_dcs_write(EDISCO_CMD_VC, data, 2);
		if (ret)
			goto error;
	}

	DBG(" edisco_ctrl_enable_te(%d) \n", enable);
	return 0;

error:
	return -EINVAL;
}

static int mapphone_panel_rotate(struct omap_dss_device *display, u8 rotate)
{
	return 0;
}

static int mapphone_panel_mirror(struct omap_dss_device *display, bool enable)
{
	return 0;
}

static int mapphone_panel_run_test(struct omap_dss_device *display,
					int test_num)
{
	return 0;
}

static int mapphone_panel_suspend(struct omap_dss_device *dssdev)
{
	mapphone_panel_disable(dssdev);
	return 0;
}

static int mapphone_panel_resume(struct omap_dss_device *dssdev)
{
	return mapphone_panel_enable(dssdev);
}

static struct omap_dss_driver mapphone_panel_driver = {
	.probe = mapphone_panel_probe,
	.remove = mapphone_panel_remove,

	.enable = mapphone_panel_enable,
	.framedone = mapphone_panel_display_on,
	.disable = mapphone_panel_disable,
	.suspend = mapphone_panel_suspend,
	.resume = mapphone_panel_resume,
	.setup_update = mapphone_panel_setup_update,
	.enable_te = mapphone_panel_enable_te,
	.set_rotate = mapphone_panel_rotate,
	.set_mirror = mapphone_panel_mirror,
	.run_test = mapphone_panel_run_test,

	.driver = {
		.name = "mapphone-panel",
		.owner = THIS_MODULE,
	},
};


static int __init mapphone_panel_init(void)
{
	DBG("mapphone_panel_init\n");
	omap_dss_register_driver(&mapphone_panel_driver);
	mapphone_panel_device_read_dt = false;
	return 0;
}

static void __exit mapphone_panel_exit(void)
{
	DBG("mapphone_panel_exit\n");

	omap_dss_unregister_driver(&mapphone_panel_driver);
}

module_init(mapphone_panel_init);
module_exit(mapphone_panel_exit);

MODULE_AUTHOR("Rebecca Schultz Zavin <rebecca@android.com>");
MODULE_DESCRIPTION("Sholes Panel Driver");
MODULE_LICENSE("GPL");
