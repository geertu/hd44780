
/*
 *  Copyright 1997-2000 by Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  This programs is subject to the terms and conditions of the GNU General
 *  Public License
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/console_struct.h>
#include <linux/console.h>

#include "hd44780.h"

static const char *lcdcon_startup(void)
{
    return "HD44780 LCD";
}


// FIXME
// Alternative using a 20x4 window on a 80x25 virtual screen?

#define LCD_COLS	20
#define LCD_ROWS	4

static void lcdcon_init(struct vc_data *conp, int init)
{
    conp->vc_can_do_color = 0;
    if (init) {
	conp->vc_cols = LCD_COLS;
	conp->vc_rows = LCD_ROWS;
    } else
	vc_resize_con(LCD_COLS, LCD_ROWS, conp->vc_num);
}

static void lcdcon_deinit(struct vc_data *conp)
{
}

static void lcdcon_clear(struct vc_data *conp, int sy, int sx, int height,
			 int width)
{
    if (sy == 0 && sx == 0 && height == LCD_ROWS && width == LCD_COLS)
	lcd_clr();
    else
	for (y = sy; y < sy+height; y++) {
	    lcd_ddram(lcd_row_offset[y]+sx);
	    memset(&lcd_data[y*LCD_COLS+sx], ' ', width);
	    lcd_write_vec(&lcd_data[y*LCD_COLS+sx], width);
	}
    lcd_ddram(lcd_row_offset[lcd_row]+lcd_col);
}

static void lcdcon_putc(struct vc_data *conp, int c, int ypos, int xpos)
{
    if (ypos != lcd_row || xpos != lcd_col)
	lcd_ddram(lcd_row_offset[ypos]+xpos);
    lcd_write(c);
    lcd_data[ypos*LCD_COLS+xpos] = c;
    lcd_ddram(lcd_row_offset[lcd_row]+lcd_col);
}

static void lcdcon_putcs(struct vc_data *conp, const unsigned short *s,
			 int count, int ypos, int xpos)
{
    if (ypos != lcd_row || xpos != lcd_col)
	lcd_ddram(lcd_row_offset[ypos]+xpos);
    while (count--) {
	lcd_write(c);
	lcd_data[ypos*LCD_COLS+xpos++] = c;
    }
    lcd_ddram(lcd_row_offset[lcd_row]+lcd_col);
}

static void lcdcon_cursor(struct vc_data *conp, int mode)
{
    switch (mode) {
	case CM_ERASE:
	    lcd_ctrl(LCD_DISP_ON, LCD_CURSOR_OFF, LCD_BLINK_OFF);
	    break;

	case CM_MOVE:
	case CM_DRAW:
	    if (conp->vc_y != lcd_row || conp->vc_x != lcd_col) {
		lcd_col = conp->vc_x;
		lcd_row = conp->vc_y;
		lcd_ddram(lcd_row_offset[lcd_row]+lcd_col);
	    }
	    lcd_ctrl(LCD_DISP_ON, LCD_CURSOR_ON, LCD_BLINK_ON);
	    break;
    }
}

static int lcdcon_scroll(struct vc_data *conp, int t, int b, int dir,
			 int count)
{
    // FIXME
    // Redraw using data from scr_readw()?
}

static void lcdcon_bmove(struct vc_data *conp, int sy, int sx, int dy, int dx,
			 int height, int width)
{
    // FIXME
    // Redraw using data from scr_readw()?
}

static int lcdcon_switch(struct vc_data *conp)
{
    // FIXME
}

static int lcdcon_blank(struct vc_data *conp, int blank)
{
    lcd_backlight(blank ? 0 : 1);
}


static struct consw lcd_con = {
    con_startup:	lcdcon_startup,
    con_init:		lcdcon_init,
    con_deinit:		lcdcon_deinit,
    con_clear:		lcdcon_clear,
    con_putc:		lcdcon_putc,
    con_putcs:		lcdcon_putcs,
    con_cursor:		lcdcon_cursor,
    con_scroll:		lcdcon_scroll,
    con_bmove:		lcdcon_bmove,
    con_switch:		lcdcon_switch,
    con_blank:		lcdcon_blank,
};


#ifdef MODULE
int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{
}
#endif /* MODULE */
