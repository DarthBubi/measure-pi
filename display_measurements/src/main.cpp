#include <cnpy.h>
#include <iostream>
#include <string>
#include "eink.c"

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        //Load the genearted image
        cnpy::NpyArray img = cnpy::npy_load(static_cast<std::string>(argv[1]));
        gImage_image = img.data<unsigned char>();

        if (!bcm2835_init())
            return 1;

        bcm2835_gpio_fsel(RPI_BPLUS_GPIO_J8_22, BCM2835_GPIO_FSEL_INPT);

        bcm2835_spi_begin();
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
        bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
        bcm2835_gpio_set_pud(RPI_BPLUS_GPIO_J8_22, BCM2835_GPIO_PUD_UP);

        std::cout << "Clearing screen..." << std::endl;
        ClearScreen();
        std::cout << "Sending image to eink display..." << std::endl;
        SendImage(img.shape[0]);
        std::cout << "Sending complete." << std::endl;

        return 0;
    }
    else
    {
        std::cerr << "Please provide an image [*.npy]" << std::endl;
        return -1;
    }
}
