
/*
 *  Copyright 2000 by Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  This programs is subject to the terms and conditions of the GNU General
 *  Public License
 */


#ifdef __KERNEL__

#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/types.h>

#else /* !__KERNEL__ */

#include <sys/io.h>

typedef unsigned char u8;

#endif /* !__KERNEL__ */

#include "hd44780.h"
#include "parlcd.h"


    /*
     *  Parallel Port Register Access
     */

static inline void parport_mod_data(u8 clear, u8 set)
{
    parport_write_data((parport_read_data() & ~clear) | set);
}


static inline void parport_mod_control(u8 clear, u8 set)
{
    parport_write_control((parport_read_control() & ~clear) | set);
}


/* ------------------------------------------------------------------------- */


    /*
     *  Low-Level LCD Access
     *  
     *  These are hardware dependent!
     *
     *  Wiring:
     *
     *	    LCD		Parport (8 bit)		Parport (4 bit)
     *	    ------------------------------------------------------
     *	    RS		*SELECTIN		*SELECTIN
     *	    RW		*AUTOFD (or GND)	*AUTOFD (or GND)
     *	    E		*INIT			*INIT
     *	    Backlight	*STROBE			*STROBE
     *	    D0		D0			-
     *	    D1		D1			-
     *	    D2		D2			-
     *	    D3		D3			-
     *	    D4		D4			D4
     *	    D5		D5			D5
     *	    D6		D6			D6
     *	    D7		D7			D7
     */

#define PARLCD_RS	(PARPORT_CONTROL_SELECT)	/* inverted! */
#define PARLCD_RW	(PARPORT_CONTROL_AUTOFD)	/* inverted! */
#define PARLCD_E	(PARPORT_CONTROL_INIT)
#define PARLCD_BL	(PARPORT_CONTROL_STROBE)	/* inverted! */

#define PARLCD_RS_1	0
#define PARLCD_RS_0	PARLCD_RS
#define PARLCD_RW_1	0
#define PARLCD_RW_0	PARLCD_RW
#define PARLCD_E_1	PARLCD_E
#define PARLCD_E_0	0
#define PARLCD_BL_1	0
#define PARLCD_BL_0	PARLCD_BL


    /*
     *  Set the Register Select and Read/Write Signals
     */

static inline void parlcd_set_rs_rw(int rs, int rw)
{
    parport_mod_control(PARLCD_RS | PARLCD_RW,
			(rs ? PARLCD_RS_1 : PARLCD_RS_0) |
			(rw ? PARLCD_RW_1 : PARLCD_RW_1));
}


    /*
     *  Set the Enable Signal
     */
static inline void parlcd_set_e(int e)
{
    parport_mod_control(PARLCD_E, e ? PARLCD_E_1 : PARLCD_E_0);
}


    /*
     *  Set the Blanking Signal
     */

static inline void parlcd_set_bl(int bl)
{
    parport_mod_control(PARLCD_BL, bl ? PARLCD_BL_1 : PARLCD_BL_0);
}


    /*
     *  Set the DATA Signals
     *
     *  For 8 bit operation, all 8 bits are valid
     *  For 4 bit operation, data is transfered using the 4 MSB bits only
     */

static int parlcd_bus_width = 8;

static void parlcd_set_data(u8 val)
{
    if (parlcd_bus_width == 4)
	val |= 0x0f;		/* Drive unconnected lines high */
    parport_write_data(val);
}

static inline u8 parlcd_get_data(void)
{
    return parport_read_data();
}


static const struct lcd_driver parlcd_driver = {
    set_rs_rw:	parlcd_set_rs_rw,
    set_e:	parlcd_set_e,
    set_bl:	parlcd_set_bl,
    set_data:	parlcd_set_data,
    get_data:	parlcd_get_data
};


    /*
     *  Parallel Port LCD Control
     */

void parlcd_init(int width)
{
    parlcd_bus_width = width;
    lcd_register_driver(&parlcd_driver);
    lcd_init(width);
}

void parlcd_cleanup(void)
{
    lcd_cleanup();
    lcd_unregister_driver(&parlcd_driver);
}

#ifdef MODULE
int init_module(void)
{
    if (check_region(PARPORT_BASE, 3))
	return -EBUSY;
    request_region(PARPORT_BASE, 3, "parlcd");
    parlcd_init(8);
    lcd_puts("Welcome to your\n"
	     "Hitachi HD44780U\n"
	     "driving a 20x4 LCD!\n");
    return 0;
}

void cleanup_module(void)
{
    parlcd_cleanup();
    release_region(PARPORT_BASE, 3);
}
#endif /* MODULE */
