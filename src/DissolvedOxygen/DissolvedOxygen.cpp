#include "DissolvedOxygen.h"

#define VREF 5000
#define ADC_RES 4095

#define CAL1_V (1500) // mv
#define CAL1_T (21)   // ℃

const uint16_t DO_Table[41] = {
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410};

float getDO(uint8_t pin, uint8_t temperature_c)
{
    uint16_t ADC_Raw = analogRead(pin);
    uint16_t ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;

    uint16_t V_saturation = (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;
    return float((ADC_Voltage * DO_Table[temperature_c] / V_saturation) / 1000);
}