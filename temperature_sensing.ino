#include <msp430.h>
#include "LCD_Launchpad.h"

// Temperature sensor calibration values
#define CALADC_15V_30C  *((unsigned int *)0x1A1A)  // 30°C calibration value
#define CALADC_15V_85C  *((unsigned int *)0x1A1C)  // 85°C calibration value

// FRAM memory address settings
#define FRAM_TEST_START 0x1800
unsigned long *FRAM_write_ptr;   // Main data storage pointer
unsigned long *FRAM_write_ptr1;  // Backup data storage pointer
unsigned long *FRAM_read_ptr;    // Read data pointer

// Temperature related variables
float temp;
float IntDegF;
float IntDegC;
int tempSum = 0;
float tempCount = 0;
float tempAvg = 0.00;
int i = 0, n = 0;

// LCD object
LCD_LAUNCHPAD myLCD;

unsigned char CalculateParity(unsigned int value) {
    unsigned char parity = 0;
    while (value) {
        parity ^= (value & 1);
        value >>= 1;
    }
    return parity;
}


// Part 2: Deliberately corrupts data under specific conditions to demonstrate data inconsistency
void FRAMWrite(void) {
    unsigned char parity = CalculateParity(tempSum);
    SYSCFG0 &= ~DFWP;  // Disable FRAM write protection
    
    *FRAM_write_ptr = ((unsigned long) tempSum << 8) | parity;
    *FRAM_write_ptr1 = (unsigned long) tempSum;
    
    // Part 2: Data consistency issue demonstration
    // Deliberately corrupt data when i=10
    if (i == 10) {
        Serial.print("(correct) Sum = ");
        Serial.println(tempSum);
        tempSum = 4345;
        *FRAM_write_ptr = ((unsigned long) tempSum << 8) | parity;
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
    i++;
    
    SYSCFG0 |= DFWP;  // Re-enable FRAM write protection
}


// Part 3: Implements checkpoint mechanism to detect and repair corrupted data
void FRAMRead(void) {
    unsigned long data = *FRAM_read_ptr;
    unsigned char read_parity = data & 0xFF;
    unsigned int read_value = data >> 8;
    unsigned char calc_parity = CalculateParity(read_value);
    
    // Part 3: Checkpoint mechanism implementation
    // If data consistency check passes, use the read value
    // Otherwise, restore data from backup
    if (read_parity == calc_parity) {
        tempSum = read_value;
    } else {
        tempSum = RestoreCounter();
        tempCount--;
    }
}

// Restore data from backup
unsigned int RestoreCounter() {
    unsigned int data = *FRAM_write_ptr1;
    return data;
}

void setup() {
    // Initialize serial and LCD
    Serial.begin(9600);
    myLCD.init();
    myLCD.clear();
    
    // Setup FRAM pointers
    FRAM_write_ptr = (unsigned long *)FRAM_TEST_START;
    FRAM_write_ptr1 = FRAM_write_ptr+1;
    FRAM_read_ptr = FRAM_write_ptr;

    // Disable high impedance mode
    PM5CTL0 &= ~LOCKLPM5;

    // Setup timer
    TA0CCTL0 |= CCIE;
    TA0CCR0 = 65535;
    TA0CTL = TASSEL__ACLK | MC__UP;

    // Configure ADC - Pulse sample mode
    ADCCTL0 |= ADCSHT_8 | ADCON;
    ADCCTL1 |= ADCSHP;
    ADCCTL2 |= ADCRES;
    ADCMCTL0 |= ADCSREF_1 | ADCINCH_12;
    ADCIE |= ADCIE0;

    // Configure reference voltage
    PMMCTL0_H = PMMPW_H;
    PMMCTL2 |= INTREFEN | TSENSOREN;

    __delay_cycles(400);


    //__bis_SR_register(LPM0_bits | GIE);    // LPM3 with interrupts enabled
    //__no_operation();                      // Only for debugger
}

// Part 1: Temperature data collection and display
void loop() {
    myLCD.clear();
    Serial.print("Temp = ");
    Serial.println(IntDegC);

    if (IntDegC != 0) {
        // If not the first reading, read previous temperature sum from FRAM
        if (n == 1) {
            FRAMRead();
        }

        tempSum += IntDegC;

        FRAMWrite();
        n = 1;

        tempCount++;
        tempAvg = tempSum/tempCount;

        Serial.print("Sum = ");
        Serial.println(tempSum);
        Serial.println(tempCount);
    }

    Serial.print("Avg = ");
    Serial.println(tempAvg);
    Serial.println("");
    myLCD.println(tempAvg);

    delay(500);
}

// ADC interrupt service routine
// Process temperature readings and convert to °C and °F
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void) {
    temp = ADCMEM0;  // Get ADC reading

    // Convert to °C
    IntDegC = round((temp-CALADC_15V_30C) * (85-30)/(CALADC_15V_85C-CALADC_15V_30C) + 30);

    // Convert to °F
    IntDegF = round(9*IntDegC/5 + 32);
}

// Timer A0 interrupt service routine
// Periodically triggers ADC conversion to read temperature
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
    ADCCTL0 |= ADCENC | ADCSC;  // Start sampling and conversion
}