#include "main.h"

int main(void)
{

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(0xF << 28);
    GPIOA->CRL |=  (0x2 << 28);

    while (1)
    {
        GPIOA->ODR ^= (1 << 7);

        for (volatile int i = 0; i < 500000; i++);
    }
}
