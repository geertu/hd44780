
/*
 *  Copyright 2000-2001 by Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  This programs is subject to the terms and conditions of the GNU General
 *  Public License
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/console_struct.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/vt_kern.h>

#include "hd44780.h"

static const char *lcdcon_startup(void)
{
    return "HD44780 LCD";
}


// FIXME
// Alternative using a 20x4 window on a 80x25 virtual screen?

#define LCD_COLS	20
#define LCD_ROWS	4

static char lcdcon_data[LCD_COLS*LCD_ROWS];
static const unsigned int lcdcon_row_offset[LCD_ROWS] = { 0, 64, 20, 84 };

static int lcdcon_cursor_shown = 1;
static unsigned int lcdcon_cur_x = 0, lcdcon_cur_y = 0;

static inline void lcdcon_goto(unsigned int x, unsigned int y)
{
    lcd_ddram(lcdcon_row_offset[y]+x);
}

static inline void lcdcon_goto_cursor(void)
{
    if (lcdcon_cursor_shown)
	lcdcon_goto(lcdcon_cur_x, lcdcon_cur_y);
}

static void lcdcon_write_vec(const char *data, unsigned int n)
{
    while (n--)
	lcd_write(*data++);
}

static void lcdcon_redraw_region(int sx, int sy, int width, int height)
{
    int y;

    for (y = sy; y < sy+height; y++) {
	lcdcon_goto(sx, y);
	lcdcon_write_vec(&lcdcon_data[y*LCD_COLS+sx], width);
    }
    lcdcon_goto_cursor();
}

static void lcdcon_init(struct vc_data *conp, int init)
{
    conp->vc_can_do_color = 0;
    if (init) {
	conp->vc_cols = LCD_COLS;
	conp->vc_rows = LCD_ROWS;
    } else
	vc_resize_con(LCD_ROWS, LCD_COLS, conp->vc_num);
}

static void lcdcon_deinit(struct vc_data *conp)
{
}

static void lcdcon_clear(struct vc_data *conp, int sy, int sx, int height,
			 int width)
{
    int y;

    if (sy == 0 && sx == 0 && height == LCD_ROWS && width == LCD_COLS) {
	lcd_clr();
	memset(lcdcon_data, ' ', LCD_COLS*LCD_ROWS);
    } else
	for (y = sy; y < sy+height; y++) {
	    lcdcon_goto(sx, y);
	    memset(&lcdcon_data[y*LCD_COLS+sx], ' ', width);
	    lcdcon_write_vec(&lcdcon_data[y*LCD_COLS+sx], width);
	}
    lcdcon_goto_cursor();
}

static void lcdcon_putc(struct vc_data *conp, int c, int ypos, int xpos)
{
    lcdcon_goto(xpos, ypos);
    lcd_write(c);
    lcdcon_data[ypos*LCD_COLS+xpos] = c;
    lcdcon_goto_cursor();
}

static void lcdcon_putcs(struct vc_data *conp, const unsigned short *s,
			 int count, int ypos, int xpos)
{
    lcdcon_goto(xpos, ypos);
    while (count--) {
	lcd_write(*s);
	lcdcon_data[ypos*LCD_COLS+xpos++] = *s++;
    }
    lcdcon_goto_cursor();
}

static void lcdcon_cursor(struct vc_data *conp, int mode)
{
    switch (mode) {
	case CM_ERASE:
	    lcdcon_cursor_shown = 0;
	    lcd_ctrl(LCD_DISP_ON, LCD_CURSOR_OFF, LCD_BLINK_OFF);
	    break;

	case CM_MOVE:
	case CM_DRAW:
	    lcdcon_cursor_shown = 1;
	    lcdcon_cur_x = conp->vc_x;
	    lcdcon_cur_y = conp->vc_y;
	    lcdcon_goto_cursor();
	    lcd_ctrl(LCD_DISP_ON, LCD_CURSOR_ON, LCD_BLINK_ON);
	    break;
    }
}

static int lcdcon_scroll(struct vc_data *conp, int t, int b, int dir,
			 int lines)
{
    switch (dir) {
	case SM_UP:
	    memmove(&lcdcon_data[t*LCD_COLS], &lcdcon_data[(t+lines)*LCD_COLS],
		    (b-t-lines)*LCD_COLS);
	    memset(&lcdcon_data[(b-lines)*LCD_COLS], ' ', lines*LCD_COLS);
	    lcdcon_redraw_region(0, t, LCD_COLS, b-t);
	    break;

	case SM_DOWN:
	    memmove(&lcdcon_data[(t+lines)*LCD_COLS], &lcdcon_data[t*LCD_COLS],
		    (b-t-lines)*LCD_COLS);
	    memset(&lcdcon_data[t*LCD_COLS], ' ', lines*LCD_COLS);
	    lcdcon_redraw_region(0, t, LCD_COLS, b-t);
	    break;
    }
    return 0;
}

static void lcdcon_bmove(struct vc_data *conp, int sy, int sx, int dy, int dx,
			 int height, int width)
{
    char *src, *dst;
    int i;

    if (sx == 0 && dx == 0 && width == LCD_COLS)
	memmove(&lcdcon_data[dy*LCD_COLS], &lcdcon_data[sy*LCD_COLS],
		height*LCD_COLS);
    else if (dy < sy || (dy == sy && dx < sx)) {
	src = &lcdcon_data[sy*LCD_COLS+sx];
	dst = &lcdcon_data[dy*LCD_COLS+dx];
	for (i = height; i > 0; i--, src += LCD_COLS, dst += LCD_COLS)
	    memmove(dst, src, width);
    } else {
	src = &lcdcon_data[(sy+height-1)*LCD_COLS+sx];
	dst = &lcdcon_data[(dy+height-1)*LCD_COLS+dx];
	for (i = height; i > 0; i--, src -= LCD_COLS, dst -= LCD_COLS)
	    memmove(dst, src, width);
    }
    lcdcon_redraw_region(dx, dy, width, height);
}

static int lcdcon_switch(struct vc_data *conp)
{
    return 1;
}

static int lcdcon_blank(struct vc_data *conp, int blank)
{
    lcd_backlight(blank ? 0 : 1);
    return 0;
}

static int lcdcon_font_op(struct vc_data *vc, struct console_font_op *f)
{
    return -ENOSYS;
}

static int lcdcon_set_palette(struct vc_data *vc, unsigned char *table)
{
    return -EINVAL;
}

static int lcdcon_scrolldelta(struct vc_data *vc, int lines)
{
    return 0;
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
    con_font_op:	lcdcon_font_op,
    con_set_palette:	lcdcon_set_palette,
    con_scrolldelta:	lcdcon_scrolldelta,
};


#ifdef MODULE
int init_module(void)
{
    take_over_console(&lcd_con, 6-1, 6-1, 0);
    return 0;
}

void cleanup_module(void)
{
    give_up_console(&lcd_con);
}
#endif /* MODULE */
