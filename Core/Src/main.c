/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "fatfs.h"
#include "memorymap.h"
#include "sdmmc.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "sdcall.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
FATFS aaa;
static SD sd;
TrialData data;

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
void filesInit(FIL *_shortWriteFile, FIL *_longWriteFile);
void SDCardUpdateFastFile(FIL *_shortWriteFile, TrialData _data);
void SDCardUpdateSlowFile(FIL *_slowWriteFIle, TrialData _data);
void changeLine(FIL *_fil);
void writeStrField(FIL *_fil, char *val);
void writeIntField(FIL *_fil, int val);
void writeFloatField(FIL *_fil, float val);
void writeLongFloatField(FIL *_fil, float val);
void writeLongField(FIL *_fil, long val);
void SDCardInit();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
FRESULT res;
static FIL slowFile;
static FIL fastFile;
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
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
  SDCardInit();
  filesInit(&fastFile, &slowFile);
  HAL_Delay(100);

  FRESULT r;
  UINT bw;
  FIL testFile;

  r = f_mount(&aaa, "", 1);

  r = f_open(&testFile, "TEST.TXT", FA_WRITE | FA_CREATE_ALWAYS);

  r = f_write(&testFile, "KALA PAME DIKE MOU \r\n", 7, &bw);

  r = f_close(&testFile);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if((uwTick - sd.slowFile.writeTime) > 100 - 1){
		  sd.slowFile.writeTime = uwTick;
		  SDCardUpdateSlowFile(&slowFile, data);
//		  HAL_GPIO_WritePin(SD_OK_Indicator_GPIO_Port, SD_OK_Indicator_Pin, GPIO_PIN_SET);
		  data.counterFlag++;
	  }

	  if((uwTick - sd.fastFile.writeTime) > 20 - 1){
		  sd.fastFile.writeTime = uwTick;
		  SDCardUpdateFastFile(&fastFile, data);
//		  HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, GPIO_PIN_SET);
	  }
	  if((uwTick - sd.syncTime) > 1000 - 1){
		  sd.syncTime = uwTick;
		  FRESULT fresFast = f_sync(&fastFile);
		  FRESULT fresSlow = f_sync(&slowFile);
		  if(fresFast != FR_OK || fresSlow != FR_OK)  {
			  HAL_GPIO_WritePin(SD_OK_Indicator_GPIO_Port, SD_OK_Indicator_Pin, GPIO_PIN_RESET);
			  HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, GPIO_PIN_RESET);
			  HAL_GPIO_WritePin(SD_ERROR_LED_GPIO_Port, SD_ERROR_LED_Pin, GPIO_PIN_SET);
//			  Error_Handler();
			  data.errorFlag++;
			  f_close(&fastFile);
			  f_mount(NULL, "", 0);
			  f_mount(&aaa, "", 1);
			  res = f_open(&fastFile, sd.fastFile.fileName, FA_WRITE | FA_OPEN_APPEND);
			  res = f_open(&slowFile, sd.slowFile.fileName, FA_WRITE | FA_OPEN_APPEND);
		  }
	  }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 34;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 3072;
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
extern char SDPath[4];   /* SD logical drive path */
void SDCardInit(){
	sd.fastFile.fileCounter = 0;
	sprintf(sd.fastFile.fileName, "%s000.CSV", FILE_FAST_BASE_NAME);
	sd.fastFile.nameSize = sizeof(FILE_FAST_BASE_NAME) - 1;
	sd.slowFile.nameSize =  sizeof(FILE_SLOW_BASE_NAME) - 1;

	sd.slowFile.fileCounter = 0;
	sprintf(sd.slowFile.fileName, "%s000.CSV", FILE_SLOW_BASE_NAME);


	HAL_Delay(100);
	res = f_mount(&aaa, (TCHAR const*)SDPath, 1);
	while(res != FR_OK){
		HAL_Delay(500);
		res = f_mount(&aaa, (TCHAR const*)SDPath, 1);
		HAL_GPIO_WritePin(SD_ERROR_LED_GPIO_Port, SD_ERROR_LED_Pin, GPIO_PIN_SET);
	}


	while(f_stat(sd.fastFile.fileName, &sd.fileInfo) != FR_NO_FILE){
		if(sd.fastFile.fileName[sd.fastFile.nameSize + 2] != '9') {
			sd.fastFile.fileName[sd.fastFile.nameSize + 2]++;
			sd.fastFile.fileCounter++;
		}
		else if(sd.fastFile.fileName[sd.fastFile.nameSize + 1] != '9') {
			sd.fastFile.fileName[sd.fastFile.nameSize + 2] = '0';
			sd.fastFile.fileName[sd.fastFile.nameSize + 1]++;
			sd.fastFile.fileCounter++;
		}
		else if(sd.fastFile.fileName[sd.fastFile.nameSize] != '9'){
			sd.fastFile.fileName[sd.fastFile.nameSize + 2] = '0';
			sd.fastFile.fileName[sd.fastFile.nameSize + 1] = '0';
			sd.fastFile.fileName[sd.fastFile.nameSize]++;
			sd.fastFile.fileCounter++;
		}
		else Error_Handler();
	}

	while(f_stat(sd.slowFile.fileName, &sd.fileInfo) != FR_NO_FILE){
		if(sd.slowFile.fileName[sd.slowFile.nameSize + 2] != '9') {
			sd.slowFile.fileName[sd.slowFile.nameSize + 2]++;
			sd.slowFile.fileCounter++;
		}
		else if(sd.slowFile.fileName[sd.slowFile.nameSize + 1] != '9') {
			sd.slowFile.fileName[sd.slowFile.nameSize + 2] = '0';
			sd.slowFile.fileName[sd.slowFile.nameSize + 1]++;
			sd.slowFile.fileCounter++;
		}
		else if(sd.slowFile.fileName[sd.slowFile.nameSize] != '9'){
			sd.slowFile.fileName[sd.slowFile.nameSize + 2] = '0';
			sd.slowFile.fileName[sd.slowFile.nameSize + 1] = '0';
			sd.slowFile.fileName[sd.slowFile.nameSize]++;
			sd.slowFile.fileCounter++;
		}
		else break;
	}

	if(f_open(&fastFile, sd.fastFile.fileName, FA_WRITE | FA_READ | FA_CREATE_ALWAYS) != FR_OK) Error_Handler();
	HAL_Delay(10);

	if(f_open(&slowFile, sd.slowFile.fileName, FA_WRITE | FA_READ | FA_CREATE_ALWAYS) != FR_OK) Error_Handler();
	HAL_Delay(10);

	sd.fastFile.writeTime = uwTick;
	sd.slowFile.writeTime = uwTick;
	sd.syncTime = uwTick;
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
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
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
