#ifndef _MCP3008_READER_H_
#define _MCP3008_READER_H_

int ADC_init(void);
int ADC_updateVolume();
int ADC_getVolume();
void ADC_ResetFlags();

#endif
