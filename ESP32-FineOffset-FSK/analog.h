#include <esp32-hal.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

#define ADC_SAMPLES 16

static esp_adc_cal_characteristics_t *adc_chars = 0;

void analogSetup(int pin) {
    pinMode(pin, INPUT);

    adc1_channel_t channel = (adc1_channel_t)digitalPinToAnalogChannel(pin);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);

    if (!adc_chars) {
        adc_chars = (esp_adc_cal_characteristics_t*)calloc(1, sizeof(esp_adc_cal_characteristics_t));
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                ADC_WIDTH_BIT_12, 1100, adc_chars);
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            printf("ADC uses Two Point Value\n");
        } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            printf("ADC uses eFuse Vref\n");
        } else {
            printf("ADC uses Default Vref\n");
        }
    }
}

// analogSample returns the millivolts measured
uint16_t analogSample(int pin) {
    adc1_channel_t channel = (adc1_channel_t)digitalPinToAnalogChannel(pin);

#if 0
    // median
    std::vector<uint16_t> readings (ADC_SAMPLES);
    for (int i=0; i<ADC_SAMPLES; i++) readings[i] = adc1_get_raw(channel);
    std::sort(readings.begin(), readings.end());
    uint32_t adc_reading = readings[ADC_SAMPLES/2];
    uint32_t millivolts = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    printf("ADC %d -%d/+%d -> %dmV\n", adc_reading, adc_reading-readings[0],
            readings[ADC_SAMPLES-1]-adc_reading, millivolts);
#else
    // averaging
    uint32_t adc_reading = 0;
    for (int i=0; i<ADC_SAMPLES; i++) {
        adc_reading += adc1_get_raw(channel);
    }
    adc_reading = (adc_reading + ADC_SAMPLES/2) / ADC_SAMPLES;
    uint32_t millivolts = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    //printf("ADC %d -> %dmV\n", adc_reading, millivolts);
#endif

    return millivolts;
}
