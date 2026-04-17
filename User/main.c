#include "stm32f10x.h"
#include "Delay.h"
#include "app.h"

int main(void)
{
    App_Init();

    while (1)
    {
        App_Loop();
        Delay_ms(50);
    }
}