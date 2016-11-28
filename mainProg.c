#include "../../../../labstreaminglayer/LSL/liblsl/include/lsl_c.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include "ads1299.h"
#include <string.h>
#include <time.h>

char *channels[] = {"C1","C2","C3","C4","C5","C6","C7","C8"};

int main()
{
	//Struct and pointer for ReadContinuous
	RCArrays my_RCArrays;
	RCArrays *RCAptr = &my_RCArrays;
	//LSL outlet for ReadContinuous and MicrophoneRead
	lsl_outlet outlet;

	char str[100];
	char run = 1;
	char option = 0xff;
	long channel[8];
	int i;

	//Variables for timing
	long int last_event;
	long int event_difference;
	struct timespec get_time;
	uint8_t io_state = 0;


	PinInit();
	LSLInit(channels,&outlet);
        ADSPowerOn();
        sleep(1);

        ADSInit();
        SpiInit();
	usleep(100);
        StartupSequence();

	// For testing timer!!
	bcm2835_gpio_fsel(RPI_V2_GPIO_P1_05 , BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(RPI_V2_GPIO_P1_05 , LOW);

	while(run){
		printf("\n\n---------------------");
		printf("\n\nChoose an option!\n\n");
		printf("0: Close Program\n");
		printf("1: Read Registers\n");
		printf("2: Single Read from ADS1299\n");
		printf("3: Continuous Read from ADS1299\n");
		printf("4: Manually Write Registers\n");
		printf("5: Reset Registers\n\n");
		printf("Option: ");
		while(option == 0xff){
			scanf("%s", str);
			if(!sscanf(str, "%d", &option))
                                printf("Error! Input was not an option!\n");
		}
		printf("\n");
		if(option == 0){
			//Close Program
			break;
		}
		else if(option == 1){
			//Read registers
			uint8_t read_array[27];
			//Read register output casted to pointer to kill a warning
                        uint8_t * out_array = (uint8_t *)ReadRegister(read_array,REG_ID, 24);
                        int i;
                        for(i = 0;i<24;i++)
                                printf("Register 0x%02X is 0x%02X\n", i, out_array[i]);

		}
		else if (option == 2){
			//Single Read from ADS1299
			long channel[8];
			ReadSingle(channel);
		}
		else if (option == 3){
			//Continuous Read from ADS1299

			char quit_str[2] = {0,0};
			memset(RCAptr->read_array,0,27);

			//Get time for first event
			last_event = InitReadMicrophone(&get_time);
			//clock_gettime(CLOCK_REALTIME, &get_time);


			InitReadContinuous();
			printf("Now transmitting data via LSL...\n"); //Add name of node
			printf("Enter 'q' to exit\n\n");
			while(1){
				ReadContinuous(&outlet,RCAptr);

				//Check time since last event
				//clock_gettime(CLOCK_REALTIME, &get_time);
		                //event_difference = get_time.tv_nsec - last_event;

				//If last_event is greater than 1.000.000.00
				//Create a new last_event from current time
				//Check difference again
               			/*if(event_difference < 0){
		                        clock_gettime(CLOCK_REALTIME, &get_time);
		                        last_event = get_time.tv_nsec;
		                        event_difference = get_time.tv_nsec - last_event;
		                }

				//If time since last_event is 1ms, run this
		                if(event_difference > 1000000){
	                        	last_event += 1000000;
	                        	io_state ^= 1;
	                        	bcm2835_gpio_write(RPI_V2_GPIO_P1_05 , io_state);
					//ReadMicrophone(&outletMic)
				}*/
				ReadMicrophone(&get_time, &last_event);

				//Check if user has entered 'q' and quit
				if(InputCheck(0)){
					scanf("%s", quit_str);
				}
				if(!strcmp(quit_str, "q")){
					break;
				}
			}
			ExitReadContinuous();
		}
		else if (option == 4){
			//Manually Read or Write Registers
			uint8_t register_setup[2] = {0xff, 0xff};

			printf("First register to write?\n");
			do{
                                scanf("%s", str);
                                if(!sscanf(str, "%02X", register_setup))
                                        printf("Error! Input was not an option!\n");
                        }while (register_setup[0] == 0xff);

			printf("How many registers to write?\n");
			do{
	                        scanf("%s", str);
	                        if(!sscanf(str, "%d", register_setup+1))
	                                printf("Error! Input was not an option!\n");
			}while (register_setup[1] == 0xff);

			uint8_t to_write[register_setup[1]];
			for(i=0;i<register_setup[1];++i){
                                printf("Write to register 0x%02X:\n", i+register_setup[0]);
                                scanf("%s", str);
                                sscanf(str, "%2X", to_write+i);
                	}

			WriteRegister(to_write,register_setup[0],register_setup[1]);
                }
		else if (option == 5){
			printf("Resetting registers...\n");
			ResetRegisters();
		}
		option = 0xff;
	}
	printf("Powering off ADS1299\n");
        ADSPowerOff();
        printf("Ending SPI Communication\n");
        bcm2835_spi_end();
        printf("Closing BCM2835\n\n");
        bcm2835_close();
        return 0;

}

