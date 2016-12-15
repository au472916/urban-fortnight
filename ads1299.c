#include "../labstreaminglayer/LSL/liblsl/include/lsl_c.h"
#include "ads1299.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <bcm2835.h>

int InputCheck(int seconds)
{
    fd_set set;
    struct timeval timeout = {seconds, 0};

    FD_ZERO(&set);
    FD_SET(0, &set);
    return select(1, &set, NULL, NULL, &timeout) == 1;
}

uint8_t PinInit()
{
	printf("Initiating BCM2835...\n");
	if (!bcm2835_init())
	{
		printf("Initiation failed. Are you running as root?\n");
		return 1;
	}
	printf("BCM2835 initiated!\n");
	
	//Set all pins as GPIO pins
	//Pins for SPI
	bcm2835_gpio_fsel(MOSI,        	BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(MISO,		BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(SCLK,		BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(CS,		BCM2835_GPIO_FSEL_OUTP);
	
	//Pins for GPIO	
	//Direct ADS1299 connections
	bcm2835_gpio_fsel(PIN_RESET,	BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_START, 	BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_PDWN, 	BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_DRDY,	BCM2835_GPIO_FSEL_INPT);
	//ADS analog and digital supply pins
	bcm2835_gpio_fsel(PIN_PWUP,	BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_DVDD, 	BCM2835_GPIO_FSEL_OUTP);

	//Set all output pins low
	bcm2835_gpio_write(MOSI,	LOW);
	bcm2835_gpio_write(MISO,	LOW);
	bcm2835_gpio_write(SCLK,	LOW);
	bcm2835_gpio_write(CS,		LOW);
	
	bcm2835_gpio_write(PIN_RESET,	LOW);
	bcm2835_gpio_write(PIN_START,	LOW);
	bcm2835_gpio_write(PIN_PDWN,	LOW);
	bcm2835_gpio_write(PIN_PWUP,	LOW);
	bcm2835_gpio_write(PIN_DVDD, 	LOW);
}
void ADSPowerOn(){
	//Enable Analog supply to Human Interface
	bcm2835_gpio_write(PIN_PWUP,	HIGH);
	//Enable digital supply to Human Interface
	bcm2835_gpio_write(PIN_DVDD,	HIGH);
}
void ADSPowerOff(){
	//Disable Analog supply to Human Interface
	bcm2835_gpio_write(PIN_PWUP,	HIGH);
	//Disable digital supply to Human Interface
	bcm2835_gpio_write(PIN_DVDD,	HIGH);
}

void ADSInit()
{
	//Prepare for reset pulse
	bcm2835_gpio_write(PIN_RESET,     HIGH);
	bcm2835_gpio_write(PIN_PDWN,      HIGH);
	sleep(1);

	//Send 6us reset pulse
	int w;
	bcm2835_gpio_write(PIN_RESET, LOW);
	for(w=0;w<1000;++w){
 	       _NOP();
	}
	//Wait for at least 60us
	bcm2835_gpio_write(PIN_RESET, HIGH);
	for(w=0;w<5000;++w){
		_NOP();
	}
}

uint8_t SpiInit()
{
	printf("Starting SPI...\n");
	if (!bcm2835_spi_begin())
	{
		printf("Starting SPI failed. Are you running as root?\n");
		return 1;
	}

	printf("SPI started!\n");
	
	//Set spi parameters
	// This function doesn't do anything as LSB first isn't supported
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);

	//Sets SPI mode to 1
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);	
	
	//Sets SCLK frequency to ~2MHz	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128);

	//Sets CS pin and polarity	
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);	
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

}

void ConfigSetup(uint8_t * config_array)
{
	if(!WriteRegister(config_array, REG_CONFIG1, 3)){
		SetupError();
	}
}

void ChannelSetup(uint8_t * channel_array)
{
       if(! WriteRegister(channel_array, REG_CH1, 8)){
		SetupError();
	}
}

void StartupSequence()
{
	//Create an array to store the Startup commands
	uint8_t seq_length = 5;
	uint8_t sequence[seq_length];
	uint8_t i;
	
	//Start with SDATAC, and end with STOP. Fill the rest with 0x00
	sequence[0] = SDATAC;
	for(i=1;i<seq_length;++i)
		sequence[i] = (i==seq_length-2) ? STOP : 0x00;
	bcm2835_spi_writenb(sequence,seq_length);

	//Initialize CONFIG Registers
	uint8_t config_array[3] = {0x96,0xC0,0xEC};
	ConfigSetup(config_array);
	
	//Initialize CHANNEL Registers
	uint8_t channel_array[8] = {0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07};
	ChannelSetup(channel_array);
	
	//Enable SRB1
	uint8_t misc = 0x20;
	WriteRegister(&misc, REG_MISC1, 1);
	
	//Enable bias averaging on all channels
	uint8_t bias_sense[2] = {0xFF, 0xFF};
	WriteRegister(bias_sense, REG_BIAS_SENSP, 2);
}

void TranslateData(long * data_array, uint8_t * read_array, uint8_t length)
{
	//The last 24 Bytes contain the the adc data
	//Read array will either be 27 or 28 Bytes

	uint8_t numBytes = length;
	int dif = numBytes-24;

        int c;
	long cmp_bit  = 1<<23;
	long sign_bit = 1<<24;
        for(c=0;c<8;++c)
		data_array[c]=0;

	//Convert data from read_array to signed decimal
        for(c=0;c<24;++c){
        	int ind = c/3;
        	data_array[ind]+=read_array[dif+c]<<(16-8*(c%3));

        	if(c%3==2){
        		data_array[ind] = (data_array[ind]<cmp_bit)? data_array[ind] : (data_array[ind] - sign_bit);
        	}
        }
}

void ReadSingle(long * data_array)
{
	//Prepares to send and sends the single shot read command
	//Start conversions
	uint8_t com_var[2] = {START, 0x00};
	bcm2835_spi_transfern(com_var, 2);
	usleep(200000);

	//Send single read command
	uint8_t read_array[28] = {0x00};
	read_array[0] = RDATA;
	bcm2835_spi_transfern(read_array, 28);
	TranslateData(data_array, read_array,28);

	//Print translated data
	int i;
	for(i = 0; i < 8; i++){
		printf("Channel %d: %li \t\n", i+1, data_array[i]);
	}

	//Stop conversions
	com_var[0] = STOP;
	bcm2835_spi_transfern(com_var,2);
}

void InitReadContinuous(){
	//Change ADS mode to Continuous Conversion Mode
	uint8_t com_var[2] = {START,0x00};
	bcm2835_spi_writenb(com_var,2);
	usleep(200000);
	
	//Change ADS mode to Read Continuous Mode
	uint8_t ret_var[2] = {0x00,0x00};
	com_var[0] = RDATAC;
	bcm2835_spi_transfernb(com_var,ret_var,2);
	bcm2835_gpio_len(PIN_DRDY);
}

void ReadContinuous(lsl_outlet * outlet, RCArrays *pRCArrays)
{
//InitReadContinuous() must be called first

	//Read conversion data and translate to decimal
	if(bcm2835_gpio_eds(PIN_DRDY))
	{
		bcm2835_spi_transfernb(pRCArrays->read_array,pRCArrays->return_array,27);
		TranslateData(pRCArrays->data_array, pRCArrays->return_array, 27);
		bcm2835_gpio_set_eds(PIN_DRDY);
		lsl_push_sample_l(*outlet,pRCArrays->data_array);
	}
}

void ExitReadContinuous(){
	int i;
	uint8_t seq_length = 5;
        uint8_t sequence[seq_length];
        sequence[0] = SDATAC;
        for(i=1;i<seq_length;++i)
                sequence[i] = (i==seq_length-2) ? STOP : 0x00;
        bcm2835_spi_writenb(sequence,seq_length);
}

long int InitReadExternal(struct timespec *get_time){
	clock_gettime(CLOCK_REALTIME, get_time);
	return get_time->tv_nsec;
	
	/* Initialise External ADC SPI*/
	
}

void ReadExternal(struct timespec *get_time, long int *last_event  ){
	// This function should execute completely in less than 23us
	
	//Find time since last
	clock_gettime(CLOCK_REALTIME, get_time);
	long int event_difference = get_time->tv_nsec - *last_event;
	uint8_t io_state = bcm2835_gpio_lev(RPI_V2_GPIO_P1_05); 
	
	//If last_event is greater than 1,000,000,000, counter overflows and starts over
	//Checks if counter has overflowed (if current time is less than last time)
	if(event_difference < 0){
		clock_gettime(CLOCK_REALTIME, get_time);
		*last_event = get_time->tv_nsec;
		event_difference = get_time->tv_nsec - *last_event;
	 }
	 
	//If time since last_event is 25us, run this
	if(event_difference >= 25000){
		//Reset last_event
		clock_gettime(CLOCK_REALTIME, get_time);
		*last_event = get_time->tv_nsec;
		
		//Toggle pin level to test timing
		io_state ^= 1;
		bcm2835_gpio_write(RPI_V2_GPIO_P1_05 , io_state);
		
		/* Read from ADC and prepare data to send to LSL*/
		/* Push collected sample to LSL stream */
	}

}

void ExitReadMicrophone(){
}

uint8_t * ReadRegister(uint8_t * read_array, uint8_t reg, uint8_t length)
{
	if(!length){
		printf("Length must be greater than 0\n");
		return NULL;
	}
	//Prepare array to send to ADS1299
	uint8_t read_length = length + 3;
	read_array[0] = reg + RREG;
	read_array[1] = length - 1;
	uint8_t i;
	for(i=2;i<read_length;++i){
		read_array[i] = 0x00;
	}
	
	//Read from MISO
	bcm2835_spi_transfern(read_array, read_length);
	
	//Return pointer to start of data
	return read_array+2;
}

void WriteRegisterSingle(uint8_t reg_value, uint8_t reg)
{
	uint8_t write_array[4];
	write_array[0] = reg + WREG;
	write_array[1] = 0x00;
	write_array[2] = reg_value;
	write_array[3] = 0x00;

	uint8_t * cmp_array = NULL;
	uint8_t read_array[4] = {0x00};
	uint8_t timeout = 0;

	do{
		bcm2835_spi_writenb(write_array, 4);

                cmp_array = ReadRegister(read_array, reg, 1);

                if(timeout<10){
			printf("Reg: %02X\tCmp: %02X \n", reg_value, *cmp_array);
                }

                ++timeout;
        }while(memcmp(&reg_value,cmp_array,1)&&timeout<10);
}

uint8_t WriteRegister(uint8_t * reg_values, uint8_t reg, uint8_t length)
{
	uint8_t write_length = length + 3;
	uint8_t write_array[write_length];

	write_array[0] = reg + WREG;
	write_array[1] = length - 1;
	write_array[write_length-1] = 0x00;

	uint8_t i;
	for(i=2;i<write_length-1;++i)
		write_array[i] = reg_values[i-2];

	uint8_t * cmp_array = NULL;
	uint8_t read_array[write_length];
	memset(read_array, 0, write_length);
	uint8_t timeout = 0;

	do{
		bcm2835_spi_writenb(write_array, write_length);

		cmp_array = ReadRegister(read_array, reg, length);

        	if(timeout<10){
                	for(i=0;i<length;++i)
                        	printf("Reg: %02X\tCmp: %02X \n", reg_values[i],cmp_array[i]);
        	}

		++timeout;
	}while(memcmp(reg_values,cmp_array,length)&&timeout<10);

	if(timeout>=10){
		printf("Could not write to register. Try to reset\n");
		return 0;
	}
	return 1;
}

void LSLInit(lsl_outlet * outlet)
{
	lsl_streaminfo info;
	char channels[] = {'C1','C2','C3','C4','C5','C6','C7','C8'};	//Channel Labels
	lsl_xml_ptr desc, chn, chns;	//XML element pointers
	uint8_t c;			//Channel index

	//Declare a new streaminfo
	//Group: RaspPi, content type: EEG, 8 channels, 100 Hz, float values
	//Name of node, must follow the format 'nodePiX', where X is unique
	info = lsl_create_streaminfo("RaspPi","EEG",8,100,cft_float32,"nodePi1");


	//Add meta-data fields
	//For more standard fields, see https://github.com/sccn/xdf/wiki/Meta-Data)
	//Meta-data is received on the MATLAB side in the initial connection
	desc = lsl_get_desc(info);
	lsl_append_child_value(desc, "manufacturer", "Grp16160");
	chns = lsl_append_child(desc,"channels");
	for (c=0;c<8;c++) {
		chn = lsl_append_child(chns,"channel");
		lsl_append_child_value(chn,"label",channels+c);
		lsl_append_child_value(chn,"unit","Extrovolts");
		lsl_append_child_value(chn,"type","EEG");
      		}
	}

        //Make a new outlet with chunk size: 1 and buffer size: 360 seconds (6 minutes)
        *outlet = lsl_create_outlet(info,1,360);
}

void SetupError(){
	uint8_t cont = 1;
	char str[100];
	printf("Something went wrong. Would you like to try again? Y/N\n");
	scanf("%s", str);
	while(strcmp(str, "y")&&strcmp(str, "Y")){
		if(!strcmp(str, "n")||!strcmp(str, "N")){
			printf("Press 0 to exit the program\n");
			cont = 0;
			break;
		}
	}
	if(cont){
		ADSPowerOff();
		sleep(1);
		ADSPowerOn();
		sleep(1);
		ADSInit();
		StartupSequence();
	}
}

void ResetRegisters(){
	WriteRegisterSingle(0x96,REG_CONFIG1);
        WriteRegisterSingle(0xC0,REG_CONFIG2);
        WriteRegisterSingle(0xEC,REG_CONFIG3);
        WriteRegisterSingle(0x00,REG_LOFF);
        WriteRegisterSingle(0x07,REG_CH1);
        WriteRegisterSingle(0x07,REG_CH2);
        WriteRegisterSingle(0x07,REG_CH3);
        WriteRegisterSingle(0x07,REG_CH4);
       	WriteRegisterSingle(0x07,REG_CH5);
        WriteRegisterSingle(0x07,REG_CH6);
        WriteRegisterSingle(0x07,REG_CH7);
        WriteRegisterSingle(0x07,REG_CH8);
        WriteRegisterSingle(0xFF,REG_BIAS_SENSP);
        WriteRegisterSingle(0xFF,REG_BIAS_SENSN);
        WriteRegisterSingle(0x00,REG_LOFF_SENSP);
        WriteRegisterSingle(0x00,REG_LOFF_SENSN);
        WriteRegisterSingle(0x00,REG_LOFF_FLIP);
        WriteRegisterSingle(0x0F,REG_GPIO);
        WriteRegisterSingle(0x20,REG_MISC1);
	WriteRegisterSingle(0x00,REG_MISC2);
	WriteRegisterSingle(0x00,REG_CONFIG4);
}
