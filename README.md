# Temperature Sensing with MSP430FR4133

## Overview
Implement a temperature sensing application using the MSP430FR4133 microcontroller's on-chip temperature sensor. The program collects temperature data, calculates averages, and displays results on the LCD module. The program ensures data consistency through a checkpoint mechanism.

## Features
- **Temperature Data Collection**: Read temperature data from the MSP430's sensor
- **Data Conversion**: Convert raw ADC readings to both Celsius and Fahrenheit
- **Average Calculation**: Maintain a running average of temperature readings
- **LCD Display**: Show the average temperature on the LCD
- **FRAM Storage**: Use FRAM to store temperature data
- **Checkpoint Mechanism**: Implement data integrity protection through parity checks and backup storage

## Requirements
- MSP430FR4133 LaunchPad
- Energia IDE (or Code Composer Studio)
- LCD_Launchpad library

## Code Structure
- **ADC Configuration**: Sets up the analog-to-digital converter for temperature sensing
- **Timer Configuration**: Configures timer for periodic measurements
- **FRAM Operations**: Functions for reading from and writing to FRAM memory
- **Data Integrity**: Parity calculation and data verification routines
- **Recovery Mechanism**: Functions to restore data from backup when corruption is detected

## Usage
1. Upload the code to MSP430FR4133 LaunchPad
2. The program will automatically begin temperature readings
3. Average temperature will be displayed on the LCD
4. Serial monitor (9600 baud) can be used to observe detailed information:
   - Current temperature reading
   - Running sum of temperature readings
   - Number of readings taken
   - Current average temperature
   - Indicators when data corruption and recovery occur

## Demonstration
The program will run normally for the first 10 readings. On the 10th reading, it will deliberately corrupt the data to demonstrate the integrity issue. The checkpoint mechanism will then detect this corruption and restore the data from the backup storage, ensuring data consistency is maintained.