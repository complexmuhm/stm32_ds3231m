#include "cmsis/stm32f1xx.h"
#include "i2c.cpp"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
using uint8_t = unsigned char;
using uint16_t = unsigned short;
int buffer[128];

const uint16_t YEAR_REF = 1980;
// LSB is 1 for read, and 0 for write
const uint8_t slave_addr = 0xD0; // address for the DS3231M

// Internal register addresses of the slave
const uint8_t s_addr = 0x00;
const uint8_t m_addr = 0x01;
const uint8_t h_addr = 0x02;
const uint8_t day_addr = 0x04;
const uint8_t month_addr = 0x05;
const uint8_t year_addr = 0x06;

struct time
{
    uint8_t sec, min, hour;
};

struct date
{
    uint8_t day, month, year;
};

volatile uint32_t ticks;
extern "C" void SysTick_Handler()
{
    ++ticks;
}

uint32_t SystemCoreClock;
extern "C" void SystemInit()
{
    // Conf clock : 72MHz using HSE 8MHz crystal w/ PLL X 9 (8MHz x 9 = 72MHz)
    FLASH->ACR      |= FLASH_ACR_LATENCY_2; // Two wait states, per datasheet
    RCC->CFGR       |= RCC_CFGR_PPRE1_2;    // prescale AHB1 = HCLK/2
    RCC->CR         |= RCC_CR_HSEON;        // enable HSE clock
    while( !(RCC->CR & RCC_CR_HSERDY) );    // wait for the HSEREADY flag
    
    RCC->CFGR       |= RCC_CFGR_PLLSRC;     // set PLL source to HSE
    RCC->CFGR       |= RCC_CFGR_PLLMULL9;   // multiply by 9 
    RCC->CR         |= RCC_CR_PLLON;        // enable the PLL 
    while( !(RCC->CR & RCC_CR_PLLRDY) );    // wait for the PLLRDY flag
    
    RCC->CFGR       |= RCC_CFGR_SW_PLL;     // set clock source to pll

    while( !(RCC->CFGR & RCC_CFGR_SWS_PLL) );    // wait for PLL to be CLK
    SystemCoreClock = 72000000;
}

void delay(uint32_t ms)
{
    uint32_t now = ticks;
    while((ticks - now) < ms);
}


void init()
{
    RCC->APB2ENR = 
        RCC_APB2ENR_AFIOEN |
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_IOPBEN |
        RCC_APB2ENR_IOPCEN |
        RCC_APB2ENR_IOPDEN |
        RCC_APB2ENR_IOPEEN |
        RCC_APB2ENR_USART1EN;
    RCC->APB1ENR = 
        RCC_APB1ENR_TIM2EN |
        RCC_APB1ENR_I2C1EN;

    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
}

void init_gpio()
{
    // For the I2C1 pins
    // Set pins B6 and B7 to alternate function open drain with 50MHZ drain
    GPIOB->CRL = 0xff000000; 

    // Setup PC13
    GPIOC->CRH = 0x00700000;
}

void init_usart()
{
    // PA9 TX, full duplex AF push pull
    // PA10 RX, full duplex Input floating / Input pull-up
    GPIOA->CRH = 0x000004B0;

    // USART
    // Page 792
    // Set the M bits to 8
    USART1->CR1 = 0;
    // Program the number of stop bits, 1 in our case
    USART1->CR2 = 0;
    // DMA enable in USART_CR3 if Multi buffer Communication
    // Select the baud rate using the USART_BRR register
    //USART1->BRR = 0x271; // Baud rate of 115.2kbps at 72 MHz
    USART1->BRR = 0x10; // Baud rate of 4500kbps at 72 MHz
}

void send_string(const char *str_)
{
    const char *str = str_;
    // Enable USART1
    SET_BIT(USART1->CR1, USART_CR1_UE);
    // Set the TE bit to send and idle frame
    SET_BIT(USART1->CR1, USART_CR1_TE);
    
    // Now we send out string
    // TXE bit ist reset when writing to the USART_DR register
    while(*str != 0)
    {
        USART1->DR = *str;
        // When TXE=1 then we can write to the USART_DR register again
        // Now wait until the contents have been transferred to the shift register
        while(!READ_BIT(USART1->SR, USART_SR_TXE)) {}
        ++str;
    }
    
    // After writing the last data into the USART_DR register, wait until TC=1
    while(!READ_BIT(USART1->SR, USART_SR_TC)) {}
    // Disable USART now
    CLEAR_BIT(USART1->CR1, USART_CR1_UE);
}

char* itoa(int val)
{
    const int base = 10;
    static char buf[32] = {0};
    int i = 30;

    for(; val && i ; --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];

    return &buf[i+1];
}

void ds3231m_set_clock(uint8_t h, uint8_t m, uint8_t s)
{
    uint8_t h_conv = 0;
    if(h / 10 == 2)
        h_conv = (1 << 5)+ h % 10;
    else if(h / 10 == 1)
        h_conv = (1 << 4)+ h % 10;
    else
        h_conv = h;
    uint8_t m_conv = ((m / 10) << 4) + (m % 10);
    uint8_t s_conv = ((s / 10) << 4) + (s % 10);

    i2c_write_register(slave_addr, h_addr, h_conv);
    i2c_write_register(slave_addr, m_addr, m_conv);
    i2c_write_register(slave_addr, s_addr, s_conv);
}

time ds3231m_get_time(uint8_t slave_addr)
{
    time t;

    uint8_t raw_s = i2c_read_register(slave_addr, s_addr);
    uint8_t raw_m = i2c_read_register(slave_addr, m_addr);
    uint8_t raw_h = i2c_read_register(slave_addr, h_addr);

    t.sec = ((raw_s >> 4) * 10) + (raw_s & 0x0f);
    t.min = ((raw_m >> 4) * 10) + (raw_m & 0x0f);
    t.hour = ((raw_h >> 5) & 1) * 20 + ((raw_h >> 4) & 1) * 10 + (raw_h & 0x0f);

    return t;
}

void ds3231m_set_date(int day, int month, int year)
{
    uint8_t day_conv = ((day / 10) << 4) + (day % 10);
    uint8_t month_conv = ((month / 10) << 4) + (month % 10);
    uint8_t year_corrected = year - YEAR_REF;
    uint8_t year_conv = ((year_corrected / 10) << 4) + (year_corrected % 10);
    
    i2c_write_register(slave_addr, day_addr, day_conv);
    i2c_write_register(slave_addr, month_addr, month_conv);
    i2c_write_register(slave_addr, year_addr, year_conv);
}

date ds3231m_get_date(uint8_t slave_addr)
{
    date d;

    uint8_t raw_day = i2c_read_register(slave_addr, day_addr);
    uint8_t raw_month = i2c_read_register(slave_addr, month_addr);
    uint8_t raw_year = i2c_read_register(slave_addr, year_addr);

    d.day = ((raw_day >> 4) * 10) + (raw_day & 0x0f);
    d.month = ((raw_month >> 4) * 10) + (raw_month & 0x0f);
    d.year = ((raw_year >> 4) * 10) + (raw_year & 0x0f);

    return d;
}

int main(void) 
{
    init();
    init_gpio();
    init_usart();
    init_i2c();

    // SET_BIT(GPIOC->BSRR, GPIO_BSRR_BR13);
    // delay(500);
    // SET_BIT(GPIOC->BSRR, GPIO_BSRR_BS13);
    
    while(1)
    {
        time t = ds3231m_get_time(slave_addr);
        date d = ds3231m_get_date(slave_addr);

        // TODO: format the output of itoa to %02d, so padded 0's 2 digits
        send_string(itoa(t.hour));
        send_string(":");
        send_string(itoa(t.min));
        send_string(":");
        send_string(itoa(t.sec));

        send_string("    ");
        send_string(itoa(d.day));
        send_string(".");
        send_string(itoa(d.month));
        send_string(".");
        send_string(itoa(d.year + YEAR_REF));

        for(int i = 0; i < 6000000; ++i) {}
    }
}

