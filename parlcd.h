
/*
 *  Copyright 2000 by Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  This programs is subject to the terms and conditions of the GNU General
 *  Public License
 */


    /*
     *  Parallel Port Register Definitions
     */

#define PARPORT_BASE		0x378

#define PARPORT_DATA		(PARPORT_BASE+0)
#define PARPORT_STATUS		(PARPORT_BASE+1)
#define PARPORT_CONTROL		(PARPORT_BASE+2)

#define PARPORT_CONTROL_STROBE	0x1
#define PARPORT_CONTROL_AUTOFD	0x2
#define PARPORT_CONTROL_INIT	0x4
#define PARPORT_CONTROL_SELECT	0x8

#define PARPORT_STATUS_ERROR	0x8
#define PARPORT_STATUS_SELECT	0x10
#define PARPORT_STATUS_PAPEROUT	0x20
#define PARPORT_STATUS_ACK	0x40
#define PARPORT_STATUS_BUSY	0x80


    /*
     *  Parallel Port Register Access
     */

static inline u8 parport_read_data(void) { return inb(PARPORT_DATA); }
static inline void parport_write_data(u8 val) { outb(val, PARPORT_DATA); }
static inline u8 parport_read_status(void) { return inb(PARPORT_STATUS); }
static inline void parport_write_status(u8 val) { outb(val, PARPORT_STATUS); }
static inline u8 parport_read_control(void) { return inb(PARPORT_CONTROL); }
static inline void parport_write_control(u8 val) { outb(val, PARPORT_CONTROL); }


    /*
     *  Parallel Port LCD Control
     */

extern void parlcd_init(int width);
extern void parlcd_cleanup(void);

