/*
 * Oliv proudly did it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <wiringPi.h>

#include "manager.h"
#include "mcp3008reader.h"

// local variables
int adc_tolerance = 90;
int adc_value = -1;

int flag_adc_value_changed = 0;

/* Pin numbers: 
 *   pin 1 is BCM_GPIO 18.
 *   pin 4 is BCM_GPIO 23.
 *   pin 5 is BCM_GPIO 24.
 *   pin 6 is BCM_GPIO 25.
 */
#define	SPI_CLK	 14 // Clock
#define SPI_MISO 13 // Master In Slave Out
#define SPI_MOSI 12 // Master Out Slave In
#define SPI_CS   10 // Chip Select 

#define ADC_CHANNEL 0 // 0 to 7, 8 channels on the MCP3008 
#define DISPLAY_DIGIT 0

int ADC_readMCP3008(int);

int ADC_init(void)
{
	pinMode(SPI_CLK,  OUTPUT);
	pinMode(SPI_MOSI, OUTPUT);
	pinMode(SPI_CS,   OUTPUT);

	digitalWrite(SPI_CLK,  LOW);
	digitalWrite(SPI_MOSI, LOW);
	digitalWrite(SPI_CS,   LOW);

	pinMode (SPI_MISO, INPUT);

	if (ADC_updateVolume() < 0)
		return -1;

	return 1;
}

int ADC_readMCP3008(int channel)
{
	int i;
	digitalWrite(SPI_CS, HIGH);

	digitalWrite(SPI_CLK, LOW);
	digitalWrite(SPI_CS,  LOW);

	int adcCommand = channel;
	adcCommand |= 0x18; // 0x18 = 00011000
	adcCommand <<= 3;
	// Send 5 bits: 8 - 3. 8 input channels on the MCP3008.
	for (i=0; i<5; i++)
	{
		if (DISPLAY_DIGIT)
			fprintf(stderr, "DEBUG: ADC: (i=%d) ADCCOMMAND: 0x%04x\n", i, adcCommand);

		if ((adcCommand & 0x80) != 0x0) // 0x80 = 0&10000000
			digitalWrite(SPI_MOSI, HIGH);
		else
			digitalWrite(SPI_MOSI, LOW);
		adcCommand <<= 1;   
		digitalWrite(SPI_CLK, HIGH);
		digitalWrite(SPI_CLK, LOW);
	}

	int adcOut = 0;
	for (i=0; i<12; i++) // Read in one empty bit, one null bit and 10 ADC bits
	{
		digitalWrite(SPI_CLK, HIGH);
		digitalWrite(SPI_CLK, LOW);
		adcOut <<= 1;

		if (digitalRead(SPI_MISO) == HIGH) {
			// Shift one bit on the adcOut
			adcOut |= 0x1;
		}

		if (DISPLAY_DIGIT)
			fprintf(stderr, "DEBUG: ADC: ADCOUT: 0x%04x\n", (adcOut));
	}
	digitalWrite(SPI_CS, HIGH);

	adcOut >>= 1; // Drop first bit

	return adcOut & 0x3FF; // 0x3FF = 2^10, 1024.  
}

/* Returns:
 * 0 - volume was not changed
 * 1 - volume has been chagned
 */
int ADC_updateVolume() {
	int new_value = ADC_readMCP3008(ADC_CHANNEL);
	if (new_value < 0)
		return -1;

	// bounce protection
	// - tolerance
	// - fast gaps
	if (adc_value < 0 || abs(adc_value-new_value) > adc_tolerance) { // tolerance

		// fast gaps, 1 cycle
		if (adc_value >= 0 && abs(adc_value-new_value) > 300) {
			int new_value_2;

			new_value_2  = ADC_readMCP3008(ADC_CHANNEL);
			if (new_value < -1)
				return -1;

			if (abs(adc_value-new_value_2) > 300)
				fprintf(stderr, "DEBUG: ADC: volume jump confirmed: old=%d, cur=%d, new=%d\n", adc_value, new_value, new_value_2);
			else {
				if (DEBUG)
					fprintf(stderr, "DEBUG: ADC: volume jump ignored: old=%d cur=%d, new=%d\n", adc_value, new_value, new_value_2);
			}

			new_value = new_value_2;
		}

		flag_adc_value_changed++;
		adc_value = new_value;
	}

	return flag_adc_value_changed ? 1 : 0;
}

int ADC_getVolume() {
	if (adc_value < 0)
		return -1;

	int v = (int)( (float)adc_value / 10.23);
	if (v > 100) {
		fprintf(stderr, "DEBUG: ADC: fixing required: volume is over 100%%: v=%d\n", v);
		v = 100;
	}

	return v;
}

void ADC_ResetFlags() {
	flag_adc_value_changed = 0;
	return;
}
