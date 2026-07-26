/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dcmi.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdarg.h> // va_list 사용을 위해 추가
#include <stdio.h>  // vsprintf 사용을 위해 추가
#include "ov2640.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define OV2640_I2C_ADDR (0x30 << 1)  // 8비트 기준 Write 주소

// 해상도 설정 (QQVGA 160x120 예시)
#define FRAME_W       160
#define FRAME_H       120
#define FRAME_PIXELS   (FRAME_W * FRAME_H)
#define FRAME_BYTES    (FRAME_PIXELS * 2)     // 38400 bytes
#define FRAME_WORDS    (FRAME_BYTES / 4)      // DCMI DMA length in 32-bit words = 9600

// H743 내부 SRAM용 32바이트 정렬 프레임 버퍼
ALIGN_32BYTES(uint16_t frame_buffer[FRAME_PIXELS]);// 프레임 완료 플래그

#define FEATURE_W     96
#define FEATURE_H     96
#define FEATURE_SIZE  (FEATURE_W * FEATURE_H)

extern const float test_features1[];
extern const float test_features2[];


volatile uint8_t frame_ready = 0;
uint8_t current_mode = 0; // 💡 기본값을 RGB 모드로 설정 (원하는 모드로 변경 가능)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void Send_FeaturesToPC(void) {
// 시작 마커 전송 (기존과 동일한 규격 유지)
	uint8_t start_msg[] = "---START---\r\n";
	HAL_UART_Transmit(&huart1, start_msg, sizeof(start_msg) - 1, HAL_MAX_DELAY);

// float 배열 데이터를 바이트 단위로 캐스팅하여 전송 (36100 bytes)
	HAL_UART_Transmit(&huart1, (uint8_t*) test_features1,
	FEATURE_SIZE * sizeof(uint32_t), HAL_MAX_DELAY);
}


void UART_Printf(const char *format, ...) {
	char loc_buf[256];
	va_list args;

	va_start(args, format);
// 가변 인자를 포맷팅하여 버퍼에 문자열로 저장
	int len = vsnprintf(loc_buf, sizeof(loc_buf), format, args);
	va_end(args);

	if (len > 0)
		HAL_UART_Transmit(&huart1, (uint8_t*) loc_buf, (uint16_t) len,
		HAL_MAX_DELAY);

}

// --------------------------------------------------
// 카메라 DMA 캡처 시작 함수
// --------------------------------------------------
static void Camera_StartCapture(void) {
	HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t) frame_buffer,
			FRAME_WORDS);
}

// --------------------------------------------------
// 카메라 모드 설정 함수
// --------------------------------------------------
void Camera_SetMode(uint8_t is_rgb) {
	current_mode = is_rgb;

	if (current_mode == 1) {
		// RGB 컬러 모드 (Normal)
		ov2640_Config(0x60, CAMERA_BLACK_WHITE, CAMERA_BLACK_WHITE_NORMAL, 0);
	} else {
		// Grayscale 흑백 모드 (BW)
		ov2640_Config(0x60, CAMERA_BLACK_WHITE, CAMERA_BLACK_WHITE_BW, 0);
	}
}

// --------------------------------------------------
// 파이썬 뷰어로 1프레임 데이터 전송 함수
// --------------------------------------------------
static void Camera_SendFrameToPC(void) {
	// 파이썬 수신기가 모드를 자동 구분하도록 Header 마커 전송
	if (current_mode == 1) {
		static const uint8_t start_rgb[] = "---START_RGB---\r\n";
		HAL_UART_Transmit(&huart1, (uint8_t*) start_rgb, sizeof(start_rgb) - 1,
				HAL_MAX_DELAY);
	} else {
		static const uint8_t start_gray[] = "---START_GRAY---\r\n";
		HAL_UART_Transmit(&huart1, (uint8_t*) start_gray,
				sizeof(start_gray) - 1, HAL_MAX_DELAY);
	}

	// Raw 이미지 데이터 전송 (160 * 120 * 2 = 38,400 bytes)
	HAL_UART_Transmit(&huart1, (uint8_t*) frame_buffer, FRAME_BYTES,
			HAL_MAX_DELAY);

	// 플래그 초기화 후 다음 프레임 수신 재개
	frame_ready = 0;
	Camera_StartCapture();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_DCMI_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

//  하드웨어 리셋
	HAL_GPIO_WritePin(CAM_PWDN_GPIO_Port, CAM_PWDN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(CAM_RET_GPIO_Port, CAM_RET_Pin, GPIO_PIN_RESET);
	HAL_Delay(30);
	HAL_GPIO_WritePin(CAM_RET_GPIO_Port, CAM_RET_Pin, GPIO_PIN_SET);
	HAL_Delay(20);

//	BSP 함수로 ID 읽기 테스트
	uint16_t pid = ov2640_ReadID(0x60);
	if (pid != OV2640_ID) { // 0x26
		UART_Printf("OV2640 Check Failed! Read ID = 0x%02X\r\n", pid);
		while (1)
			;
	}
	UART_Printf("OV2640 Connected! ID = 0x%02X\r\n", pid);

//	BSP 함수로 카메라 레지스터 및 해상도 초기화 (QQVGA 160x120)
	ov2640_Init(OV2640_I2C_ADDR, CAMERA_R160x120);
//	초기 모드 적용 (1: RGB 모드, 0: Grayscale 모드)
	Camera_SetMode(current_mode);
//	캡처 시작
	Camera_StartCapture();
	UART_Printf("Start Capture...\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		if (frame_ready) {
			Camera_SendFrameToPC();
		}

		if (hdcmi.State == HAL_DCMI_STATE_ERROR) {
			HAL_DCMI_Stop(&hdcmi);
			hdcmi.State = HAL_DCMI_STATE_READY;
			frame_ready = 0;
			Camera_StartCapture();
		}

//		테스트 이미지 송신
//		Send_FeaturesToPC();
//		HAL_Delay(2000);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_2);
}

/* USER CODE BEGIN 4 */

// DCMI 프레임 수신 완료 콜백 함수
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi) {
	frame_ready = 1;
}

// 에러 발생 시 강제 복구 콜백 함수
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi) {
	hdcmi->State = HAL_DCMI_STATE_READY;
	frame_ready = 0;
}

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{

  /* Disables the MPU */
  HAL_MPU_Disable();

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
/* User can add his own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
