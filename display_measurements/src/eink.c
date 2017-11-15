#include <bcm2835.h>
#include <stdio.h>
#include <stdbool.h>

#define STATUS_PIN RPI_BPLUS_GPIO_J8_22
#define CS_PIN RPI_BPLUS_GPIO_J8_24
#define MOSI_PIN RPI_BPLUS_GPIO_J8_19
#define MISO_PIN RPI_BPLUS_GPIO_J8_21
#define SCK_PIN RPI_BPLUS_GPIO_J8_23

#define FRAME_END_LEN 11

#define SpiExchangeByte(x) bcm2835_spi_transfer(x)
#define SPI_MODE SPI_MODE1

unsigned char *gImage_image;

bool digitalRead(uint8_t pin)
{
  if (bcm2835_gpio_lev(pin) == 1)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void digitalWrite(uint8_t pin, bool level)
{
  if (level == true)
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
  while(digitalRead(STATUS_PIN) == DeviceStatus);
}

void SendImage(size_t imgSize)
{
  bool DeviceStatus;
  unsigned char data;
  unsigned char cnt0;
  unsigned int cnt1;
  unsigned long time;
  bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);
  DeviceStatus = digitalRead(STATUS_PIN);
  bcm2835_gpio_fsel(CS_PIN, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_clr(CS_PIN);
  SpiExchangeByte(4);

  while (digitalRead(STATUS_PIN) == DeviceStatus);

  DeviceStatus = !DeviceStatus;
  for (cnt0 = 0; cnt0 < FRAME_END_LEN; cnt0++)
  {
    /* printf("Frame %d \n", cnt0); */
    for (cnt1 = 0; cnt1 < imgSize; cnt1++)
    {
      data = gImage_image[cnt1];
      SpiExchangeByte(data);
      time = 0;
      while (digitalRead(STATUS_PIN) == DeviceStatus)
      {
        time++;
        if (time > 10000000)
        {
          printf("Error\n");
          return;
        }
      }
      DeviceStatus = !DeviceStatus;
    }
  }
  bcm2835_gpio_set(CS_PIN);
  bcm2835_gpio_fsel(CS_PIN, BCM2835_GPIO_FSEL_ALT0);
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
}
