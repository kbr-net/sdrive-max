//moved to display.c
//#include "font.c"


#define TFT_data_out_port_low                                                            PORTB
#define TFT_data_out_port_high                                                           PORTD

#define TFT_data_in_port_low                                                             PINB
#define TFT_data_in_port_high                                                            PIND

#define TFT_port_config_low                                                              DDRB
#define TFT_port_config_high                                                             DDRD

#define TFT_ctrl_ddr DDRC
#define TFT_RST_pin                                                                      PORTC4
#define TFT_CS_pin                                                                       PORTC3
#define TFT_RD_pin                                                                       PORTC0
#define TFT_WR_pin                                                                       PORTC1
#define TFT_RS_pin                                                                       PORTC2

//TFT_RD_pin pull up 3.3V

#define TFT_ctrl_port PORTC
#define TFT_RST_pin_dir                                                                  PINC4
#define TFT_CS_pin_dir                                                                   PINC3
#define TFT_RD_pin_dir                                                                   PINC0
#define TFT_WR_pin_dir                                                                   PINC1
#define TFT_RS_pin_dir                                                                   PINC2

//color definitions
#define atari_bg 0x257b
#define window_bg 0x528a

#define White                                                                            0xFFFF
#define Black                                                                            0x0000
//#define Grey                                                                             0xF7DE
#define Grey                                                                             0xCE59
#define Grey2                                                                            0x8430
#define Light_Grey                                                                       0xC618
#define Dark_Grey                                                                        0x8410
#define Purple                                                                           0xF81F
#define Grey_Blue                                                                        0x5458
#define Blue                                                                             0x001F
#define Dark_Blue                                                                        0x01CF
#define Light_Blue                                                                       0x051F
#define Light_Blue_2                                                                     0x7D7C
#define Red                                                                              0xF800
#define Green                                                                            0x07E0
#define Dark_Green                                                                       0x06E0
#define Cyan                                                                             0x7FFF
#define Yellow                                                                           0xFFE0
#define Orange                                                                           0xFC08
#define Magenta                                                                          0xF81F

/*
#define TRUE                                                                             1
#define FALSE                                                                            0
*/

#define YES                                                                              1
#define NO                                                                               0

#define ON                                                                               1
#define OFF                                                                              0

#define HIGH                                                                             1
#define LOW                                                                              0

#define input                                                                   	0
#define output                                                                          1 
#define low                                                                              0
#define high                                                                             1

#define DAT                                                                              1
#define CMD                                                                              0

#define SQUARE                                                                           0
#define ROUND                                                                            1

#define delay_ms(x) _delay_ms(x)

#define ILI9341_NOP                                                                      0x00
#define ILI9341_RESET                                                                    0x01
#define ILI9341_READ_DISPLAY_IDENTIFICATION_INFORMATION                                  0x04
#define ILI9341_READ_DISPLAY_STATUS                                                      0x09
#define ILI9341_READ_DISPLAY_POWER_MODE                                                  0x0A
#define ILI9341_READ_DISPLAY_MADCTL                                                      0x0B
#define ILI9341_READ_DISPLAY_PIXEL_FORMAT                                                0x0C
#define ILI9341_READ_DISPLAY_IMAGE_FORMAT                                                0x0D
#define ILI9341_READ_DISPLAY_SIGNAL_MODE                                                 0x0E
#define ILI9341_READ_DISPLAY_SELF_DIAGNOSTIC_RESULT                                      0x0F
#define ILI9341_ENTER_SLEEP_MODE                                                         0x10
#define ILI9341_SLEEP_OUT                                                                0x11
#define ILI9341_PARTIAL_MODE_ON                                                          0x12
#define ILI9341_NORMAL_DISPLAY_MODE_ON                                                   0x13
#define ILI9341_DISPLAY_INVERSION_OFF                                                    0x20
#define ILI9341_DISPLAY_INVERSION_ON                                                     0x21
#define ILI9341_GAMMA                                                                    0x26
#define ILI9341_DISPLAY_OFF                                                              0x28
#define ILI9341_DISPLAY_ON                                                               0x29
#define ILI9341_COLUMN_ADDR                                                              0x2A
#define ILI9341_PAGE_ADDR                                                                0x2B
#define ILI9341_GRAM                                                                     0x2C
#define ILI9341_COLOR_SET                                                                0x2D
#define ILI9341_MEMORY_READ                                                              0x2E
#define ILI9341_PARTIAL_AREA                                                             0x30
#define ILI9341_VERTICAL_SCROLLING_DEFINITION                                            0x33
#define ILI9341_TEARING_EFFECT_LINE_OFF                                                  0x34
#define ILI9341_TEARING_EFFECT_LINE_ON                                                   0x35
#define ILI9341_MAC                                                                      0x36
#define ILI9341_VERTICAL_SCROLLING_START_ADDRESS                                         0x37
#define ILI9341_IDLE_MODE_OFF                                                            0x38
#define ILI9341_IDLE_MODE_ON                                                             0x39
#define ILI9341_PIXEL_FORMAT                                                             0x3A
#define ILI9341_WMC                                                                      0x3C
#define ILI9341_RMC                                                                      0x3E
#define ILI9341_SET_TEAR_SCANLINE                                                        0x44
#define ILI9341_WDB                                                                      0x51
#define ILI9341_READ_DISPLAY_BRIGHTNESS                                                  0x52
#define ILI9341_WCD                                                                      0x53
#define ILI9341_READ_CTRL_DISPLAY                                                        0x54
#define ILI9341_WCABC                                                                    0x55
#define ILI9341_RCABC                                                                    0x56
#define ILI9341_WCABCMB                                                                  0x5E
#define ILI9341_RCABCMB                                                                  0x5F
#define ILI9341_RGB_INTERFACE                                                            0xB0
#define ILI9341_FRC                                                                      0xB1
#define ILI9341_FRAME_CTRL_NM                                                            0xB2
#define ILI9341_FRAME_CTRL_IM                                                            0xB3
#define ILI9341_FRAME_CTRL_PM                                                            0xB4
#define ILI9341_BPC                                                                      0xB5
#define ILI9341_DFC                                                                      0xB6
#define ILI9341_ENTRY_MODE_SET                                                           0xB7
#define ILI9341_BACKLIGHT_CONTROL_1                                                      0xB8
#define ILI9341_BACKLIGHT_CONTROL_2                                                      0xB9
#define ILI9341_BACKLIGHT_CONTROL_3                                                      0xBA
#define ILI9341_BACKLIGHT_CONTROL_4                                                      0xBB
#define ILI9341_BACKLIGHT_CONTROL_5                                                      0xBC
#define ILI9341_BACKLIGHT_CONTROL_6                                                      0xBD
#define ILI9341_BACKLIGHT_CONTROL_7                                                      0xBE
#define ILI9341_BACKLIGHT_CONTROL_8                                                      0xBF
#define ILI9341_POWER1                                                                   0xC0
#define ILI9341_POWER2                                                                   0xC1
#define ILI9341_VCOM1                                                                    0xC5
#define ILI9341_VCOM2                                                                    0xC7
#define ILI9341_POWERA                                                                   0xCB
#define ILI9341_POWERB                                                                   0xCF
#define ILI9341_READ_ID1                                                                 0xDA
#define ILI9341_READ_ID2                                                                 0xDB
#define ILI9341_READ_ID3                                                                 0xDC
#define ILI9341_PGAMMA                                                                   0xE0
#define ILI9341_NGAMMA                                                                   0xE1
#define ILI9341_DTCA                                                                     0xE8
#define ILI9341_DTCB                                                                     0xEA
#define ILI9341_POWER_SEQ                                                                0xED
#define ILI9341_3GAMMA_EN                                                                0xF2
#define ILI9341_INTERFACE                                                                0xF6
#define ILI9341_PRC                                                                      0xF7

#define PORTRAIT_1                                                                       0
#define PORTRAIT_2                                                                       1
#define LANDSCAPE_1                                                                      2
#define LANDSCAPE_2                                                                      3

#define X_max                                                                            240
#define Y_max                                                                            320

/* moved to .c
#define pixels (X_max * Y_max)

unsigned int MAX_X = X_max;
unsigned int MAX_Y = Y_max;
*/

void TFT_init();
void TFT_on();
void TFT_off();
void TFT_sleep_on();
void TFT_sleep_off();
void TFT_GPIO_init();
void TFT_reset();
void TFT_write_bus(unsigned char value);
void TFT_write_cmd(unsigned char value);
void TFT_write_data(unsigned int value);
void TFT_write(unsigned char value);
void TFT_write_REG_DATA(unsigned char reg, unsigned char data_value);
unsigned int TFT_getID();
void TFT_set_rotation(unsigned char value);
void TFT_set_display_window(unsigned int x_pos1, unsigned int y_pos1, unsigned int x_pos2, unsigned int y_pos2);
void TFT_scroll_init(unsigned int tfa, unsigned int vsa, unsigned int bfa);
void TFT_scroll(unsigned int scroll);
void TFT_fill(unsigned int colour);
void TFT_fill_area(signed int x1, signed int y1, signed int x2, signed int y2, unsigned int colour);
unsigned int TFT_BGR2RGB(unsigned int colour);
unsigned int RGB565_converter(unsigned char r, unsigned char g, unsigned char b);
void swap(signed int *a, signed int *b);
void Draw_Pixel(unsigned int x_pos, unsigned int y_pos, unsigned int colour);
void Draw_Point(unsigned int x_pos, unsigned int y_pos, unsigned char pen_width, unsigned int colour);
void Draw_Line(signed int x1, signed int y1, signed int x2, signed int y2, unsigned int colour);
void Draw_V_Line(signed int x1, signed int y1, signed int y2, unsigned colour);
void Draw_H_Line(signed int x1, signed int x2, signed int y1, unsigned colour);
void Draw_Triangle(signed int x1, signed int y1, signed int x2, signed int y2, signed int x3, signed int y3, unsigned char fill, unsigned int colour);
void Draw_Rectangle(signed int x1, signed int y1, signed int x2, signed int y2, unsigned char fill, unsigned char type, unsigned int colour, unsigned int back_colour);
void Draw_H_Bar(signed int x1, signed int x2, signed int y1, signed int bar_width, signed int bar_value, unsigned int border_colour, unsigned int bar_colour, unsigned int back_colour, unsigned char border);
void Draw_V_Bar(signed int x1, signed int y1, signed int y2, signed int bar_width, signed int bar_value, unsigned int border_colour, unsigned int bar_colour, unsigned int back_colour, unsigned char border);
void Draw_Circle(signed int xc, signed int yc, signed int radius, unsigned char fill, unsigned int colour);
void Draw_Font_Pixel(unsigned int x_pos, unsigned int y_pos, unsigned int colour, unsigned char pixel_size);
void print_char(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, unsigned char ch);
void print_str_P(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, const char *ch);
void print_str(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, char *ch);
void print_strn(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, char *ch, unsigned char n);
void set_text_pos (unsigned int x, unsigned int y);
void print_ln_P(unsigned char font_size, unsigned int colour, unsigned int back_colour, const char *ch);
void print_ln(unsigned char font_size, unsigned int colour, unsigned int back_colour, char *ch);
void print_C(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, signed int value);
void print_I(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, signed int value);
void print_D(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, unsigned int value, unsigned char points);
void print_F(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, float value, unsigned char points);
void Draw_BMP(signed int x_pos1, signed int y_pos1, signed int x_pos2, signed int y_pos2, const char * bitmap);
