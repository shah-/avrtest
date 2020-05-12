#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdio.h>

#define F_CPU 8000000UL
#include <util/delay.h>

/*
    ATMEGA328P
    GND - 8, 22
    VCC - 7, 20
    RX - PD0
    TX - PD1
    5 (PD3) - 100K 23(ADC0)
    23 (ADC0) - 2.2uF GND

    Loop 5V: 8Mhz-16.3ma, 1Mhz-10.9ma, 128Khz-9.9ma, 31Khz-9.9ma
    Loop 3.3V: 8Mhz-3.74ma, 1Mhz-0.74ma, 128Kh-0.21ma, 31Khz-0.15ma
*/

#define USART_BAUD 38400UL
#define USART_UBBR_VALUE ((F_CPU / (USART_BAUD << 4)) - 1)

static void uart_init() {
    UBRR0 = USART_UBBR_VALUE;
    UCSR0A = 0;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    //8 data bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

ISR(USART_RX_vect)
{
}

static int uart_print_char(char ch, FILE* pf)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = ch;
    return 0;
}

static FILE uart_str = {
    .put = uart_print_char,
    .flags = _FDEV_SETUP_WRITE
};

static void gpio_enable_reset_pullup()
{
    PORTC = 0b01000000;
}

static void cpu_clock_div_set(uint8_t num)
{
    cli();
    CLKPR = 0b10000000;
    CLKPR = num & 0b00001111;
    sei();
}

static void sys_init()
{
    cli();
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    gpio_enable_reset_pullup();
    uart_init();
    sei();
}

static void adc_set_src_1_1v__ref_avcc_with_cap_at_aref_pin()   { ADMUX = 0b01001110; }
static void adc_set_src_adc0__ref_vcc_with_cap_at_aref_pin()    { ADMUX = 0b01000000; }
static void adc_set_src_temp__ref_1_1v_with_cap_at_aref_pin()   { ADMUX = 0b11001000; }
static void adc_enable_start_conversion__div_128()              { ADCSRA = 0b11000111; }
static void adc_enable_start_conversion__div_2()                { ADCSRA = 0b11000001; }
static void adc_wait_convertion()                               { while(ADCSRA & 0b01000000); }
static uint16_t adc_read_result_16()                            { return ADC; }
static void adc_src_gnd__ref_off()                              { ADMUX = 0b00001111; }
static void adc_disable__div_128()                              { ADCSRA = 0b00000111; }

static void adc_release()
{
    adc_src_gnd__ref_off();
    adc_disable__div_128();
}

static void f0_vcc_read(const char* descr)
{
    adc_set_src_1_1v__ref_avcc_with_cap_at_aref_pin();
    adc_enable_start_conversion__div_2();
    adc_wait_convertion();
    float vcc = adc_read_result_16();
    uint8_t cnt = 0xFF;
    while(cnt --) {
        adc_enable_start_conversion__div_2();
        adc_wait_convertion();
        vcc += adc_read_result_16();
        vcc /= 2.0;
    }
    adc_release();
    fprintf(&uart_str, "%s: %u mV\r\n", descr, (uint16_t)(1100.0 * 1023.0 / vcc));
}

static void f0_gpio_set(const char* descr)
{
    DDRD = 0b00001000;
    PORTD = 0b00001000;
    fprintf(&uart_str, "%s\r\n", descr);
}

static void f0_gpio_unset(const char* descr)
{
    DDRD = 0b00001000;
    PORTD = 0b00000000;
    fprintf(&uart_str, "%s\r\n", descr);
}

static uint16_t adc_warmup_wait_read()
{
    adc_enable_start_conversion__div_2();
    adc_wait_convertion();
    adc_enable_start_conversion__div_2();
    adc_wait_convertion();
    return adc_read_result_16();
}

static void f0_adc_read(const char* descr)
{
    adc_set_src_adc0__ref_vcc_with_cap_at_aref_pin();
    uint16_t val = adc_warmup_wait_read();
    adc_release();
    fprintf(&uart_str, "%s: %u\r\n", descr, val);
}

static void f0_temp_read(const char* descr)
{
    adc_set_src_temp__ref_1_1v_with_cap_at_aref_pin();
    uint16_t val = adc_warmup_wait_read();
    adc_release();
    fprintf(&uart_str, "%s: %u\r\n", descr, val);
}

static void f0_gpio_time(const char* descr)
{
    DDRD = 0b00010000;
    PORTD = 0b00000000;
    DDRD = 0b00000000;
    PORTD = 0b00010000;
//    __asm__ __volatile__("nop");
    fprintf(&uart_str, "%s %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d\r\n", descr
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000)
            , !!(PIND & 0b00010000));
}

static void f0_cpu_clock_test()
{
    fprintf(&uart_str, "CPU 31250 Hz 5s\r\n"); //3.3v0.15ma,5v9.9ma
    _delay_ms(5);
    cpu_clock_div_set(0b00001000);
    _delay_ms(5000 / 256);
    cpu_clock_div_set(0b00000000);
    fprintf(&uart_str, "CPU 128 KHz 5s\r\n"); //3.3v0.21ma,5v9.9ma
    _delay_ms(5);
    cpu_clock_div_set(0b00000110);
    _delay_ms(5000 / 64);
    cpu_clock_div_set(0b00000000);
    fprintf(&uart_str, "CPU 1 Mhz 5s\r\n"); //3.3v0.74ma,5v10.9ma
    _delay_ms(5);
    cpu_clock_div_set(0b00000011);
    _delay_ms(5000 / 8);
    cpu_clock_div_set(0b00000000);
    fprintf(&uart_str, "CPU 8 Mhz 5s\r\n"); //3.3v3.74ma,5v16.3ma
    _delay_ms(5000);
    fprintf(&uart_str, "CPU done\r\n\r\n");
}

static void f0_menu()
{
    struct {
        char* descr;
        void (*proc)();
    } menu[] = {
        {"VCC read",    f0_vcc_read},
        {"GPIO set",    f0_gpio_set},
        {"GPIO unset",  f0_gpio_unset},
        {"ADC read",    f0_adc_read},
        {"Temp. read",  f0_temp_read},
        {"GPIO dU/dT",  f0_gpio_time},
        {"Clock test",  f0_cpu_clock_test},
    };

    uint8_t ch = UDR0;
    if(ch < 'a' || ch >= 'a' + sizeof(menu) / sizeof(menu[0])) {
        fprintf(&uart_str, "\r\nUsage:\r\n");
        for(uint8_t i = 0; i < sizeof(menu) / sizeof(menu[0]); i ++) {
            fprintf(&uart_str, "%c: %s\r\n", 'a' + i, menu[i].descr);
        }
        fprintf(&uart_str, "\r\n");
        return;
    }

    ch -= 'a';
    menu[ch].proc(menu[ch].descr);
}

int main(void)
{
    sys_init();
    while(1) {
        sleep_cpu();
        if(!(UCSR0A & (1 << RXC0)))
            continue;
        f0_menu();
    }
    return 0;
}

