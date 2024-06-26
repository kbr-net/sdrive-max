#define CMD_PORTREG PORTC
#define CMD_DDR DDRC
#define CMD_PORT PINC
#define CMD_PIN PINC5

#define DEVICESNUM      5       //      //D0:-D4:

struct SDriveParameters
{
        u08 p0;
        u08 p1;
        u08 p2;
        u08 p3;
        u32 p4_5_6_7;
};
