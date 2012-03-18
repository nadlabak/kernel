/*
 * arch/arm/mach-omap2/board-mapphone-cpcap-client.c
 *
 * Copyright (C) 2009-2010 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/spi/cpcap.h>
#include <linux/leds-ld-cpcap.h>
#include <linux/leds-ld-cpcap-disp.h>
#include <linux/leds-cpcap-kpad.h>
#include <linux/leds-cpcap-display.h>
#include <linux/leds-cpcap-button.h>
#include <mach/gpio.h>
#include <plat/mux.h>
#include <plat/resource.h>
#include <plat/omap34xx.h>

#ifdef CONFIG_ARM_OF
#include <mach/dt_path.h>
#include <asm/prom.h>
#endif

//////////////////////////////////////////////////////////////////////
// Adding by no change device tree
/* LM3559 Node */
#define DT_PATH_LM3559			"/System@0/I2C@0/LEDLM3559@0"

//////////////////////////////////////////////////////////////////////

/*
 * CPCAP devcies are common for different HW Rev.
 *
 */
static struct platform_device cpcap_3mm5_device = {
	.name   = "cpcap_3mm5",
	.id     = -1,
	.dev    = {
		.platform_data  = NULL,
	},
};

#ifdef CONFIG_CPCAP_USB
static struct platform_device cpcap_usb_device = {
	.name           = "cpcap_usb",
	.id             = -1,
	.dev.platform_data = NULL,
};

static struct platform_device cpcap_usb_det_device = {
	.name   = "cpcap_usb_det",
	.id     = -1,
	.dev    = {
		.platform_data  = NULL,
	},
};
#endif /* CONFIG_CPCAP_USB */

#ifdef CONFIG_TTA_CHARGER
static struct platform_device cpcap_tta_det_device = {
	.name   = "cpcap_tta_charger",
	.id     = -1,
	.dev    = {
		.platform_data  = NULL,
	},
};
#endif


static struct platform_device cpcap_rgb_led = {
	.name           = LD_MSG_IND_DEV,
	.id             = -1,
	.dev.platform_data  = NULL,
};

#ifdef CONFIG_SOUND_CPCAP_OMAP
static struct platform_device cpcap_audio_device = {
	.name           = "cpcap_audio",
	.id             = -1,
	.dev.platform_data  = NULL,
};
#endif

#ifdef CONFIG_LEDS_AF_LED
static struct platform_device cpcap_af_led = {
	.name           = LD_AF_LED_DEV,
	.id             = -1,
	.dev            = {
		.platform_data  = NULL,
       },
};
#endif

static struct platform_device cpcap_bd7885 = {
	.name           = "bd7885",
	.id             = -1,
	.dev            = {
		.platform_data  = NULL,
       },
};

static struct platform_device cpcap_vio_active_device = {
	.name		= "cpcap_vio_active",
	.id		= -1,
	.dev		= {
		.platform_data = NULL,
	},
};

static struct platform_device *cpcap_devices[] = {
#ifdef CONFIG_CPCAP_USB
	&cpcap_usb_device,
	&cpcap_usb_det_device,
#endif
#ifdef CONFIG_SOUND_CPCAP_OMAP
	&cpcap_audio_device,
#endif
	&cpcap_3mm5_device,
#ifdef CONFIG_TTA_CHARGER
	&cpcap_tta_det_device,
#endif
#ifdef CONFIG_LEDS_AF_LED
	&cpcap_af_led,
#endif
	&cpcap_bd7885
};


/*
 * CPCAP devcies whose availability depends on HW
 *
 */
static struct platform_device  cpcap_kpad_led = {
	.name           = CPCAP_KPAD_DEV,
	.id             = -1,
	.dev            = {
	.platform_data  = NULL,
	},
};

static struct platform_device ld_cpcap_kpad_led = {
	.name           = LD_KPAD_DEV,
	.id             = -1,
	.dev            = {
		.platform_data  = NULL,
	},
};

static struct platform_device cpcap_button_led = {
	.name           = CPCAP_BUTTON_DEV,
	.id             = -1,
	.dev            = {
	.platform_data  = NULL,
	},
};

static struct disp_button_config_data btn_data;

static struct platform_device ld_cpcap_disp_button_led = {
	.name           = LD_DISP_BUTTON_DEV,
	.id             = -1,
	.dev            = {
	.platform_data  = &btn_data,
	},
};

static struct platform_device cpcap_display_led = {
	.name           = CPCAP_DISPLAY_DRV,
	.id             = -1,
	.dev            = {
		.platform_data  = NULL,
	},
};

static struct platform_device cpcap_lm3554 = {
	.name           = "flash-torch",
	.id             = -1,
	.dev            = {
		.platform_data  = NULL,
	},
};

static struct platform_device cpcap_lm3559 = {
	.name           = "flash-torch-3559",
	.id             = -1,
	.dev            = {
		.platform_data  = NULL,
	},
};

#ifdef CONFIG_ARM_OF
static int __init is_ld_cpcap_kpad_on(void)
{
	u8 device_available;

     device_available = 1;
	return device_available;
}

static int __init is_cpcap_kpad_on(void)
{
	u8 device_available;

	device_available = 0;
	return device_available;
}

static int __init cpcap_button_init(void)
{
	u8 device_available;

    device_available = 0;
	return device_available;
}

static int __init ld_cpcap_disp_button_init(void)
{
	struct disp_button_config_data *pbtn_data = &btn_data;
	u8 device_available, ret;

	ret = -ENODEV;

        device_available = 1;
        pbtn_data->duty_cycle = 0x2A0;
	pbtn_data->cpcap_mask = 0x3FF;
	pbtn_data->led_current =  0xA;
	pbtn_data->reg = CPCAP_REG_KLC;

	ret = 1;
	return ret;
}

static int __init is_disp_led_on(void)
{
	u8 device_available;

	device_available = 0;
	return device_available;
}

static int __init led_cpcap_lm3554_init(void)
{
	u8 device_available;

	device_available = 1;
	return device_available;
}

static int __init led_cpcap_lm3559_init(void)
{
	u8 device_available;
	struct device_node *node;
	const void *prop;

	node = of_find_node_by_path(DT_PATH_LM3559);
	if (node == NULL)
	{
		return -ENODEV;
	}

	prop = of_get_property(node, "device_available", NULL);
	if (prop)
		device_available = *(u8 *)prop;
	else {
		pr_err("Read property %s error!\n", "device_available");
		of_node_put(node);
		return -ENODEV;
	}

	of_node_put(node);

	return device_available;
}

static int __init is_ld_cpcap_rgb_on(void)
{
	u8 device_available;

	device_available = 1;
	return device_available;
}

static int is_cpcap_vio_supply_converter(void)
{
	/* The converter is existing by default */
	return 1;
}

#endif /* CONFIG_ARM_OF */


void __init mapphone_cpcap_client_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cpcap_devices); i++)
		cpcap_device_register(cpcap_devices[i]);

	if (is_cpcap_kpad_on() > 0)
		cpcap_device_register(&cpcap_kpad_led);

	if (is_ld_cpcap_kpad_on() > 0)
		cpcap_device_register(&ld_cpcap_kpad_led);

	if (ld_cpcap_disp_button_init() > 0)
		cpcap_device_register(&ld_cpcap_disp_button_led);

	if (cpcap_button_init() > 0)
		cpcap_device_register(&cpcap_button_led);

	if (is_disp_led_on() > 0)
		cpcap_device_register(&cpcap_display_led);

	if (led_cpcap_lm3554_init() > 0)
		cpcap_device_register(&cpcap_lm3554);

	if (led_cpcap_lm3559_init() > 0)
		cpcap_device_register(&cpcap_lm3559);

	if (is_ld_cpcap_rgb_on() > 0)
		cpcap_device_register(&cpcap_rgb_led);

	if (!is_cpcap_vio_supply_converter())
		cpcap_device_register(&cpcap_vio_active_device);
}
