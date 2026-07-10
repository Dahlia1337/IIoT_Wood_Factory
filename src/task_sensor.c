#include "task_sensor.h"
#include "task.h"
#include <stdlib.h>

/* Extern ADC handle defined in main.c */
extern ADC_HandleTypeDef hadc1;

extern TIM_HandleTypeDef htim2;

/* Internal persistent dust level tracker for simulation */
#if (SIMULATION_MODE == 1)
static uint16_t current_dust_level = 100;
#endif

uint16_t Read_Temperature(void) {
#if (SIMULATION_MODE == 1)
    /* --- MODE MÔ PHỎNG --- */
    uint16_t base_temp = 45;
    int16_t fluctuation = (rand() % 5) - 2; /* -2 to +2 */
    uint16_t final_temp = (uint16_t)(base_temp + fluctuation);

    if ((rand() % 100) == 0) {
        final_temp = 85;
    }
    return final_temp;
#else
    /* --- MODE CHẠY THẬT (Đọc ADC Channel 1 - Chân PA1) --- */
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t adc_val = 0;

    /* Cấu hình động cho Channel 1 chạy ở Rank 1 */
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0; /* Trả về 0 nếu cấu hình lỗi */
    }

    /* Kích hoạt ADC */
    HAL_ADC_Start(&hadc1);
    
    /* Chờ chuyển đổi xong với thời gian Timeout an toàn cho RTOS (10ms) */
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adc_val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    
    /* Dừng ADC để tiết kiệm năng lượng và chuẩn bị cho kênh khác */
    HAL_ADC_Stop(&hadc1);
    
    return adc_val;
#endif
}

uint16_t Read_Vibration(void) {
#if (SIMULATION_MODE == 1)
    /* --- MODE MÔ PHỎNG --- */
    return (uint16_t)((rand() % 7) + 12);
#else
    /* --- MODE CHẠY THẬT (Đọc ADC Channel 2 - Chân PA2) --- */
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t adc_val = 0;

    /* Cấu hình động cho Channel 2 chạy ở Rank 1 */
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0; /* Trả về 0 nếu cấu hình kênh lỗi */
    }

    /* Kích hoạt ADC */
    HAL_ADC_Start(&hadc1);
    
    /* Chờ chuyển đổi với Timeout 10ms */
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adc_val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    
    /* Dừng ADC */
    HAL_ADC_Stop(&hadc1);
    
    return adc_val;
#endif
}

uint16_t Read_Dust(uint8_t current_status) {
#if (SIMULATION_MODE == 1)
    /* --- MODE MÔ PHỎNG --- */
    if (current_status == 1) {
        if (current_dust_level > 15) {
            current_dust_level -= 15;
        } else {
            current_dust_level = 0;
        }
    } else {
        current_dust_level += 5;
    }
    return current_dust_level;
#else
    /* --- MODE CHẠY THẬT (Đọc Tần số/Xung cảm biến từ chân PA0) --- */
    uint32_t captured_value = 0;
    uint16_t dust_density_calculated = 0;

    captured_value = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_1);

    /* * Giải thuật quy đổi thực tế: 
     * Nếu captured_value càng nhỏ nghĩa là tần số xung càng cao (Mật độ bụi lớn).
     * Dưới đây là công thức nội suy tuyến tính chuẩn mẫu để ánh xạ ra dải bụi:
     */
    if (captured_value > 0) {
        // Giả lập ánh xạ ngược: Giá trị đếm càng lớn thì mức bụi càng thấp
        dust_density_calculated = (uint16_t)(50000 / captured_value); 
    } else {
        dust_density_calculated = 0;
    }

    return dust_density_calculated;
#endif
}

void vTaskSensorSimulation(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(500);
    IndustrialSystemData_t sensor_payload;

    sensor_payload.system_status = 0;
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        sensor_payload.temperature = Read_Temperature();
        sensor_payload.vibration = Read_Vibration();
        sensor_payload.dust_density = Read_Dust(sensor_payload.system_status);

        if (xSensorQueue != NULL) {
            xQueueSend(xSensorQueue, (void *)&sensor_payload, (TickType_t)0);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}