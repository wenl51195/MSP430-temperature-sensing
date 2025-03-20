#include <msp430.h>
#include "LCD_Launchpad.h"

// Temperature sensor calibration values - from datasheet
#define CALADC_15V_30C  *((unsigned int *)0x1A1A)  // 30°C calibration value
#define CALADC_15V_85C  *((unsigned int *)0x1A1C)  // 85°C calibration value

// FRAM memory address settings
#define FRAM_TEST_START 0x1800
unsigned long *FRAM_write_ptr;   // Main data storage pointer
unsigned long *FRAM_write_ptr1;  // Backup data storage pointer
unsigned long *FRAM_read_ptr;    // Read data pointer

// Temperature related variables
float temp;         // Raw ADC reading
float IntDegF;      // Temperature in Fahrenheit
float IntDegC;      // Temperature in Celsius
int tempSum = 0;    // Sum of temperatures
float tempCount = 0; // Temperature count
float tempAvg = 0.00; // Average temperature
int i = 0, n = 0;   // Counters

// LCD object
LCD_LAUNCHPAD myLCD;

/**
 * Calculate parity value (original implementation, not used)
 * Calculates parity bit for an integer value
 * @param value Integer value to calculate parity for
 * @return Parity bit (0 or 1)
 */

unsigned char CalculateParity(unsigned int value) {
    unsigned char parity = 0;
    while (value) {
        parity ^= (value & 1);
        value >>= 1;
    }
    return parity;
}

/**
 * Write data to FRAM
 * Saves both main data and backup data
 * Part 2: Deliberately corrupts data under specific conditions to demonstrate data inconsistency
 */
void FRAMWrite(void) {
    unsigned char parity = CalculateParity(tempSum);
    SYSCFG0 &= ~DFWP;  // Disable FRAM write protection
    
    // Save temperature sum with parity bit
    *FRAM_write_ptr = ((unsigned long) tempSum << 8) | parity;
    // Save backup data
    *FRAM_write_ptr1 = (unsigned long) tempSum;
    
    // Part 2: Data consistency issue demonstration
    // Deliberately corrupt data when i=10
    if (i == 10) {
        Serial.print("(correct) Sum = ");
        Serial.println(tempSum);
        tempSum = 4345;  // Deliberately modify to incorrect value
        *FRAM_write_ptr = ((unsigned long) tempSum << 8) | parity;  // Only modify main data, not parity code or backup
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
    i++;
    
    SYSCFG0 |= DFWP;  // Re-enable FRAM write protection
}

/**
 * Read data from FRAM
 * Part 3: Implements checkpoint mechanism to detect and repair corrupted data
 */
void FRAMRead(void) {
    unsigned long data = *FRAM_read_ptr;
    unsigned char read_parity = data & 0xFF;       // Get stored parity bit
    unsigned int read_value = data >> 8;           // Get stored data value
    unsigned char calc_parity = CalculateParity(read_value);  // Calculate parity bit for current data
    
    // Part 3: Checkpoint mechanism implementation
    // If data consistency check passes, use the read value
    // Otherwise, restore data from backup
    if (read_parity == calc_parity) {
        tempSum = read_value;
    } else {
        tempSum = RestoreCounter();  // Restore data from backup
        tempCount--;                 // Adjust count to maintain accurate average
    }
}

/**
 * Restore data from backup
 * @return Restored data value
 */
unsigned int RestoreCounter() {
    unsigned int data = *FRAM_write_ptr1;
    return data;
}

/**
 * Setup initialization
 */
void setup() {
    // Initialize serial and LCD
    Serial.begin(9600);
    myLCD.init();
    myLCD.clear();
    
    // Setup FRAM pointers
    FRAM_write_ptr = (unsigned long *)FRAM_TEST_START;
    FRAM_write_ptr1 = FRAM_write_ptr+1;
    FRAM_read_ptr = FRAM_write_ptr;
    
    // Disable watchdog timer
    //WDTCTL = WDTPW | WDTHOLD;

    // Disable high impedance mode
    PM5CTL0 &= ~LOCKLPM5;

    // Setup timer
    TA0CCTL0 |= CCIE;                  // Enable timer interrupt
    TA0CCR0 = 65535;                   // Set count value
    TA0CTL = TASSEL__ACLK | MC__UP;    // Use ACLK clock source, up-count mode

    // Configure ADC - Pulse sample mode
    ADCCTL0 |= ADCSHT_8 | ADCON;       // ADC ON, sampling time >30us
    ADCCTL1 |= ADCSHP;                 // Software trigger, single channel/conversion
    ADCCTL2 |= ADCRES;                 // 10-bit conversion results
    ADCMCTL0 |= ADCSREF_1 | ADCINCH_12; // ADC input channel A12 => temperature sensor
    ADCIE |= ADCIE0;                    // Enable ADC conversion complete interrupt request

    // Configure reference voltage
    PMMCTL0_H = PMMPW_H;               // Unlock PMM registers
    PMMCTL2 |= INTREFEN | TSENSOREN;   // Enable internal reference and temperature sensor

    __delay_cycles(400);               // Wait for reference voltage to stabilize

    // Low power mode settings
    //__bis_SR_register(LPM0_bits | GIE);    // LPM3 with interrupts enabled
    //__no_operation();                      // Only for debugger
}

/**
 * Main loop
 * Part 1: Temperature data collection and display
 */
void loop() {
    myLCD.clear();
    Serial.print("Temp = ");
    Serial.println(IntDegC);

    if (IntDegC != 0) { // Ensure valid temperature has been read
        // If not the first reading, read previous temperature sum from FRAM
        if (n == 1) {
            FRAMRead();
        }
        
        // Add current temperature to sum
        tempSum += IntDegC;

        // Write updated temperature sum to FRAM
        FRAMWrite();
        n = 1;

        // Calculate average temperature
        tempCount++;
        tempAvg = tempSum/tempCount;

        // Display debug information
        Serial.print("Sum = ");
        Serial.println(tempSum);
        Serial.println(tempCount);
    }

    // Display average temperature
    Serial.print("Avg = ");
    Serial.println(tempAvg);
    Serial.println("");
    myLCD.println(tempAvg);  // Display average temperature on LCD

    delay(500);  // Delay 500ms
}

/**
 * ADC interrupt service routine
 * Process temperature readings and convert to Celsius and Fahrenheit
 */
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void) {
    //__delay_cycles(400);  
    temp = ADCMEM0;  // Get ADC reading

    // Convert to Celsius temperature
    IntDegC = round((temp-CALADC_15V_30C) * (85-30)/(CALADC_15V_85C-CALADC_15V_30C)+30);

    // Convert to Fahrenheit temperature
    IntDegF = round(9*IntDegC/5 + 32);
}

/**
 * Timer A0 interrupt service routine
 * Periodically triggers ADC conversion to read temperature
 */
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
    ADCCTL0 |= ADCENC | ADCSC;  // Start sampling and conversion
}