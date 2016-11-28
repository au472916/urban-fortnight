#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

//Define spi pins for use as gpio pins on startup
#define MOSI    RPI_V2_GPIO_P1_19
#define MISO    RPI_V2_GPIO_P1_21
#define SCLK    RPI_V2_GPIO_P1_23
#define CS      RPI_V2_GPIO_P1_24

//Define other hardware pins
#define PIN_RESET   RPI_V2_GPIO_P1_38
#define PIN_START   RPI_V2_GPIO_P1_37
#define PIN_PDWN    RPI_V2_GPIO_P1_40
#define PIN_DRDY    RPI_V2_GPIO_P1_31
#define PIN_PWUP    RPI_V2_GPIO_P1_03
#define PIN_DVDD    RPI_V2_GPIO_P1_29

//Define extremely short delay
#define _NOP() do { __asm__ __volatile__ ("nop"); } while (0)

//Define registers of ADS1299
#define REG_ID		0x00
#define REG_CONFIG1	0x01
#define REG_CONFIG2	0x02
#define REG_CONFIG3	0x03
#define REG_LOFF	0x04
#define REG_CH1		0x05
#define REG_CH2		0x06
#define REG_CH3		0x07
#define REG_CH4		0x08
#define REG_CH5		0x09
#define REG_CH6		0x0A
#define REG_CH7		0x0B
#define REG_CH8		0x0C
#define REG_BIAS_SENSP	0x0D
#define REG_BIAS_SENSN	0x0E
#define REG_LOFF_SENSP	0x0F
#define REG_LOFF_SENSN	0x10
#define REG_LOFF_FLIP	0x11
#define REG_LOFF_STATP	0x12
#define REG_LOFF_STATN	0x13
#define REG_GPIO	0x14
#define REG_MISC1	0x15
#define REG_MISC2	0x16
#define REG_CONFIG4	0x17

//Define opcodes of ADS1299
#define WAKEUP 		0x02
#define STANDBY		0x04
#define RESET		0x06
#define START		0x08
#define STOP		0x0A
#define RDATAC		0x10
#define SDATAC		0x11
#define RDATA		0x12
#define RREG		0x20
#define WREG		0x40

typedef struct _RCArrays{
        uint8_t read_array[27];
        uint8_t return_array[27];
        long data_array[8];
}RCArrays;


int InputCheck(int);

uint8_t PinInit();
void ADSPowerOn();
void ADSPowerOff();
void ADSInit();
uint8_t SpiInit();
void StartupSequence();
void LSLInit(char*[],lsl_outlet*);

uint8_t* ReadRegiter(uint8_t*, uint8_t, uint8_t);
uint8_t WriteRegister(uint8_t*, uint8_t, uint8_t);
void WriteRegisterSingle(uint8_t, uint8_t);
void ResetRegisters();

void ConfigSetup(uint8_t*);
void ChannelSetup(uint8_t*);
void ReadSingle(long*);

void InitReadContinuous();
void ReadContinuous(lsl_outlet*, RCArrays*);
void ExitReadContinuous();

long int InitReadMicrophone(struct timespec*);
void ReadMicrophone(struct timespec*, long int*);
void ExitReadMicrophone();

void TranslateData(long*, uint8_t*, uint8_t);
void SetupError();
