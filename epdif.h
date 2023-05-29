/***************************************************
//Web: http://www.buydisplay.com
EastRising Technology Co.,LTD
****************************************************/

#ifndef EPDIF_H
#define EPDIF_H

#include <arduino.h>

// Pin definition
#define CS_PIN    11
#define DC_PIN    10
#define RST_PIN   9
#define BUSY_PIN  8

class EpdIf {
public:
    EpdIf(void);
    ~EpdIf(void);

    static int  IfInit(void);
    static void DigitalWrite(int pin, int value); 
    static int  DigitalRead(int pin);
    static void DelayMs(unsigned int delaytime);
    static void SpiTransfer(unsigned char data);
};

#endif
