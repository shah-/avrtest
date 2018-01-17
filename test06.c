#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define F_CPU 8000000UL
#include <util/delay.h>

/*
   ATMEGA 328P
   Vcc -- 0.068 uF ceramic -- Gnd
   RTC 9 -- 32768 Hz QZ -- 10
   9 -- 32768HZ -- 10
   Serial in/out
 */

///////////////////////////////////////////////////////////////////////////////
//LCD

// RST, CE, DC, DIN, CLK

static void lcd_write_cmd(uint8_t cmd)
{
    uint8_t i = 8;
    while(i --) {
        if(cmd & (1 << i)) {
            PORTC = 0b100100;
            PORTC = 0b100110;
        }
        else {
            PORTC = 0b100000;
            PORTC = 0b100010;
        }
        PORTC = 0b100000;
    }
}

static void lcd_write_data(uint8_t data)
{
    uint8_t i = 8;
    while(i --) {
        if(data & (1 << i)) {
            PORTC = 0b101100;
            PORTC = 0b101110;
        }
        else {
            PORTC = 0b101000;
            PORTC = 0b101010;
        }
        PORTC = 0b101000;
    }
}

static void lcd_init()
{
    uint16_t i = 504;
    // RST, CE, DC, DIN, CLK, 0
    DDRC = 0b111110;
    PORTC = 0b000000;
    _delay_ms(10);
    PORTC = 0b100000;
    lcd_write_cmd(0x21);
    lcd_write_cmd(0x13);
    lcd_write_cmd(0x06);
    lcd_write_cmd(0xC2);
    lcd_write_cmd(0x20);
    lcd_write_cmd(0x09);

    /* Clear LCD RAM */
    lcd_write_cmd(0x80);
    lcd_write_cmd(0x40);

    /* Activate LCD */
    lcd_write_cmd(0x08);
    lcd_write_cmd(0x0C);
}

///////////////////////////////////////////////////////////////////////////////
//RTC

static void rtc_init(void)
{  
    TCCR2A = 0x00;  //overflow
    TCCR2B = 0x02;  //5 gives 1 sec. prescale 
    TIMSK2 = 0x01;  //enable timer2A overflow interrupt
    ASSR  = 0x20;   //enable asynchronous mode
}

///////////////////////////////////////////////////////////////////////////////
//UART

#define USART_BAUD 38400UL
#define USART_UBBR_VALUE ((F_CPU / (USART_BAUD << 4)) - 1)

static void uart_init()
{
    UBRR0H = (uint8_t) (USART_UBBR_VALUE >> 8);
    UBRR0L = (uint8_t) USART_UBBR_VALUE;

    UCSR0A = 0;


    //Enable UART
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

    //8 data bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static uint8_t uart_rx()
{
    while(!(UCSR0A & (1 << RXC0)));

    return UDR0;
}

static void uart_tx(uint8_t data)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

static void p_line(const char* pp)
{
    while(*pp) {
        uart_tx(*pp++);
    }
    uart_tx('\r');
    uart_tx('\n');
}

///////////////////////////////////////////////////////////////////////////////
//Interrupts
ISR(USART_RX_vect)
{
}

ISR(TIMER2_OVF_vect)
{
}

///////////////////////////////////////////////////////////////////////////////
static char val[] = {'0', '0', '0', '0', '0', '0', '0', 0};

static void reset_val()
{
    char *p = val;
    while(*p) {
        *p ++ = '0';
    }
    TCNT2 = 0; //zero RTC counter
}

static void update_val()
{
    char* p = val + sizeof(val) - 2;
    while(p >= val) {
        if('9' >= ++(*p))
            break;
        *p -- = '0';
    }
}

static void p_val()
{
    p_line(val);
}

static void sys_init()
{
    cli();
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    lcd_init();
    uart_init();
    rtc_init();
    reset_val();
    sei();
}

static void draw_cols(uint8_t x, uint8_t y, uint8_t* pix, uint8_t col_x)
{
    uint8_t col1_x = col_x - 100;
    if(2 == y) {
        if((col_x - 5 < x && col_x + 5 > x) || (col1_x - 5 < x && col1_x + 5 > x) ) {
            *pix = 0b10000001;
        }
    }
    else {
        if((col_x - 4 < x && col_x + 4 > x) || (col1_x - 4 < x && col1_x + 4 > x)) {
            *pix = 0b11111111;
        }
    }
}

static void draw(uint8_t col_x)
{
    uint8_t x, y, pix;
    for(y = 0; y < 6; y ++) {
        for(x = 0; x < 84; x ++) {
            pix = 0;
            draw_cols(x, y, &pix, col_x);
            lcd_write_data(pix);
        }
    }
}

static void update_scene(uint8_t* col_x)
{
    (*col_x) -= 1;
}

int main(void)
{
    uint8_t col_x = 100;
    sys_init();
    while(1) {
        sleep_cpu();
        if(UCSR0A & (1 << RXC0)) {
            uint8_t ch = UDR0;
            if(ch == '\r') {
                p_line("Value reset");
                reset_val();
            }
            else {
                p_line("Press <ENTER> to reset the value");
            }
        }
        else {
            update_val();
            p_val();
            draw(col_x);
            update_scene(&col_x);
        }
    } 

    return 1;
}

