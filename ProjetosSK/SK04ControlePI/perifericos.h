#ifndef PERIFERICOS_H
#define PERIFERICOS_H

#include "driverlib.h"
#include "device.h"

#define TS_S   0.005f   // periodo de amostragem [s]
#define VREF   3.0f     // VREFHI da LaunchPad (REF5030) [V]

void perifericos_init(void);

// Escreve no DACOUTA (J3.30), saturando entre 0 e VREF
static inline void dac_escreve_volts(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > VREF) v = VREF;
    DAC_setShadowValue(DACA_BASE, (uint16_t)(v * 4095.0f / VREF));
}

// Le o resultado do SOC0 (ADCINA3, J3.26) em volts
static inline float adc_le_volts(void)
{
    return (float)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0) * VREF / 4095.0f;
}

#endif
