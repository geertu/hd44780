
/*
 *  Copyright 2000-2001 by Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  This programs is subject to the terms and conditions of the GNU General
 *  Public License
 */


    /*
     *  Physical LCD Interface
     */

struct lcd_driver {
    /* High-level Interface (may be NULL) */
    void (*write)(u8 val, int rs);
    u8 (*read)(int rs);
    /* Low-level Interface */
    void (*set_rs_rw)(int rs, int rw);
    void (*set_e)(int e);
    void (*set_bl)(int bl);
    void (*set_data)(u8 val);
    u8 (*get_data)(void);
};

extern void lcd_register_driver(const struct lcd_driver *driver);
extern void lcd_unregister_driver(const struct lcd_driver *driver);


/* ------------------------------------------------------------------------- */


    /*
     *  Mid-Level LCD Access
     */

extern void __lcd_write(u8 val, int rs);
extern u8 __lcd_read(int rs);


/* ------------------------------------------------------------------------- */


    /*
     *  High-Level LCD Access
     */

extern void lcd_init(int width);
extern void lcd_cleanup(void);


    /*
     *  LCD Commands
     */

static inline void lcd_write_cmd(u8 cmd) { __lcd_write(cmd, 0); }
static inline u8 lcd_read_cmd(void) { return __lcd_read(0); }


#define LCD_CMD_CLR	(1)
#define LCD_CMD_HOME	(2)
#define LCD_CMD_MODE	(4)
#define LCD_CMD_CTRL	(8)
#define LCD_CMD_SHIFT	(16)
#define LCD_CMD_FUNC	(32)
#define LCD_CMD_CGRAM	(64)
#define LCD_CMD_DDRAM	(128)


    /*
     *  Clear Display
     */

extern void lcd_clr(void);


    /*
     *  Return Home
     */

extern void lcd_home(void);


    /*
     *  Entry Mode Set
     */

#define LCD_INC		(2)
#define LCD_DEC		(0)
#define LCD_SHIFT_ON	(1)
#define LCD_SHIFT_OFF	(0)

static inline void lcd_mode(int inc, int shift)
{
    lcd_write_cmd(LCD_CMD_MODE | inc | shift);
}


    /*
     *  Display On/Off Control
     */

#define LCD_DISP_ON	(4)
#define LCD_DISP_OFF	(0)

#define LCD_CURSOR_ON	(2)
#define LCD_CURSOR_OFF	(0)

#define LCD_BLINK_ON	(1)
#define LCD_BLINK_OFF	(0)

static inline void lcd_ctrl(int display, int cursor, int blink)
{
    lcd_write_cmd(LCD_CMD_CTRL | display | cursor | blink);
}


    /*
     *  Cursor or Display Shift
     */

#define LCD_SHIFT_DISP	(8)
#define LCD_MOVE_CURSOR	(0)

#define LCD_SHIFT_RIGHT	(4)
#define LCD_SHIFT_LEFT	(0)

static inline void lcd_shift(int display, int right)
{
    lcd_write_cmd(LCD_CMD_SHIFT | display | right);
}


    /*
     *  Function Set
     */

#define LCD_DATALEN_8	(16)
#define LCD_DATALEN_4	(0)

#define LCD_LINES_2	(8)
#define LCD_LINES_1	(0)

#define LCD_FONT_5x10	(4)
#define LCD_FONT_5x8	(0)

static inline void lcd_func(int datalen, int lines, int font)
{
    lcd_write_cmd(LCD_CMD_FUNC | datalen | lines | font);
}


    /*
     *  Set CGRAM Address
     */

#define LCD_CGRAM_MASK	(63)

static inline void lcd_cgram(u8 a)
{
    lcd_write_cmd(LCD_CMD_CGRAM | (a & LCD_CGRAM_MASK));
}


    /*
     *  Set DDRAM Address
     */

#define LCD_DDRAM_MASK	(127)

static inline void lcd_ddram(u8 a)
{
    lcd_write_cmd(LCD_CMD_DDRAM | (a & LCD_DDRAM_MASK));
}


    /*
     *  Read Busy Flag and Address
     */

#define LCD_BUSY	(128)
#define LCD_ADDR_MASK	(127)

extern int lcd_is_busy(u8 *addr);


    /*
     *  Write Data to CG or DDRAM
     */

static inline void lcd_write(u8 val) { __lcd_write(val, 1); }


    /*
     *  Read Data from CG or DDRAM
     */

static inline u8 lcd_read(void) { return __lcd_read(1); }


    /*
     *  Backlight Control
     */

extern void lcd_backlight(int light);


    /*
     *  Device-Specific Initialization for a 20x4 LCD
     */

extern void lcd_init(int width);
extern void lcd_cleanup(void);


    /*
     *  LCD Text Support
     */

extern void lcd_putc(char c);
extern void lcd_puts(const char *s);
extern void lcd_printf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

