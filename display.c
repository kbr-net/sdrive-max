#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "display.h"
#include "font.h"

unsigned int MAX_X = X_max;
unsigned int MAX_Y = Y_max;

#if defined HX8347G || defined HX8347I
struct init_cmds {
	unsigned char cmd;
	unsigned char data;
};

const struct init_cmds hx_init_cmds[] PROGMEM = {
	//set later after scroll init
	//{0x01, 0x08},	//scroll mode on
	{0x17, 0x05},	//COLMOD 16bit
	{0x18, 0x34},	//frame rate idle/normal 50Hz/60Hz
	{0x19, 0x01},	//enable oscillator
	{0x1f, 0xd4},	//set power on and exit standby mode
	//{0x36, 0x00},	//characteristic SS, GS, BGR
	{0x28, 0x3c}	//gate output and display on
};
#endif

#if defined ILI9325
struct init_cmds {
	unsigned char cmd;
	unsigned short data;
};

const struct init_cmds ili9325_init_cmds[] PROGMEM = {
	{0xe5, 0x78f0},	// set SRAM internal timing
	{0x01, 0x0000},	// set Driver Output Control
	{0x02, 0x0300},	// set 1 line inversion
	{0x03, 0x1030},	// set GRAM write direction and BGR=1.
	{0x04, 0x0000},	// Resize register
	{0x05, 0x0000},	// .kbv 16bits Data Format Selection
	{0x08, 0x0207},	// set the back porch and front porch
	{0x09, 0x0000},	// set non-display area refresh cycle ISC[3:0]
	{0x0a, 0x0000},	// FMARK function
	{0x0c, 0x0000},	// RGB interface setting
	{0x0d, 0x0000},	// Frame marker Position
	{0x0f, 0x0000},	// RGB interface polarity
	// ----------- Power On sequence -----------
	{0x10, 0x0000},	// SAP, BT[3:0], AP, DSTB, SLP, STB
	{0x11, 0x0007},	// DC1[2:0], DC0[2:0], VC[2:0]
	{0x12, 0x0000},	// VREG1OUT voltage
	{0x13, 0x0000},	// VDV[4:0] for VCOM amplitude
	{0x07, 0x0001},
	{0x00, 200},	//delay 200ms, Dis-charge capacitor power voltage
	{0x10, 0x1690},	// SAP=1, BT=6, APE=1, AP=1, DSTB=0, SLP=0, STB=0
	{0x11, 0x0227},	// DC1=2, DC0=2, VC=7
	{0x00, 50},
	{0x12, 0x000d},	// VCIRE=1, PON=0, VRH=5
	{0x00, 50},
	{0x13, 0x1200},	// VDV=28 for VCOM amplitude
	{0x29, 0x000a},	// VCM=10 for VCOMH
	{0x2b, 0x000d},	// Set Frame Rate
	{0x00, 50},
	//not needed here, is set on each write
	//{0x20, 0x0000},	// GRAM horizontal Address
	//{0x21, 0x0000},	// GRAM Vertical Address
	// ----------- Adjust the Gamma Curve ----------
	{0x30, 0x0000},
	{0x31, 0x0404},
	{0x32, 0x0003},
	{0x35, 0x0405},
	{0x36, 0x0808},
	{0x37, 0x0407},
	{0x38, 0x0303},
	{0x39, 0x0707},
	{0x3c, 0x0504},
	{0x3d, 0x0808},
	//------------------ Set GRAM area ---------------
	// Gate Scan Line GS=0 [0xA700]
	// Gate Scan Line GS=320 [0x2700]
	{0x60, 0x2700},
	{0x61, 0x0001},	// NDL,VLE, REV .kbv
	{0x6a, 0x0000},	// set scrolling line
	//-------------- Partial Display Control ---------
	{0x80, 0x0000},
	{0x81, 0x0000},
	{0x82, 0x0000},
	{0x83, 0x0000},
	{0x84, 0x0000},
	{0x85, 0x0000},
	//-------------- Panel Control -------------------
	{0x90, 0x0010},
	{0x92, 0x0000},
	{0x93, 0x0003},
	{0x95, 0x0110},
	{0x97, 0x0000},
	{0x98, 0x0000}
	//not needed here, is set on TFT_on()
	//{0x07, 0x0133}	// 262K color and display ON
};
#endif

void TFT_init()
{
    TFT_GPIO_init();

    TFT_reset();
#ifdef ILI9329
    delay_ms(200);
//    TFT_write_cmd(ILI9341_RESET);
//    TFT_write_cmd(ILI9341_DISPLAY_INVERSION_ON);
//    TFT_write(0x00);
#elif ILI9340
	delay_ms(200);
    TFT_write_cmd(ILI9341_DISPLAY_INVERSION_ON);
//    TFT_write(0x00);
#else
    delay_ms(60);
#endif

#if defined HX8347G || defined HX8347I
    unsigned char i;
    for(i = 0; i < sizeof(hx_init_cmds)/2; i++) {
	TFT_write_cmd(pgm_read_byte(&hx_init_cmds[i].cmd));
	TFT_write(pgm_read_byte(&hx_init_cmds[i].data));
    }
    //TFT_write_cmd(0x22);	//GRAM
#elif defined ILI9325
    unsigned char i;
    unsigned char cmd;
    unsigned short data;

    for(i = 0; i < sizeof(ili9325_init_cmds)/3; i++) {
	cmd = pgm_read_byte(&ili9325_init_cmds[i].cmd);
	data = pgm_read_word(&ili9325_init_cmds[i].data);
	if(cmd == 0) {
		while(data--)
			delay_ms(1);
		continue;
	}
	TFT_write_cmd(cmd);
	TFT_write_data(data);
    }

    // ------------- Initialization Done -------------


#else
    TFT_write_cmd(ILI9341_PIXEL_FORMAT);
    TFT_write(0x55);
    TFT_write_cmd(ILI9341_SLEEP_OUT);
    delay_ms(100);

    //TFT_write_cmd(ILI9341_DISPLAY_ON);
    //TFT_write_cmd(ILI9341_GRAM);
#endif
}

void TFT_on() {
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x28);	//gate output and display on
    TFT_write(0x3c);
    //TFT_write_cmd(0x22);	//GRAM
#elif defined ILI9325
    TFT_write_cmd(0x07);
    TFT_write_data(0x0133);
#else
    TFT_write_cmd(ILI9341_DISPLAY_ON);
    //TFT_write_cmd(ILI9341_GRAM);
#endif
}

void TFT_off() {
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x28);	//gate output and display off
    TFT_write(0x00);
    //TFT_write_cmd(0x22);	//GRAM
#elif defined ILI9325
    TFT_write_cmd(0x07);
    TFT_write_data(0x0000);
#else
    TFT_write_cmd(ILI9341_DISPLAY_OFF);
    //TFT_write_cmd(ILI9341_GRAM);
#endif
}

// enter/release display sleep mode
void TFT_sleep_on() {
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x1f);	//set standby mode
    TFT_write(0xd5);
#elif defined ILI9325
    // SAP=1, BT=6, APE=1, AP=1, DSTB=0, SLP=1, STB=0
    TFT_write_cmd(0x10);
    TFT_write_data(0x1692);
#else
    TFT_write_cmd(ILI9341_ENTER_SLEEP_MODE);
#endif
    delay_ms(120);
}

void TFT_sleep_off() {
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x1f);	//set power on and exit standby mode
    TFT_write(0xd4);
#elif defined ILI9325
    // SAP=1, BT=6, APE=1, AP=1, DSTB=0, SLP=0, STB=0
    TFT_write_cmd(0x10);
    TFT_write_data(0x1690);
#else
    TFT_write_cmd(ILI9341_SLEEP_OUT);
#endif
    delay_ms(5);
}

void TFT_GPIO_init()
{
    TFT_port_config_low |= 0x03;
    TFT_port_config_high |= 0xFC;

    //output
    TFT_ctrl_ddr |=	(1 << TFT_RST_pin_dir) |
			(1 << TFT_CS_pin_dir) |
			(1 << TFT_RD_pin_dir) |
			(1 << TFT_WR_pin_dir) |
			(1 << TFT_RS_pin_dir);

    //RD/WR initial high
    TFT_ctrl_port |= (1 << TFT_CS_pin) | (1 << TFT_RD_pin) | (1 << TFT_WR_pin);

    delay_ms(10);
}


void TFT_reset()
{
    TFT_ctrl_port &= ~(1 << TFT_RST_pin);
    delay_ms(15);
    TFT_ctrl_port |= (1 << TFT_RST_pin);
    delay_ms(15);
}


void TFT_write_bus(unsigned char value)
{
    TFT_data_out_port_high = (TFT_data_out_port_high & 0x03) | (value & 0xFC);
    TFT_data_out_port_low = (TFT_data_out_port_low & 0xFC) | (value & 0x03);

    TFT_ctrl_port &= ~(1 << TFT_WR_pin);
    TFT_ctrl_port |= (1 << TFT_WR_pin);
}

unsigned char TFT_read_bus()
{
    unsigned char value;
    TFT_ctrl_port &= ~(1 << TFT_RD_pin);
    delay_ms(20);

    value = (TFT_data_in_port_high & 0xFC) | (TFT_data_in_port_low & 0x03);

    TFT_ctrl_port |= (1 << TFT_RD_pin);
    return(value);
}

void TFT_write_cmd(unsigned char value)
{
    TFT_ctrl_port &= ~(1 << TFT_RS_pin);
    TFT_ctrl_port &= ~(1 << TFT_CS_pin);
    TFT_write_bus(value);
    TFT_ctrl_port |= (1 << TFT_CS_pin);
}

void TFT_write_data(unsigned int value)
{
    TFT_ctrl_port |= 1 << TFT_RS_pin;
    TFT_ctrl_port &= ~(1 << TFT_CS_pin);
    TFT_write_bus(value>>8);
    TFT_write_bus(value);
    TFT_ctrl_port |= (1 << TFT_CS_pin);
}

unsigned int TFT_read_data()
{
    unsigned int value;

    TFT_port_config_low &= ~0x03;	//data pins to input
    TFT_port_config_high &= ~0xFC;

    TFT_ctrl_port |= 1 << TFT_RS_pin;
    TFT_ctrl_port &= ~(1 << TFT_CS_pin);
    value = (TFT_read_bus() << 8) | TFT_read_bus();
    TFT_ctrl_port |= (1 << TFT_CS_pin);

    TFT_port_config_low |= 0x03;	//data pins to output
    TFT_port_config_high |= 0xFC;

    return(value);
}

void TFT_write(unsigned char value)
{
    TFT_ctrl_port |= 1 << TFT_RS_pin;
    TFT_ctrl_port &= ~(1 << TFT_CS_pin);
    TFT_write_bus(value);
    TFT_ctrl_port |= (1 << TFT_CS_pin);
}


void TFT_write_REG_DATA(unsigned char reg, unsigned char data_value)
{
    TFT_write_cmd(reg);
    TFT_write(data_value);
}

unsigned int TFT_getID()
{
#if defined HX8347G || defined HX8347I || defined ILI9325
	TFT_write_cmd(0x00);	//READ-ID
#else
	TFT_write_cmd(0xD3);	//READ-ID4
#endif
	TFT_read_data();	//drop first int
	return(TFT_read_data());
}

void TFT_set_rotation(unsigned char value)
{
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x16);
#elif defined ILI9325
    //nothing there
#else
    TFT_write_cmd(ILI9341_MAC);
#endif

    switch(value)
    {
        case PORTRAIT_1:
        {
#if defined ILI9329 || defined HX8347G
            TFT_write(0x08);
#elif defined ILI9325
    // Gate Scan Line GS=0 [0xA700]
    TFT_write_cmd(0x60);
    TFT_write_data(0xa700);
    // set Driver Output Control SS=1
    TFT_write_cmd(0x01);
    TFT_write_data(0x0100);
#else
            TFT_write(0x48);
#endif
            break;
        }
        case PORTRAIT_2:
        {
#if defined ILI9329 || defined HX8347G
            TFT_write(0xd8);
#elif defined ILI9325
            // Gate Scan Line GS=320 [0xA700]
            TFT_write_cmd(0x60);
            TFT_write_data(0x2700);
            // set Driver Output Control SS=0
            TFT_write_cmd(0x01);
            TFT_write_data(0x0000);
#else
            TFT_write(0x98);
#endif
            break;
        }
/* not used yet, we support only portrait mode
        case LANDSCAPE_1:
        {
            TFT_write(0x28);
            break;
        }
        case LANDSCAPE_2:
        {
            TFT_write(0xE8);
            break;
        }
*/
    }

/* not used yet
    if((value == PORTRAIT_1) || (value == PORTRAIT_2))
    {
        MAX_X = X_max;
        MAX_Y = Y_max;
    }

    if((value == LANDSCAPE_1) || (value == LANDSCAPE_2))
    {
        MAX_X = Y_max;
        MAX_Y = X_max;
    }
*/
}


void TFT_set_display_window(unsigned int x_pos1, unsigned int y_pos1, unsigned int x_pos2, unsigned int y_pos2)
{
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x02);	//col start
    TFT_write(x_pos1>>8);
    TFT_write_cmd(0x03);
    TFT_write(x_pos1);

    TFT_write_cmd(0x04);	//col end
    TFT_write(x_pos2>>8);
    TFT_write_cmd(0x05);
    TFT_write(x_pos2);

    TFT_write_cmd(0x06);	//row start
    TFT_write(y_pos1>>8);
    TFT_write_cmd(0x07);
    TFT_write(y_pos1);

    TFT_write_cmd(0x08);	//row end
    TFT_write(y_pos2>>8);
    TFT_write_cmd(0x09);
    TFT_write(y_pos2);

    TFT_write_cmd(0x22);	//GRAM

#elif defined ILI9325
    TFT_write_cmd(0x50);   //Horizontal adress start
    TFT_write_data(x_pos1);
    TFT_write_cmd(0x51);   //Horizontal adress stop
    TFT_write_data(x_pos2);
    TFT_write_cmd(0x52);   //Vertical adress start
    TFT_write_data(y_pos1);
    TFT_write_cmd(0x53);   //Vertical adress stop
    TFT_write_data(y_pos2);
    //----------- Set Cursor ------------
    TFT_write_cmd(0x20);   //Vertical adress stop
    TFT_write_data(x_pos1);
    TFT_write_cmd(0x21);   //Vertical adress stop
    TFT_write_data(y_pos1);
    TFT_write_cmd(0x22);   //Write data to

#else
    TFT_write_cmd(ILI9341_COLUMN_ADDR);
    TFT_write_data(x_pos1);
    TFT_write_data(x_pos2);

    TFT_write_cmd(ILI9341_PAGE_ADDR);
    TFT_write_data(y_pos1);
    TFT_write_data(y_pos2);

    TFT_write_cmd(ILI9341_GRAM);
#endif
}

void TFT_scroll_init(unsigned int tfa, unsigned int vsa, unsigned int bfa) {
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x0e);	//TFA
    TFT_write(tfa>>8);
    TFT_write_cmd(0x0f);
    TFT_write(tfa);

    TFT_write_cmd(0x10);	//VSA
    TFT_write(vsa>>8);
    TFT_write_cmd(0x11);
    TFT_write(vsa);

    TFT_write_cmd(0x12);	//BFA
    TFT_write(bfa>>8);
    TFT_write_cmd(0x13);
    TFT_write(bfa);

    TFT_write_cmd(0x01);	//scroll mode on
    TFT_write(0x08);
#elif defined ILI9325
    //nothing
    //not supported by this chip
#else
    TFT_write_cmd(ILI9341_VERTICAL_SCROLLING_DEFINITION);
    TFT_write_data(tfa);	//TFA
    TFT_write_data(vsa);	//VSA
    TFT_write_data(bfa);	//BFA
#endif
}

void TFT_scroll(unsigned int scroll) {
#if defined HX8347G || defined HX8347I
    TFT_write_cmd(0x14);
    TFT_write(scroll>>8);
    TFT_write_cmd(0x15);
    TFT_write(scroll);
#elif defined ILI9325
    //nothing
    //not supported by this chip
#else
    TFT_write_cmd(ILI9341_VERTICAL_SCROLLING_START_ADDRESS);
    TFT_write_data(scroll);
#endif
}

void TFT_fill(unsigned int colour)
{
    unsigned long index = (unsigned long) X_max * (unsigned long) Y_max;

    TFT_set_display_window(0, 0, (MAX_X - 1), (MAX_Y - 1));

    while(index)
    {
       TFT_write_data(colour);
       index--;
    };
}


void TFT_fill_area(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int colour)
{
    unsigned long index;

    index = (x2 - x1 + 1)*(y2 - y1 + 1);
    TFT_set_display_window(x1, y1, x2, y2);

    while(index)
    {
       TFT_write_data(colour);
       index--;
    };
}


unsigned int TFT_BGR2RGB(unsigned int colour)
{
     unsigned int r;
     unsigned int g;
     unsigned int b;
     unsigned int rgb_colour;

     b = ((colour >> 0)  & 0x1F);
     g = ((colour >> 5)  & 0x3F);
     r = ((colour >> 11) & 0x1F);

     rgb_colour = ((b << 11) + (g << 5) + (r << 0));

     return rgb_colour;
}


unsigned int RGB565_converter(unsigned char r, unsigned char g, unsigned char b)
{
    return (((((unsigned int)r) >> 3) << 11) | ((((unsigned int)g) >> 2) << 5) | (((unsigned int)b) >> 3));
}


void swap(signed int *a, signed int *b)
{
    signed int temp;

    temp = *b;
    *b = *a;
    *a = temp;
}


void Draw_Pixel(unsigned int x_pos, unsigned int y_pos, unsigned int colour)
{
    TFT_set_display_window(x_pos, y_pos, x_pos, y_pos);
    TFT_write_data(colour);
}


void Draw_Point(unsigned int x_pos, unsigned int y_pos, unsigned char pen_width, unsigned int colour)
{
    Draw_Circle(x_pos, y_pos, pen_width, YES, colour);
}


void Draw_Line(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int colour)
{
    unsigned int dx;
    unsigned int dy;
    unsigned int stepx = 1;
    unsigned int stepy = 1;
    signed int fraction;

    dx = (x2 - x1);
    dy = (y2 - y1);

    dx <<= 0x01;
    dy <<= 0x01;

    Draw_Pixel(x1, y1, colour);

    if(dx > dy)
    {
        fraction = (dy - (dx >> 1));
        while(x1 != x2)
        {
            if(fraction >= 0)
            {
                y1 += stepy;
                fraction -= dx;
            }
            x1 += stepx;
            fraction += dy;

            Draw_Pixel(x1, y1, colour);
        }
    }
    else
    {
        fraction = (dx - (dy >> 1));

        while(y1 != y2)
        {
            if (fraction >= 0)
            {
                x1 += stepx;
                fraction -= dy;
            }
            y1 += stepy;
            fraction += dx;
            Draw_Pixel(x1, y1, colour);
        }
    }
}


void Draw_V_Line(unsigned int x1, unsigned int y1, unsigned int y2, unsigned colour)
{
    TFT_fill_area(x1, y1, x1, y2, colour);
}


void Draw_H_Line(unsigned int x1, unsigned int x2, unsigned int y1, unsigned colour)
{
    TFT_fill_area(x1, y1, x2, y1, colour);
}


void Draw_Triangle(signed int x1, signed int y1, signed int x2, signed int y2, signed int x3, signed int y3, unsigned char fill, unsigned int colour)
{
    signed int a = 0;
    signed int b = 0;
    signed int sa = 0;
    signed int sb = 0;
    signed int yp = 0;
    signed int last = 0;
    signed int dx12 = 0;
    signed int dx23 = 0;
    signed int dx13 = 0;
    signed int dy12 = 0;
    signed int dy23 = 0;
    signed int dy13 = 0;

    switch(fill)
    {
        case YES:
        {
            if(y1 > y2)
            {
                swap(&y1, &y2);
                swap(&x1, &x2);
            }
            if(y2 > y3)
            {
                swap(&y3, &y2);
                swap(&x3, &x2);
            }
            if(y1 > y2)
            {
                swap(&y1, &y2);
                swap(&x1, &x2);
            }

            if(y1 == y3)
            {
                a = b = x1;

                if(x2 < a)
                {
                    a = x2;
                }
                else if(x2 > b)
                {
                    b = x2;
                }
                if(x2 < a)
                {
                    a = x3;
                }
                else if(x3 > b)
                {
                    b = x3;
                }

                Draw_H_Line(a, (a + (b - (a + 1))), y1, colour);
                return;
            }

            dx12 = (x2 - x1);
            dy12 = (y2 - y1);
            dx13 = (x3 - x1);
            dy13 = (y3 - y1);
            dx23 = (x3 - x2);
            dy23 = (y3 - y2);
            sa = 0,
            sb = 0;

            if(y2 == y3)
            {
                last = y2;
            }
            else
            {
                last = (y2 - 1);
            }

            for(yp = y1; yp <= last; yp++)
            {
                a = (x1 + (sa / dy12));
                b = (x1 + (sb / dy13));
                sa += dx12;
                sb += dx13;
                if(a > b)
                {
                    swap(&a, &b);
                }
                Draw_H_Line(a, (a + (b - (a + 1))), yp, colour);
            }

            sa = (dx23 * (yp - y2));
            sb = (dx13 * (yp - y1));
            for(; yp <= y3; yp++)
            {
                a = (x2 + (sa / dy23));
                b = (x1 + (sb / dy13));
                sa += dx23;
                sb += dx13;

                if(a > b)
                {
                    swap(&a, &b);
                }
                Draw_H_Line(a, (a + (b - (a + 1))), yp, colour);
            }


            break;
        }
        default:
        {
            Draw_Line(x1, y1, x2, y2, colour);
            Draw_Line(x2, y2, x3, y3, colour);
            Draw_Line(x3, y3, x1, y1, colour);
            break;
        }
    }
}


void Draw_Rectangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned char fill, unsigned char type, unsigned int colour, unsigned int back_colour)
{
     switch(fill)
     {
         case YES:
         {
	     TFT_fill_area(x1, y1, x2, y2, colour);
             break;
         }
         default:
         {
             Draw_V_Line(x1, y1, y2, colour);
             Draw_V_Line(x2, y1, y2, colour);
             Draw_H_Line(x1, x2, y1, colour);
             Draw_H_Line(x1, x2, y2, colour);
             break;
         }
     }

     if(type != SQUARE)
     {
         Draw_Pixel(x1, y1, back_colour);
         Draw_Pixel(x1, y2, back_colour);
         Draw_Pixel(x2, y1, back_colour);
         Draw_Pixel(x2, y2, back_colour);
     }
}


void Draw_H_Bar(unsigned int x1, unsigned int x2, unsigned int y1, unsigned int bar_width, unsigned int bar_value, unsigned int border_colour, unsigned int bar_colour, unsigned int back_colour, unsigned char border)
{
    switch(border)
    {
        case YES:
        {
            Draw_Rectangle((x1 + 1), (y1 + 1), (x1 + bar_value), (y1 + bar_width - 1), YES, SQUARE, bar_colour, back_colour);
            Draw_Rectangle((x1 + bar_value + 1), (y1 + 1), (x2 - 1), (y1 + bar_width - 1), YES, SQUARE, back_colour, back_colour);
            Draw_Rectangle(x1, y1, x2, (y1 + bar_width), NO, SQUARE, border_colour, back_colour);
            break;
        }
        default:
        {
            Draw_Rectangle(x1, y1, (x1 + bar_value), (y1 + bar_width), YES, SQUARE, bar_colour, back_colour);
            Draw_Rectangle((x1 + bar_value), y1, x2, (y1 + bar_width), YES, SQUARE, back_colour, back_colour);
            break;
        }
    }
}


void Draw_V_Bar(unsigned int x1, unsigned int y1, unsigned int y2, unsigned int bar_width, unsigned int bar_value, unsigned int border_colour, unsigned int bar_colour, unsigned int back_colour, unsigned char border)
{
    switch(border)
    {
        case YES:
        {
            Draw_Rectangle((x1 + 1), (y2 - 1), (x1 + bar_width - 1), (y2 - bar_value), YES, SQUARE, bar_colour, back_colour);
            Draw_Rectangle((x1 + 1), (y2 - bar_value - 1), (x1 + bar_width - 1), (y1 + 1), YES, SQUARE, back_colour, back_colour);
            Draw_Rectangle(x1, y1, (x1 + bar_width), y2, NO, SQUARE, border_colour, back_colour);
            break;
        }
        default:
        {
            Draw_Rectangle(x1, y2, (x1 + bar_width), (y2 - bar_value), YES, SQUARE, bar_colour, back_colour);
            Draw_Rectangle(x1, (y2 - bar_value), (x1 + bar_width), y1, YES, SQUARE, back_colour, back_colour);
            break;
        }
    }
}


void Draw_Circle(signed int xc, signed int yc, signed int radius, unsigned char fill, unsigned int colour)
{
     signed int a = 0x0000;
     signed int b;
     signed int p;

     b = radius;
     p = (1 - b);

     do
     {
          switch(fill)
          {
              case YES:
              {
                  Draw_H_Line((xc - a), (xc + a), (yc + b), colour);
                  Draw_H_Line((xc - a), (xc + a), (yc - b), colour);
                  Draw_H_Line((xc - b), (xc + b), (yc + a), colour);
                  Draw_H_Line((xc - b), (xc + b), (yc - a), colour);
                  break;
              }
              default:
              {
                  Draw_Pixel((xc + a), (yc + b), colour);
                  Draw_Pixel((xc + b), (yc + a), colour);
                  Draw_Pixel((xc - a), (yc + b), colour);
                  Draw_Pixel((xc - b), (yc + a), colour);
                  Draw_Pixel((xc + b), (yc - a), colour);
                  Draw_Pixel((xc + a), (yc - b), colour);
                  Draw_Pixel((xc - a), (yc - b), colour);
                  Draw_Pixel((xc - b), (yc - a), colour);
                  break;
              }
          }

          if(p < 0)
          {
              p += (0x03 + (0x02 * a++));
          }
          else
          {
              p += (0x05 + (0x02 * ((a++) - (b--))));
          }
    }while(a <= b);
}



void Draw_Font_Pixel(unsigned int x_pos, unsigned int y_pos, unsigned int colour, unsigned char pixel_size)
{
     volatile unsigned char i = (pixel_size * pixel_size);

     TFT_set_display_window(x_pos, y_pos, (x_pos + pixel_size - 1), (y_pos + pixel_size - 1));

     while(i > 0)
     {
         TFT_write_data(colour);
         i--;
     }
}


void print_char(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, unsigned char ch)
{
     unsigned char i;
     unsigned char j;
     unsigned char k;
     unsigned char l;
     unsigned char value;
     unsigned PGM_P fp;

     if(font_size <= 0)
     {
         font_size = 1;
     }

     if(x_pos < font_size)
     {
         x_pos = font_size;
     }

     TFT_set_display_window(x_pos, y_pos, (x_pos + 5 * font_size - 1), (y_pos + 8 * font_size - 1));

     for(j = 0; j < 8; j++) {
	 for(l = 0; l < font_size; l++) {
	     for(i = 0; i < 5; i++) {
	         fp = &(font[((unsigned char)ch) - 0x20][i]);
		 value = pgm_read_byte(fp);
		 for(k = 0; k < font_size; k++) {
		     if(((value >> j) & 1) != 0)
		         TFT_write_data(colour);
		     else
			 TFT_write_data(back_colour);
		 }
	     }
	 }
     }
/* slow
     for(i = 0x00; i < 0x05; i++)
     {
         for(j = 0x00; j < 0x08; j++)
         {
             //value = 0x0000;
             //value = ((font[((unsigned char)ch) - 0x20][i]));
             fp = &(font[((unsigned char)ch) - 0x20][i]);
             value = pgm_read_byte(fp);

             if(((value >> j) & 0x01) != 0x00)
             {
                 Draw_Font_Pixel(x_pos, y_pos, colour, font_size);
             }
             else
             {
                 Draw_Font_Pixel(x_pos, y_pos, back_colour, font_size);
             }

             y_pos += font_size;
          }

          y_pos -= (font_size << 0x03);
          x_pos += font_size;
      }
*/
}


void print_str_P(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, const char *ch)
{
     char c;

     while((c = pgm_read_byte(ch++)))
     {
         print_char(x_pos, y_pos, font_size, colour, back_colour, c);
         x_pos += (font_size * 0x06);
     }
}

void print_str(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, const char *ch)
{
     do
     {
         print_char(x_pos, y_pos, font_size, colour, back_colour, *ch++);
         x_pos += (font_size * 0x06);
     }while((*ch >= 0x20) && (*ch <= 0x7F));
}

void print_strn(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, char *ch, unsigned char n)
{
     do
     {
         print_char(x_pos, y_pos, font_size, colour, back_colour, *ch++);
         x_pos += (font_size * 0x06);
     }while(--n);
}

unsigned int text_x, text_y;

void set_text_pos (unsigned int x, unsigned int y)
{
     text_x = x;
     text_y = y;
}

void print_ln_P(unsigned char font_size, unsigned int colour, unsigned int back_colour, const char *ch)
{
     print_str_P(text_x, text_y, font_size, colour, back_colour, ch);
     //text_y += (font_size * 0x09);
     text_y += (font_size * 12);
}

void print_ln(unsigned char font_size, unsigned int colour, unsigned int back_colour, char *ch)
{
     print_str(text_x, text_y, font_size, colour, back_colour, ch);
     //text_y += (font_size * 0x09);
     text_y += (font_size * 12);
}

void print_C(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, signed int value)
{
     char ch[4];

     if(value < 0x00)
     {
        ch[0] = 0x2D;
        value = -value;
     }
     else
     {
        ch[0] = 0x20;
     }

     if((value > 99) && (value <= 999))
     {
         ch[1] = ((value / 100) + 0x30);
         ch[2] = (((value % 100) / 10) + 0x30);
         ch[3] = ((value % 10) + 0x30);
     }
     else if((value > 9) && (value <= 99))
     {
         ch[1] = (((value % 100) / 10) + 0x30);
         ch[2] = ((value % 10) + 0x30);
         ch[3] = 0x20;
     }
     else if((value >= 0) && (value <= 9))
     {
         ch[1] = ((value % 10) + 0x30);
         ch[2] = 0x20;
         ch[3] = 0x20;
     }

     print_str(x_pos, y_pos, font_size, colour, back_colour, ch);
}


void print_I(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, signed int value)
{
    char ch[6];

    if(value < 0)
    {
        ch[0] = 0x2D;
        value = -value;
    }
    else
    {
        ch[0] = 0x20;
    }

    if(value > 9999)
    {
        ch[1] = ((value / 10000) + 0x30);
        ch[2] = (((value % 10000)/ 1000) + 0x30);
        ch[3] = (((value % 1000) / 100) + 0x30);
        ch[4] = (((value % 100) / 10) + 0x30);
        ch[5] = ((value % 10) + 0x30);
    }

    else if((value > 999) && (value <= 9999))
    {
        ch[1] = (((value % 10000)/ 1000) + 0x30);
        ch[2] = (((value % 1000) / 100) + 0x30);
        ch[3] = (((value % 100) / 10) + 0x30);
        ch[4] = ((value % 10) + 0x30);
        ch[5] = 0x20;
    }
    else if((value > 99) && (value <= 999))
    {
        ch[1] = (((value % 1000) / 100) + 0x30);
        ch[2] = (((value % 100) / 10) + 0x30);
        ch[3] = ((value % 10) + 0x30);
        ch[4] = 0x20;
        ch[5] = 0x20;
    }
    else if((value > 9) && (value <= 99))
    {
        ch[1] = (((value % 100) / 10) + 0x30);
        ch[2] = ((value % 10) + 0x30);
        ch[3] = 0x20;
        ch[4] = 0x20;
        ch[5] = 0x20;
    }
    else
    {
        ch[1] = ((value % 10) + 0x30);
        ch[2] = 0x20;
        ch[3] = 0x20;
        ch[4] = 0x20;
        ch[5] = 0x20;
    }

    print_str(x_pos, y_pos, font_size, colour, back_colour, ch);
}


void print_D(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, unsigned int value, unsigned char points)
{
    char ch[5];

    ch[0] = 0x2E;
    ch[1] = ((value / 1000) + 0x30);

    if(points > 1)
    {
        ch[2] = (((value % 1000) / 100) + 0x30);

        if(points > 2)
        {
            ch[3] = (((value % 100) / 10) + 0x30);

            if(points > 3)
            {
                ch[4] = ((value % 10) + 0x30);
            }
        }
    }

    print_str(x_pos, y_pos, font_size, colour, back_colour, ch);
}


void print_F(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, float value, unsigned char points)
{
    signed long tmp;

    tmp = value;
    print_I(x_pos, y_pos, font_size, colour, back_colour, tmp);
    tmp = ((value - tmp) * 10000);

    if(tmp < 0)
    {
       tmp = -tmp;
    }

    if((value >= 10000) && (value < 100000))
    {
        print_D((x_pos + (36 * font_size)), y_pos, font_size, colour, back_colour, tmp, points);
    }
    else if((value >= 1000) && (value < 10000))
    {
        print_D((x_pos + (30 * font_size)), y_pos, font_size, colour, back_colour, tmp, points);
    }
    else if((value >= 100) && (value < 1000))
    {
        print_D((x_pos + (24 * font_size)), y_pos, font_size, colour, back_colour, tmp, points);
    }
    else if((value >= 10) && (value < 100))
    {
        print_D((x_pos + (18 * font_size)), y_pos, font_size, colour, back_colour, tmp, points);
    }
    else if(value < 10)
    {
        print_D((x_pos + (12 * font_size)), y_pos, font_size, colour, back_colour, tmp, points);

        if((value) < 0)
        {
            print_char(x_pos, y_pos, font_size, colour, back_colour, 0x2D);
        }
        else
        {
            print_char(x_pos, y_pos, font_size, colour, back_colour, 0x20);
        }
    }
}


void Draw_BMP(signed int x_pos1, signed int y_pos1, signed int x_pos2, signed int y_pos2, const char *bitmap)
{
     unsigned long size;
     unsigned long index = 0x00000000;

     size = (y_pos2 - y_pos1);
     size *= (x_pos2 - x_pos1);
     //size <<= 1;

     TFT_set_display_window(x_pos1, y_pos1, (x_pos2 - 1), (y_pos2 - 1));

     unsigned int * b;	//convert to 16bit pointer, then we can read words
     b = (unsigned int *) bitmap;
     for(index = 0; index < size; index++)
     {
         TFT_write_data(pgm_read_word(b++));
     }
}
