
#define SCROLL_REDRAW
#undef SCROLL_SHIFT

/*
 *  Copyright 1997-2000 by Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  This programs is subject to the terms and conditions of the GNU General
 *  Public License
 */


#include <stdarg.h>

#ifdef __KERNEL__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/console.h>

static int lcd_console_messages = 1;

#else /* !__KERNEL__ */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MOD_INC_USE_COUNT	do { } while (0)
#define MOD_DEC_USE_COUNT	do { } while (0)

typedef unsigned char u8;

unsigned long loops_per_sec = 1;
unsigned long max_udelay;

static void delay(unsigned long loops)
{
    long i;

    for (i = loops; i >= 0 ; i--)
	;
}

static void calibrate_delay(void)
{
    unsigned long ticks;

    printf("Calibrating delay loop.. ");
    fflush(stdout);

    loops_per_sec = 1000000;
    while ((loops_per_sec <<= 1)) {
	ticks = clock();
	delay(loops_per_sec);
	ticks = clock() - ticks;
	if (ticks >= CLOCKS_PER_SEC) {
	    loops_per_sec = (loops_per_sec / ticks) * CLOCKS_PER_SEC;
	    printf("ok - %lu.%02lu BogoMips\n\n", loops_per_sec/500000,
		   (loops_per_sec/5000) % 100);
	    max_udelay = 0xffffffff/loops_per_sec;
	    return;
	}
    }
    printf("failed\n");
    exit(-1);
}

static void udelay(unsigned long usecs)
{
    while (usecs > max_udelay) {
	delay(0xffffffff/1000000);
	usecs -= max_udelay;
    }
    delay(usecs*loops_per_sec/1000000);
}

#endif /* !__KERNEL__ */

#include "hd44780.h"


#define LCD_COLS	20
#define LCD_ROWS	4

static int lcd_col = 0, lcd_row = 0;
#ifdef SCROLL_REDRAW
static char lcd_data[LCD_COLS*LCD_ROWS];
#endif /* SCROLL_REDRAW */

static const unsigned int lcd_row_offset[LCD_ROWS] = { 0, 64, 20, 84 };


#define LCD_DELAY_STROBE_US	1
#define LCD_DELAY_WRITE_US	37
#define LCD_DELAY_READ_US	6
#define LCD_DELAY_CLR		1437

static inline void lcd_delay_strobe(void) { udelay(LCD_DELAY_STROBE_US); }
static inline void lcd_delay_write(void) { udelay(LCD_DELAY_WRITE_US); }
static inline void lcd_delay_read(void) { udelay(LCD_DELAY_READ_US); }
static inline void lcd_delay_clr(void) { udelay(LCD_DELAY_CLR); }

    /*
     *  Mid-Level LCD Access
     */

static int lcd_current_width = 8;	/* The HD44780 boots up in 8-bit mode */

static const struct lcd_driver *lcd_driver = NULL;

static unsigned int lcd_stat_write = 0, lcd_stat_read = 0;


void lcd_register_driver(const struct lcd_driver *driver)
{
    lcd_driver = driver;
}

void lcd_unregister_driver(const struct lcd_driver *driver)
{
    lcd_driver = NULL;
}

void __lcd_write(u8 val, int rs)
{
    lcd_stat_write++;
    if (lcd_driver)
	if (lcd_driver->write)
	    lcd_driver->write(val, rs);
	else {
	    lcd_driver->set_rs_rw(rs, 0);
	    if (lcd_current_width == 4) {
		/* Write High Nibble */
		lcd_driver->set_data(val);
		lcd_driver->set_e(1);
		lcd_delay_strobe();
		lcd_driver->set_e(0);
		lcd_delay_strobe();
		/* Write Low Nibble */
		val <<= 4;
	    }
	    lcd_driver->set_data(val);
	    lcd_driver->set_e(1);
	    lcd_delay_strobe();
	    lcd_driver->set_e(0);
	    /* Pause */
	    lcd_delay_write();
	}
}

u8 __lcd_read(int rs)
{
    u8 val = 0;

    lcd_stat_read++;
    if (lcd_driver)
	if (lcd_driver->read)
	    val = lcd_driver->read(rs);
	else {
	    lcd_driver->set_rs_rw(rs, 1);
	    /* Read Byte or High Nibble */
	    lcd_driver->set_e(1);
	    lcd_delay_strobe();
	    val = lcd_driver->get_data();
	    lcd_driver->set_e(0);
	    if (lcd_current_width == 4) {
		val &= 0xf0;
		lcd_delay_strobe();
		/* Read Low Nibble */
		lcd_driver->set_e(1);
		lcd_delay_strobe();
		val |= lcd_driver->get_data() >> 4;
		lcd_driver->set_e(0);
	    }
	    /* Pause */
	    lcd_delay_read();
	}
    return val;
}


/* ------------------------------------------------------------------------- */


    /*
     *  High-Level LCD Access
     */

    /*
     *  LCD Commands
     */

    /*
     *  Clear Display
     */

void lcd_clr(void)
{
    lcd_write_cmd(LCD_CMD_CLR);
    lcd_delay_clr();
    lcd_col = lcd_row = 0;
#ifdef SCROLL_REDRAW
    memset(lcd_data, ' ', LCD_COLS*LCD_ROWS);
#endif /* SCROLL_REDRAW */
}

    /*
     *  Return Home
     */

void lcd_home(void)
{
    lcd_write_cmd(LCD_CMD_HOME);
    lcd_col = lcd_row = 0;
}


    /*
     *  Read Busy Flag and Address
     */

#define LCD_BUSY	(128)
#define LCD_ADDR_MASK	(127)

int lcd_is_busy(u8 *addr)
{
    u8 val = lcd_read_cmd();

    if (addr)
	*addr = val & LCD_ADDR_MASK;
    return val & LCD_BUSY ? 1 : 0;
}


void lcd_backlight(int light)
{
    if (lcd_driver)
	lcd_driver->set_bl(light);
}


    /*
     *  Device-Specific Initialization for a 20x4 LCD
     */

#ifdef __KERNEL__
static void lcd_console_write(struct console *console, const char *s,
			      unsigned count)
{
    while (count--)
	lcd_putc(*s++);
}

static struct console lcd_console = {
    name:	"hd44780",
    write:	lcd_console_write,
//  flags:	CON_PRINTBUFFER | CON_CONSDEV | CON_ENABLED,
    flags:	CON_CONSDEV | CON_ENABLED,
    index:	-1
};
#endif /* __KERNEL__ */

void lcd_init(int width)
{
#ifndef __KERNEL__
    if (loops_per_sec == 1)
	calibrate_delay();
#endif /* !__KERNEL__ */
    MOD_INC_USE_COUNT;

    switch (width) {
	case 8:
	    lcd_func(LCD_DATALEN_8, LCD_LINES_2, LCD_FONT_5x8);
	    lcd_current_width = 8;
	    break;

	case 4:
	    lcd_func(LCD_DATALEN_4, LCD_LINES_2, LCD_FONT_5x8);
	    if (lcd_current_width == 8) {
		lcd_current_width = 4;
		lcd_func(LCD_DATALEN_4, LCD_LINES_2, LCD_FONT_5x8);
	    }
	    break;
    }
    lcd_ctrl(LCD_DISP_ON, LCD_CURSOR_ON, LCD_BLINK_ON);
    lcd_mode(LCD_INC, LCD_SHIFT_OFF);
    lcd_clr();

#ifdef __KERNEL__
    if (lcd_console_messages)
	register_console(&lcd_console);
#endif /* __KERNEL__ */
}

void lcd_cleanup(void)
{
#ifdef __KERNEL__
    if (lcd_console_messages)
	unregister_console(&lcd_console);
#else
    printf("Statistics: %d writes, %d reads\n", lcd_stat_write, lcd_stat_read);
#endif /* __KERNEL__ */

    /* Return to 8-bit mode */
    lcd_func(LCD_DATALEN_8, LCD_LINES_2, LCD_FONT_5x8);
    lcd_current_width = 8;

    MOD_DEC_USE_COUNT;
}


/* ------------------------------------------------------------------------- */


    /*
     *  LCD Text Support
     *
     *  FIXME: split this off in a separate lcdtext module
     */

static void lcd_write_vec(const char *data, unsigned int n)
{
    while (n--)
	lcd_write(*data++);
}

#ifdef SCROLL_REDRAW
static void lcd_redraw_region(int sx, int sy, int width, int height)
{
    int y;

    for (y = sy; y < sy+height; y++) {
	lcd_ddram(lcd_row_offset[y]+sx);
	lcd_write_vec(&lcd_data[y*LCD_COLS+sx], width);
    }
    lcd_ddram(lcd_row_offset[lcd_row]+lcd_col);
}

static inline void lcd_redraw(void)
{
    lcd_redraw_region(0, 0, LCD_COLS, LCD_ROWS);
}

static void lcd_scroll_up(void)
{
    memmove(&lcd_data[0], &lcd_data[LCD_COLS], (LCD_ROWS-1)*LCD_COLS);
    memset(&lcd_data[(LCD_ROWS-1)*LCD_COLS], ' ', LCD_COLS);
    lcd_redraw();
}
#endif /* SCROLL_REDRAW */

#ifdef SCROLL_SHIFT
static int lcd_current_shift = 0;

static void lcd_scroll_up(void)
{
    int dir, exor, i;

    if (lcd_current_shift == 0) {
	dir = LCD_SHIFT_LEFT;
	exor = 0;
	lcd_current_shift = 20;
    } else {
	dir = LCD_SHIFT_RIGHT;
	exor = 2;
	lcd_current_shift = 0;
    }
    for (i = 0; i < LCD_COLS; i++)
	lcd_shift(LCD_SHIFT_DISP, dir);
    lcd_ddram(lcd_row_offset[exor]);
    for (i = 0; i < LCD_COLS; i++)
	lcd_write(' ');
    lcd_ddram(lcd_row_offset[1 ^ exor]);
    for (i = 0; i < LCD_COLS; i++)
	lcd_write(' ');
    lcd_ddram(lcd_row_offset[lcd_row ^ exor]+lcd_col);
}
#endif /* SCROLL_SHIFT */

#ifdef __KERNEL__
static void lcd_blank(unsigned long data)
{
    lcd_backlight(0);
}

static struct timer_list timer = {
    function:	lcd_blank
};

#define LCD_BLANK_TIMEOUT	(60*HZ)

static void lcd_kick(void)
{
    if (!timer_pending(&timer))
	lcd_backlight(1);
    mod_timer(&timer, jiffies+LCD_BLANK_TIMEOUT);
}
#endif /* !__KERNEL__ */

#ifdef SCROLL_REDRAW
void lcd_putc(char c)
{
#ifdef __KERNEL__
    lcd_kick();
#endif /* !__KERNEL__ */

    if (c == '\n') {
	lcd_col = 0;
	lcd_row++;
    } else {
	lcd_write(c);
	lcd_data[lcd_row*LCD_COLS+lcd_col++] = c;
	if (lcd_col == LCD_COLS) {
	    lcd_col = 0;
	    lcd_row++;
	}
    }
    if (lcd_row == LCD_ROWS) {
	lcd_scroll_up();
	lcd_row--;
    }
    if (lcd_col == 0)
	lcd_ddram(lcd_row_offset[lcd_row]);
}
#endif /* SCROLL_REDRAW */

#ifdef SCROLL_SHIFT
void lcd_putc(char c)
{
#ifdef __KERNEL__
    lcd_kick();
#endif /* !__KERNEL__ */

    if (c == '\n') {
	lcd_col = 0;
	lcd_row++;
    } else {
	lcd_write(c);
	if (++lcd_col == LCD_COLS) {
	    lcd_col = 0;
	    lcd_row++;
	}
    }
    if (lcd_row == LCD_ROWS) {
	lcd_scroll_up();
	lcd_row -= 2;
    }
    if (lcd_col == 0)
	lcd_ddram(lcd_row_offset[lcd_row ^ (lcd_current_shift == 0 ? 0 : 2)]);
}
#endif /* SCROLL_SHIFT */

void lcd_puts(const char *s)
{
    char c;

    while ((c = *s++))
	lcd_putc(c);
}

void lcd_printf(const char *fmt, ...)
{
    static char buf[1024];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    lcd_puts(buf);
}

#ifdef MODULE
int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{
    del_timer(&timer);
}
#endif /* MODULE */
