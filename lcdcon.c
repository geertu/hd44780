
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

static void lcdcon_init(struct vc_data *conp, int init)
{
    conp->vc_cols = 20;
    conp->vc_rows = 4;
    conp->vc_can_do_color = 0;
    conp->vc_complement_mask = 0x0800;
    conp->vc_hi_font_mask = 0;
}

static void lcdcon_deinit(struct vc_data *conp)
{
    int unit = conp->vc_num;
}

static void lcdcon_clear(struct vc_data *conp, int sy, int sx, int height,
			 int width)
{
    // FIXME
}

static void lcdcon_putc(struct vc_data *conp, int c, int ypos, int xpos)
{
    // FIXME
}

static void lcdcon_putcs(struct vc_data *conp, const unsigned short *s,
			 int count, int ypos, int xpos)
{
    // FIXME
}

static void lcdcon_cursor(struct vc_data *conp, int mode)
{
    // FIXME
}

static int lcdcon_scroll(struct vc_data *conp, int t, int b, int dir,
			 int count)
{
    // FIXME
}

static void lcdcon_bmove(struct vc_data *conp, int sy, int sx, int dy, int dx,
			 int height, int width)
{
    // FIXME
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
