
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

#define usleep(x)	udelay((x))

#else /* !__KERNEL__ */

#include <stdio.h>
#include <unistd.h>

#define MOD_INC_USE_COUNT	do { } while (0)
#define MOD_DEC_USE_COUNT	do { } while (0)

typedef unsigned char u8;

#endif /* !__KERNEL__ */

#include "hd44780.h"


static int position = 0;


#define LCD_DELAY_STROBE_US	1
#define LCD_DELAY_WRITE_US	37
#define LCD_DELAY_READ_US	6

static inline void lcd_delay_strobe(void) { usleep(LCD_DELAY_STROBE_US); }
static inline void lcd_delay_write(void) { usleep(LCD_DELAY_WRITE_US); }
static inline void lcd_delay_read(void) { usleep(LCD_DELAY_READ_US); }

    /*
     *  Mid-Level LCD Access
     */

static int lcd_current_width = 8;	/* The HD44780 boots up in 8-bit mode */

static const struct lcd_driver *lcd_driver = NULL;



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
    u8 val;

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
    /* FIXME: Clear takes 1437 more us */
    usleep(1437);
    position = 0;
}

    /*
     *  Return Home
     */

void lcd_home(void)
{
    lcd_write_cmd(LCD_CMD_HOME);
    /* Home takes 1437 more us */
    usleep(1437);
    position = 0;
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
    flags:	CON_PRINTBUFFER | CON_CONSDEV | CON_ENABLED,
    index:	-1
};
#endif /* __KERNEL__ */

void lcd_init(int width)
{
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
//  FIXME
//  lcd_puts("Registering console\n");
//  register_console(&lcd_console);
#endif /* __KERNEL__ */
}

void lcd_cleanup(void)
{
#ifdef __KERNEL__
//  FIXME
//  lcd_puts("Unregistering console\n");
//  unregister_console(&lcd_console);
#endif /* __KERNEL__ */

    /* Return to 8-bit mode */
    lcd_func(LCD_DATALEN_8, LCD_LINES_2, LCD_FONT_5x8);
    lcd_current_width = 8;

    MOD_DEC_USE_COUNT;
}


/* ------------------------------------------------------------------------- */


    /*
     *  LCD Text Support
     */

static void fix_position(void)
{
    switch (position) {
	/* FIXME */
	case 20:
	    lcd_ddram(64);
	    break;
	case 40:
	    lcd_ddram(20);
	    break;
	case 60:
	    lcd_ddram(84);
	    break;
	case 80:
	    position = 0;
	    lcd_ddram(0);
	    break;
    }
}

#ifdef __KERNEL__
static void lcd_blank(unsigned long data)
{
    lcd_backlight(0);
}

static struct timer_list timer = {
    function:	lcd_blank
};

#define LCD_BLANK_TIMEOUT	(5*HZ)

static void lcd_kick(void)
{
    if (!timer_pending(&timer))
	lcd_backlight(1);
    mod_timer(&timer, jiffies+LCD_BLANK_TIMEOUT);
}
#endif /* !__KERNEL__ */

void lcd_putc(char c)
{
#ifdef __KERNEL__
    lcd_kick();
#endif /* !__KERNEL__ */

    if (c == '\n') {
	int i = 20-position%20;
	while (i--)
	    lcd_putc(' ');
    } else {
	lcd_write(c);
	position++;
	fix_position();
    }
}

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
}
#endif /* MODULE */
