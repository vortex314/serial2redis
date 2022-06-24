#ifndef HARDWARE_H
#define HARDWARE_H
#include <stdint.h>
#include <string>

typedef void (*FunctionPointer)(void*);

typedef uint32_t Erc;
typedef unsigned char uint8_t;
typedef uint32_t PhysicalPin;
typedef enum {
    LP_TXD = 0,
    LP_RXD,
    LP_SCL,
    LP_SDA,
    LP_MISO,
    LP_MOSI,
    LP_SCK,
    LP_CS
} LogicalPin;

class Driver
{
public:
    virtual int init() = 0;
    virtual int deInit() = 0;
};


class UART : public Driver
{
public:
    static UART& create(uint32_t module,PhysicalPin txd, PhysicalPin rxd);
    virtual int mode(const char*)=0;
    virtual int init() = 0;
    virtual int deInit() = 0;
    virtual int setClock(uint32_t clock) = 0;

    virtual int write(const uint8_t* data, uint32_t length) = 0;
    virtual int write(uint8_t b) = 0;
    virtual int read(std::string& bytes) = 0;
    virtual uint8_t read() = 0;
    virtual void onRxd(FunctionPointer, void*) = 0;
    virtual void onTxd(FunctionPointer, void*) = 0;
    virtual uint32_t hasSpace() = 0;
    virtual uint32_t hasData() = 0;
};

//===================================================== GPIO DigitalIn ========

class DigitalIn : public Driver
{
public:
    typedef enum { DIN_NONE, DIN_RAISE, DIN_FALL, DIN_CHANGE } PinChange;

    typedef enum { DIN_NO_PULL=0,DIN_PULL_UP = 1, DIN_PULL_DOWN = 2 } Mode;
    static DigitalIn& create(PhysicalPin pin);
    virtual int read() = 0;
    virtual int init() = 0;
    virtual int deInit() = 0;
    virtual int onChange(PinChange pinChange, FunctionPointer fp,
                         void* object) = 0;
    virtual int setMode(Mode m) = 0;
    virtual PhysicalPin getPin() = 0;
};
//===================================================== GPIO DigitalOut
class DigitalOut : public Driver
{
public:
    typedef enum { DOUT_NONE, DOUT_PULL_UP = 1, DOUT_PULL_DOWN = 2 } Mode;
    static DigitalOut& create(PhysicalPin pin);
    virtual int init() = 0;
    virtual int deInit() = 0;
    virtual int write(int) = 0;
    virtual PhysicalPin getPin() = 0;
    virtual int setMode(Mode m)=0;
};
//===================================================== I2C ===

#define I2C_WRITE_BIT
#define I2C_READ_BIT 0

class I2C : public Driver
{
public:
    static I2C& create(PhysicalPin scl, PhysicalPin sda);
    virtual int init() = 0;
    virtual int deInit() = 0;
    virtual int setClock(uint32_t) = 0;
    virtual int setSlaveAddress(uint8_t address) = 0;
    virtual int write(uint8_t* data, uint32_t size) = 0;
    virtual int write(uint8_t data) = 0;
    virtual int read(uint8_t* data, uint32_t size) = 0;
};

class Spi : public Driver
{
public:
public:
    typedef enum {
        SPI_MODE_PHASE0_POL0 = 0,
        SPI_MODE_PHASE1_POL0 = 1,
        SPI_MODE_PHASE0_POL1 = 2,
        SPI_MODE_PHASE1_POL1 = 3
    } SpiMode;
    typedef enum {
        SPI_CLOCK_125K = 125000,
        SPI_CLOCK_250K = 250000,
        SPI_CLOCK_500K = 500000,
        SPI_CLOCK_1M = 1000000,
        SPI_CLOCK_2M = 2000000,
        SPI_CLOCK_4M = 4000000,
        SPI_CLOCK_8M = 8000000,
        SPI_CLOCK_10M = 10000000,
        SPI_CLOCK_20M = 20000000
    } SpiClock;

    static Spi& create(PhysicalPin miso, PhysicalPin mosi, PhysicalPin sck,
                       PhysicalPin cs);
    virtual ~Spi(){};
    virtual int init() = 0;
    virtual int deInit() = 0;
    virtual int exchange(std::string& in,  std::string& out) = 0;
    virtual int onExchange(FunctionPointer, void*) = 0;
    virtual int setClock(uint32_t) = 0;
    virtual int setMode(SpiMode) = 0;
    virtual int setLsbFirst(bool) = 0;
    virtual int setHwSelect(bool) = 0;
};

class ADC
{

public:
    static ADC& create(PhysicalPin pin);

    virtual int init() = 0;
    virtual int getValue() = 0;
};

class Uext
{
    uint32_t _pinsUsed;
    uint32_t _connectorIdx;
    uint32_t _physicalPins[8];
    UART* _uart;
    Spi* _spi;
    I2C* _i2c;
    ADC* _adc;

private:
    void lockPin(LogicalPin);
    bool isUsedPin(LogicalPin lp)
    {
        return _pinsUsed & lp;
    }
    void freePin(LogicalPin);

public:
    uint32_t toPin(uint32_t logicalPin);
    static const char* uextPin(uint32_t logicalPin);
    Uext(uint32_t idx);
    UART& getUART();
    Spi& getSPI();
    I2C& getI2C();
    DigitalIn& getDigitalIn(LogicalPin);
    DigitalOut& getDigitalOut(LogicalPin);
    ADC& getADC(LogicalPin);
    uint32_t index()
    {
        return _connectorIdx;
    };
    // PWM& getPWM();
};

#endif
