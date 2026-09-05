#ifdef USE_LCD_HEADLESS
/* Headless "display" for boards without an LCD. The VGA core keeps
 * rendering into console->fb (needed for correct emulation), frames are
 * discarded. globals.panel stays NULL so lcd_draw() is a no-op. */
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include <stdio.h>
#include "common.h"

void pc_vga_step(void *o);

void lcd_draw(int x_start, int y_start, int x_end, int y_end, void *src)
{
	(void) x_start; (void) y_start;
	(void) x_end; (void) y_end; (void) src;
}

void vga_task(void *arg)
{
	(void) arg;
	fprintf(stderr, "vga runs headless (no LCD)\n");
	/* No panel to wait for: let the emulator task proceed. */
	xEventGroupSetBits(global_event_group, BIT1);
	xEventGroupWaitBits(global_event_group,
			    BIT0,
			    pdFALSE,
			    pdFALSE,
			    portMAX_DELAY);
	while (1) {
		pc_vga_step(globals.pc);
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}
#endif /* USE_LCD_HEADLESS */
