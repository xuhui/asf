/**
 * \file
 *
 * \brief Task to manage USB Device feature
 *
 * Copyright (c) 2014-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */

#include <asf.h>
#include "conf_app_display.h"
#include "conf_usb.h"
#include "app_usb.h"
#include "app_microsd.h"

/**
 * \ingroup usb_task_group
 * \defgroup usb_task_group_implementation USB task implementation
 * @{
 */

/* ! Link to USB icon */
extern struct gfx_mono_bitmap bitmap_usb;
/* ! Virtual Port is open, if true */
static bool app_usb_cdc_open = false;
/* ! Notify that the USB rectangle must be toggled */
static bool app_usb_rec_toggle = false;

void app_usb_task(void)
{
	static bool sw0_released = true;
	static bool usb_running = false;
	static bool cdc_running = false;
	static bool toggle = true;

	if (sw0_released && ioport_pin_is_low(GPIO_PUSH_BUTTON_0)) {
		/* A new press has been done */
		sw0_released = false;
		/* Switch USB state */
		if (usb_running) {
			udc_stop();
			gfx_mono_draw_filled_rect(DISPLAY_USB_STA_POS_X,
					DISPLAY_USB_STA_POS_Y,
					DISPLAY_USB_STA_SIZE_X,
					DISPLAY_USB_STA_SIZE_Y,
					GFX_PIXEL_CLR);
			app_microsd_start();
		} else {
			/* Stop FatFS on uSD card if enabled */
			app_microsd_stop();
			stdio_usb_init(); /* Start USB */
			gfx_mono_generic_put_bitmap(&bitmap_usb,
					DISPLAY_USB_STA_POS_X,
					DISPLAY_USB_STA_POS_Y);
		}

		usb_running = !usb_running;
		cdc_running = true;
	} else {
		/* Wait switch release */
		sw0_released = ioport_pin_is_high(GPIO_PUSH_BUTTON_0);
	}

	if (!usb_running) {
		return;
	}

	/* USB MSC task */
	while (udi_msc_process_trans()) {
	}

	if (app_usb_cdc_open && !cdc_running) {
		printf("\x0CSensor data logs:\r\n");
		cdc_running = true;
	}

	if (!app_usb_cdc_open && cdc_running) {
		cdc_running = false;
	}

	/* Toggle USB icon */
	if ((app_usb_cdc_open && app_usb_rec_toggle) ||
			(toggle && app_usb_rec_toggle)) {
		app_usb_rec_toggle = false;
		toggle = !toggle;
		gfx_mono_draw_rect(DISPLAY_USB_STACDC_POS_X,
				DISPLAY_USB_STACDC_POS_Y,
				DISPLAY_USB_STACDC_SIZE_X,
				DISPLAY_USB_STACDC_SIZE_Y,
				toggle ? GFX_PIXEL_SET : GFX_PIXEL_CLR);
	}
}

void app_usb_sof_action(void)
{
	static uint16_t counter = 0;
	if (++counter == 500) {
		counter = 0;
		app_usb_rec_toggle = true;
	}
}

void app_usb_cdc_set_dtr(bool b_enable)
{
	app_usb_cdc_open = b_enable;
}

bool app_usb_cdc_is_open(void)
{
	return app_usb_cdc_open;
}

/* ! @} */
