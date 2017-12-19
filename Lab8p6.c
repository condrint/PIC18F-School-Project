#include <p18f4321.h>
#include <stdio.h>
#include <math.h>
#include <usart.h>
#include <pic18f4321.h>
#pragma config OSC = INTIO2
#pragma config WDT=OFF
#pragma config LVP=OFF
#pragma config BOR =OFF
#define TPulse PORTCbits.RC0
#define RED PORTCbits.RC3
#define GREEN PORTCbits.RC4
#define BLUE PORTCbits.RC5

//Prototype Area
void Init_IO(void);
void Wait_Half_Second(void);
void Wait_One_Second(void);
void gen_short_beep (void);
void gen_long_beep (void);
void play_series_beep_tone(char);
unsigned int Tint(float);
unsigned int GET_FULL_ADC(void);
void SPI_out(char);
void T0ISR(void);
void interrupt high_priority chk_isr(void); 
void Do_Init(void);
void init_UART(void);
void Serial_RX_ISR(void);
void do_print_menu(void);
void Do_Pattern(void);
void LED_Output(char, char);

//Variable Declarations
char RX_char;
char RX_flag;
int count = 0;
int sequence = 0;
int TMR_flag;
float Vin;
char SWD_flag = 0;
char SWU_flag = 0;
int Seq[8] = {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F};

void init_UART() //Initialized UART
{
    OpenUSART (USART_TX_INT_OFF & USART_RX_INT_OFF &
    USART_ASYNCH_MODE & USART_EIGHT_BIT & USART_CONT_RX &
    USART_BRGH_HIGH, 25);
    OSCCON = 0x60;
}

void putch (char c)
{
    while (!TRMT);
    TXREG = c;
}

void Init_IO()
{
    TRISA = 0x01;                           //Set Port A, bit 0 as input and others as output
    TRISB = 0x03;                           //Set PORT B, bit 0 and 1 as inputs and others as output
    TRISC = 0x00;                           //Set Port C as output
    TRISD = 0x00;                           //Set Port D as output
    TRISE = 0x00;                           //Set Port E as output
}

void Wait_Half_Second()
{
    for (int i=0; i < 8500; i ++);          //For loop until 500 msec is reached
}

void Wait_One_Second()
{
    Wait_Half_Second();                     //Wait for half second (or 500 msec)
    Wait_Half_Second();                     //Wait for half second (or 500 msec)
}

void gen_short_beep (void)
{
    PR2 = 0x52;                             //values for short beep
    CCPR1L = 0x29;
    CCP1CON = 0x1C;
    T2CON = 0x05;                           //Turn T2 on here
    Wait_Half_Second();                     //Wait half-second
    T2CON = 0x00;                           //Turn T2 off here
    Wait_Half_Second();
}

void gen_long_beep (void)
{
    PR2 = 0xA6;                             //values for long beep
    CCPR1L = 0x37;
    CCP1CON = 0x0C;
    T2CON = 0x05;                           //Turn T2 on here
    Wait_One_Second();                      //Wait half-second
    T2CON = 0x00;                           //Turn T2 off here
    Wait_One_Second();
}

void play_series_beep_tone(char sequence_number)
{
    if (sequence_number == 0)               //If sequence is 0, generate 2 short beeps
    {
        gen_short_beep();
        gen_short_beep();
    }
    else
    {
        for(int i = 1; i <= sequence_number; i++)
        {
            if ((i % 2) == 0)               //Otherwise, generate sound accordingly
                gen_long_beep();
            else
                gen_short_beep();
        }
    }
}

unsigned int GET_FULL_ADC(void)
{
    int result;
    ADCON0bits.GO=1;                        //Start Conversion
    while(ADCON0bits.DONE==1);              //wait for conversion to be completed
    result = (ADRESH * 0x100) + ADRESL;     //combine result of upper byte and
                                            //lower byte into result
    return result;                          //return the result.
}

unsigned int Tint (float Vin)
{
    int x;
    if (Vin >= 3)                           //Controls RGB LED for speed signal
    {
        x = 2;
        RED = 1;
        GREEN = 0;
        BLUE = 0;
    }
    else if(Vin >= 2)
    {
        x = 1;
        RED = 0;
        GREEN = 1;
        BLUE = 0;
    }
    else
    {
        x = 0;
        RED = 0;
        GREEN = 0;
        BLUE = 1;
    }
    return x;
}

void interrupt high_priority T0ISR()
{
    int T0L, T0H;
    if (Tint(Vin) == 0)                     //Low Voltage = 200ms
    {
        T0H = 0xF3;
        T0L = 0xD5;
    }
    else if (Tint(Vin) == 1)                //Medium Voltage = 100ms
    {
        T0H = 0xE7;
        T0L = 0x89;
    }
    else                                    //High Voltage = 50ms
    {
        T0H = 0xCF;
        T0L = 0x10;
    }
    INTCONbits.TMR0IF=0;                    //Clear the interrupt flag
    TMR0H=T0H;                              //Reload Timer High and
    TMR0L=T0L;                              //Timer Low
    TPulse = ~TPulse;                       //flip logic state of TPulse
    TMR_flag = 1;
}

void INT0_ISR()
{
    INTCONbits.INT0IF = 0;                  //Clear INT0 interrupt flag
    SWU_flag = 1;                           //Set switch up flag
}

void INT1_ISR()
{
    INTCON3bits.INT1IF = 0;                 //Clear INT1 interrupt flag
    SWD_flag = 1;                           //Set switch down flag
}

void interrupt high_priority chk_isr(void)
{
    if (PIR1bits.RCIF == 1)
    Serial_RX_ISR();                        //If RCIF is set it goes to Serial_RX_ISR
    if (INTCONbits.TMR0IF == 1)
    {
        T0ISR();                            //If TMR0IF is set, go to T0ISR
    }
    if (INTCONbits.INT0IF == 1)
    {
        INT0_ISR();                         //if INT0IF is set, go to INT0_ISR
    }
    if (INTCON3bits.INT1IF == 1)
    {
        INT1_ISR();                         //if INT1IF is set, go to INT1_ISR
    }
}

void Serial_RX_ISR(void)
{
    PIR1bits.RCIF = 0;                      //Clear RCIF bit
    RX_char = RCREG;                        //read the receive character from Serial Port
    RX_flag = 1;                            //Set software RX_flag to indicate reception
                                            //of rx character
} 

void Do_Init(void)
{
    Init_IO();
    init_UART();
    ADCON0 = 0x01;                          //Change AN0 from analog to digital signal
    ADCON1 = 0x0E;                          //Change 0x0E from analog to digital signal
    ADCON2 = 0xA9;                          //Change 0xA9 from analog to digital signal
    T0CON=0x02;                             //Timer0 off, increment on positive
                                            //edge, 1:8 prescaler
    TMR0H=0x0B;                             //Set Timer High with dummy values
    TMR0L=0xDB;                             //Set Timer Low with dummy values
    INTCONbits.TMR0IE=1;                    //Set interrupt enable
    INTCONbits.TMR0IF=0;                    //Clear interrupt flag
    INTCONbits.GIE=1;                       //Set the Global Interrupt Enable
    T0CONbits.TMR0ON=1;                     //Turn on Timer0

    RX_char = RCREG;                        //read RCRG just to clear it
    RX_flag = 0;                            //Initialize software RX_flag to zero

    PIR1bits.RCIF = 0;                      //Clear the RX Receive flag
    PIE1bits.RCIE = 1;                      //Enables Rx Receive interrupt
    INTCONbits.PEIE = 1;                    //Enables PE INT for Peripheral Interrupt
    INTCONbits.GIE = 1;                     //Enables Global interrupt
    
    INTCONbits.INT0IF = 0;                  //INT0 IF is in INTCON 
 	INTCON3bits.INT1IF = 0;                 //INT1 IF is in INTCON3 
    INTCONbits.INT0IE =	1;                  //INT0 IE is in INTCON 
 	INTCON3bits.INT1IE = 1;                 //INT1 IE is in INTCON3 
    INTCON2bits.INTEDG0 = 0;                //Edge programming for INT0 and INT1
    INTCON2bits.INTEDG1 = 0;                //are in INTCON2 
}

void Do_Pattern(void)
{
    TMR_flag = 0;                           //Clear TMR_flag
    switch(sequence)                        //Assign patterns based on the sequence
    {
        case 0:
            if(count == 0) LED_Output(0,0);
            else LED_Output(0xff, 0xff);
            count++;
            if(count > 1) count = 0;
            break;
        case 1:
            if((count == 0) || (count == 1)) LED_Output(0,0);
            else LED_Output(0xff, 0xff);
            count++;
            if(count > 3) count = 0;
            break;
        case 2:
            if(count == 0) LED_Output(0xAA,0xAA);
            else if(count == 2) LED_Output(0x55, 0x55);
            else LED_Output(0xff, 0xff);
            count++;
            if(count > 3) count = 0;
            break;
        case 3:
            if(count == 0) LED_Output(0x00,0xff);
            else if(count == 1) LED_Output(0xff, 0xff);
            else if(count == 2) LED_Output(0xff, 0x00);
            else LED_Output(0xff, 0xff);
            count++;
            if(count > 3) count = 0;
            break;            
        case 4:
            if(count == 0) LED_Output(0xFC,0x03);
            else if((count == 1)||(count == 3)) LED_Output(0xFF, 0xFF);
            else LED_Output(0x03, 0xFC);
            count++;
            if(count > 3) count = 0;
            break;
        case 5:
            if(count == 0) LED_Output(0xF0,0xF0);
            else if(count == 2) LED_Output(0x0F, 0x0F);
            else LED_Output(0xFF, 0xFF);
            count++;
            if(count > 3) count = 0;
            break;
        case 6:
            if((count%2) == 1)
            {
                LED_Output(0xFF, 0xFF);
            }
            else if(count <16)
            {
                LED_Output(Seq[count/2], 0xFF);
            }
            else
            {
                LED_Output(0xFF, Seq[(count/2)-8]);
            }
            count++;
            if(count > 31) {count = 0;}
            break;
        case 7:
            if((count%2) == 1)
            {
                LED_Output(0xFF, 0xFF);
            }
            else if(count <16)
            {
                LED_Output(Seq[count/2], 0xFF);
            }
            else if(count < 31)
            {
                LED_Output(0xFF, Seq[(count/2)-8]);
            }
            else if(count < 45)
            {
                LED_Output(0xFF, Seq[(44-count)/2]);
            }
            else
            {
                LED_Output(Seq[(60 - count)/2], 0xFF);
            }
            count++;
            if(count > 61) count = 0;
            break;
        default:
            LED_Output(0xFF, 0xFF);
            count = 0;
            break;
    }
}

void LED_Output (char Value1, char Value2) 
{ 
    PORTA = (Value1 & 0x0f) << 2;           //Shift bits where the least significant LED starts in PORTA    
 	PORTB = (Value1 & 0xf0) >> 2;           //Shift bits where the least significant LED starts in PORTB 
 	PORTD = Value2;                         //no need to shift because all the LEDs are used in this port
}

void do_print_menu(void) 
{ 
 	printf ("\r\n\n Menu\r\n\n");           //Print out choice to TeraTerms
    printf ("Choose pattern from 0 to 7\r\n");
    printf ("\r\nEnter choice : "); 
} 

void main()
{
    Do_Init();                              //do all the initialization
    do_print_menu();                        //print the menu
    
    while (1)                               //Infinite While loop
    {
        
        Vin = GET_FULL_ADC() * 5.0 / 1000.0;
        PORTE = ~sequence;                  //Port E LED's display sequence in binary form
        
        if(TMR_flag == 1)
        {
            Do_Pattern();
        }
        if (SWU_flag == 1)
        {
            if(sequence < 7)
            {
                sequence++;                 //Sequence increases if Up Switch is pressed
            }
            else
            {
                sequence = 0;               //If sequence was 7, return the sequence to 0
            };
            SWU_flag = 0;                   //Clear the SWU_flag
        }
        if (SWD_flag == 1)
        {
            if(sequence > 0)
            {
                --sequence;                 //Sequence decreases if Down Switch is pressed
            }
            else
            {
                sequence = 7;               //If the sequence was 0, return the sequence to 7
            };
            SWD_flag = 0;                   //Clear the SWD_flag
        }
        
        if (RX_flag == 1)                   //wait until RX_flag is set to 1 indicating rx char
        {
            printf ("%c\r\n\n", RX_char);
            RX_flag = 0;
            if ((RX_char >= '0') && (RX_char <= '7'))
            {
                sequence = RX_char - '0';
            }

            else
            {
                printf ("\n\n*** INVALID ENTRY ***\n\n");
            }
            do_print_menu();
        }
        
    }
}