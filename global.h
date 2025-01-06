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

#define US_POKEY_DIV_STANDARD   0x28            //#40  => 19040 bps
                                                //=> 19231 bps #51
//#define ATARI_SPEED_STANDARD  (US_POKEY_DIV_STANDARD+11)      //#51 (o sest vic)
#define ATARI_SPEED_STANDARD    (US_POKEY_DIV_STANDARD+12)*2    //#104 U2X=1
//#define ATARI_SPEED_STANDARD  (US_POKEY_DIV_STANDARD+6)       //#46 (o sest vic)

#define US_POKEY_DIV_DEFAULT    0x06            //#6   => 68838 bps

#define US_POKEY_DIV_MAX                (255-6)         //pokeydiv 249 => avrspeed 255 (vic nemuze)

