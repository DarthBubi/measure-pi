#include <bcm2835.h>
#include <stdio.h>
#include <stdbool.h>
#include "image.c"

#define IMAGE_WIDE 600
#define IMAGE_HIGH 800

#define STATUS_PIN RPI_BPLUS_GPIO_J8_22 
#define CS_PIN RPI_BPLUS_GPIO_J8_24 
#define MOSI_PIN RPI_BPLUS_GPIO_J8_19
#define MISO_PIN RPI_BPLUS_GPIO_J8_21
#define SCK_PIN RPI_BPLUS_GPIO_J8_23

#define FRAME_END_LEN	11

#define SpiExchangeByte(x) bcm2835_spi_transfer(x)
#define SPI_MODE SPI_MODE1


bool digitalRead(uint8_t pin)
{
	if(bcm2835_gpio_lev(pin)==1)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void digitalWrite(uint8_t pin,bool level)
{
	if(level==true)
	{
		bcm2835_gpio_set(pin);
	}
	else
	{
		bcm2835_gpio_clr(pin);
	}
}


void ClearScreen(void)
{
  bool DeviceStatus;
  
  DeviceStatus = digitalRead(STATUS_PIN);
  SpiExchangeByte(3); 
  while(digitalRead(STATUS_PIN) == DeviceStatus)
  {
     ; 
  }
}

void SetDefaultImage(void)
{
  bool DeviceStatus;
  
  DeviceStatus = digitalRead(STATUS_PIN);
  SpiExchangeByte(5); 
  while(digitalRead(STATUS_PIN) == DeviceStatus)
  {
     ; 
  }
}


void SetImageWide(unsigned int wide)
{
  bool DeviceStatus;
  
   DeviceStatus = digitalRead(STATUS_PIN);
   SpiExchangeByte(1);
   while(digitalRead(STATUS_PIN) == DeviceStatus)
   {
     ; 
   }
   DeviceStatus = !DeviceStatus;
   SpiExchangeByte(wide >> 8);
   while(digitalRead(STATUS_PIN) == DeviceStatus)
   {
     ; 
   }
   DeviceStatus = !DeviceStatus;
   SpiExchangeByte(wide & 0xff);
   while(digitalRead(STATUS_PIN) == DeviceStatus)
   {
     ; 
   }
}

void SetImageHigh(unsigned int high)
{
  bool DeviceStatus;
  
  DeviceStatus = digitalRead(STATUS_PIN);
  SpiExchangeByte(2);
   while(digitalRead(STATUS_PIN) == DeviceStatus)
   {
     ; 
   }
   DeviceStatus = !DeviceStatus;
   SpiExchangeByte(high >> 8);
   while(digitalRead(STATUS_PIN) == DeviceStatus)
   {
     ; 
   }
   DeviceStatus = !DeviceStatus;  
   SpiExchangeByte(high & 0xff);
   while(digitalRead(STATUS_PIN) == DeviceStatus)
   {
     ; 
   }
}


void SendImage()
{
  bool DeviceStatus;
  unsigned char data;
  unsigned char cnt0;
  unsigned int cnt1;
  unsigned long time;
  bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);
  DeviceStatus = digitalRead(STATUS_PIN);
  bcm2835_gpio_fsel(CS_PIN,BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_clr(CS_PIN);	
  SpiExchangeByte(4);

  while(digitalRead(STATUS_PIN) == DeviceStatus)
  {
    ; 
  }
  DeviceStatus = !DeviceStatus;
  for(cnt0 = 0;cnt0 < FRAME_END_LEN; cnt0 ++)
  {
    printf("Frame %d \n",cnt0);
    for(cnt1 = 0; cnt1 < sizeof(gImage_image); cnt1 ++)
    { 
       data = gImage_image[cnt1]; 
       SpiExchangeByte(data);
       time = 0;
       while(digitalRead(STATUS_PIN) == DeviceStatus)
       {
         time ++;
         if(time > 10000000)
         {
          printf("Error\n");
           return;
         }
       } 
       DeviceStatus = !DeviceStatus;
    } 
  } 
  bcm2835_gpio_set(CS_PIN);	
  bcm2835_gpio_fsel(CS_PIN,BCM2835_GPIO_FSEL_ALT0);
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
}


int main(int argc, char **argv)
{
   
    if (!bcm2835_init())
        return 1;

    bcm2835_gpio_fsel(RPI_BPLUS_GPIO_J8_22,BCM2835_GPIO_FSEL_INPT);
    
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);     
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);                  
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); 
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      
	bcm2835_gpio_set_pud(RPI_BPLUS_GPIO_J8_22,BCM2835_GPIO_PUD_UP);

	printf("5 -> Set Default Image\n");
	printf("4 -> Send Picture included in >> Image.c << \n");
	printf("3 -> Clear Screen\n");
	printf("2 -> Set Image Resolution (High)\n");
	printf("1 -> Set Image Resolution (Wide)\n");

	printf("Please Enter Command(1-5): \n");


	for(;;){
	char cmd;
	scanf("%d",&cmd);
    switch(cmd)
    {
      case 5:
        printf("Start Set Default Image!\n");
        SetDefaultImage();
        printf("Send Image Data Complete !\n");
        break;
      case 4:
        printf("Start Send Image Data!\n");
        SendImage();
        printf("Send Image Data Complete !\n");
        break;
      case 3:
        printf("Start Clear Screen !\n");
        ClearScreen();
        printf("Clear Screen Complete !\n");
        break;
      case 2:
        SetImageHigh(IMAGE_HIGH);
        printf("Set Image High Complete !\n");
        break;
      case 1:
        SetImageWide(IMAGE_WIDE);
        printf("Set Image Wide Complete !\n");
        break;
      default:
        printf("Unknow instruction :0x%02x!\n",cmd);
        break; 
  }
}
}


