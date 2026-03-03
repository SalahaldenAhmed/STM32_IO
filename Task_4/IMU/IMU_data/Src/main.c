/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    : main.c
  * @brief   : IMU (MPU6050) -> UART DMA with checksum + XOR encryption
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * HANDLES
 * ============================================================ */
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_tx;

/* ============================================================
 * DEFINES
 * ============================================================ */
#define MPU_ADDR      0xD0
#define MPU_REG_PWR   0x6B
#define MPU_REG_ACCEL 0x3B
#define MPU_REG_GYRO  0x43
#define ACCEL_SENS    16384.0f
#define GYRO_SENS     131.0f
#define XOR_KEY       0x5A

/* ============================================================
 * GLOBALS
 * ============================================================ */
uint8_t tx_buf[128];

/* ============================================================
 * FUNCTION PROTOTYPES
 * ============================================================ */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
void MPU6050_Init(void);
void MPU6050_Read(float *ax, float *ay, float *az,
                  float *gx, float *gy, float *gz);
void Send_IMU_Packet(float ax, float ay, float az,
                     float gx, float gy, float gz);
void Error_Handler(void);

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    /* Enable DMA IRQ after UART init */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    HAL_Delay(200);
    MPU6050_Init();
    HAL_Delay(100);

    float ax, ay, az, gx, gy, gz;

    while (1)
    {
        MPU6050_Read(&ax, &ay, &az, &gx, &gy, &gz);
        Send_IMU_Packet(ax, ay, az, gx, gy, gz);
        HAL_Delay(100);
    }
}

/* ============================================================
 * MPU6050 INIT
 * ============================================================ */
void MPU6050_Init(void)
{
    uint8_t data = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, MPU_REG_PWR, 1, &data, 1, 100);
}

/* ============================================================
 * MPU6050 READ
 * ============================================================ */
void MPU6050_Read(float *ax, float *ay, float *az,
                  float *gx, float *gy, float *gz)
{
    uint8_t raw[6];

    HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, MPU_REG_ACCEL, 1, raw, 6, 100);
    *ax = (int16_t)((int16_t)raw[0] << 8 | raw[1]) / ACCEL_SENS;
    *ay = (int16_t)((int16_t)raw[2] << 8 | raw[3]) / ACCEL_SENS;
    *az = (int16_t)((int16_t)raw[4] << 8 | raw[5]) / ACCEL_SENS;

    HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, MPU_REG_GYRO, 1, raw, 6, 100);
    *gx = (int16_t)((int16_t)raw[0] << 8 | raw[1]) / GYRO_SENS;
    *gy = (int16_t)((int16_t)raw[2] << 8 | raw[3]) / GYRO_SENS;
    *gz = (int16_t)((int16_t)raw[4] << 8 | raw[5]) / GYRO_SENS;
}

/* ============================================================
 * SEND IMU PACKET VIA DMA
 * FIX: use HAL_UART_GetState() instead of dma_busy flag
 *      This reads the actual hardware state — no race condition
 * ============================================================ */
void Send_IMU_Packet(float ax, float ay, float az,
                     float gx, float gy, float gz)
{
    /* Wait until UART is ready — timeout after 50ms to never get stuck */
    uint32_t t = HAL_GetTick();
    while (HAL_UART_GetState(&huart1) != HAL_UART_STATE_READY)
    {
        if (HAL_GetTick() - t > 50)
        {
            /* Force reset UART and DMA if stuck */
            HAL_UART_Abort(&huart1);
            break;
        }
    }

    /* Step 1: Build plain string */
    char plain[96];
    int len = snprintf(plain, sizeof(plain),
        "AX:%+.2f,AY:%+.2f,AZ:%+.2f,GX:%+.1f,GY:%+.1f,GZ:%+.1f",
        ax, ay, az, gx, gy, gz);

    /* Step 2: Checksum */
    uint8_t checksum = 0;
    for (int i = 0; i < len; i++)
        checksum ^= (uint8_t)plain[i];

    /* Step 3: XOR Encrypt */
    uint8_t encrypted[96];
    for (int i = 0; i < len; i++)
        encrypted[i] = (uint8_t)plain[i] ^ XOR_KEY;

    /* Step 4: Build packet  $<encrypted>,CS:XX\n */
    tx_buf[0] = '$';
    memcpy(&tx_buf[1], encrypted, len);
    int total = 1 + len + snprintf((char*)&tx_buf[1 + len], 16, ",CS:%02X\n", checksum);

    /* Step 5: Send via DMA */
    HAL_UART_Transmit_DMA(&huart1, tx_buf, total);

    /* Blink LED on every send */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}

/* ============================================================
 * SYSTEM CLOCK — 72MHz
 * ============================================================ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState        = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState        = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState    = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL      = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

/* ============================================================
 * I2C1 INIT
 * ============================================================ */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

/* ============================================================
 * USART1 INIT
 * ============================================================ */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

/* ============================================================
 * GPIO INIT — PC13 LED
 * ============================================================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ============================================================
 * ERROR HANDLER
 * ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
