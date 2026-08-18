#include "main.h"
#include "can.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <time.h>
#include <stdio.h>

#include <string.h>
#include "can_manager.hpp"
#include "pinecan_handlers.h"
// NEW CHANGE: new lighting pipeline entry points (led_init/Select_Pattern/Generate_Leds/Push_Leds)
#include "new_lighting_controller.hpp"
// RAM2 trace buffer. All DBG_* macros compile to nothing unless LIGHTING_TRACE is
// defined, so the STM32CubeIDE build is unaffected. See Core/Inc/debug_trace.h.
#include "debug_trace.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// #define ROTATE_LED
// #define CYCLE_ONE_LED_ON
// #define CONSTANT_COLOR

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim6;
static uint32_t node_id;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  Return a unique ID made out of the 96-bit STM32 UID
 * @param  id an array of size 16 to fill with the unique ID
 * @retval None
 */
void getUniqueID(uint8_t id[16])
{
	uint32_t HALUniqueIDs[4];
	// Make Unique ID out of the 96-bit STM32 UID
	memset(id, 0, 16);
	HALUniqueIDs[0] = HAL_GetUIDw0();
	HALUniqueIDs[1] = HAL_GetUIDw1();
	HALUniqueIDs[2] = HAL_GetUIDw2();
	HALUniqueIDs[3] = HAL_GetUIDw1(); // repeating UIDw1 for this, no specific reason I chose this..
	memcpy(id, HALUniqueIDs, 16);
}

void initializeNodeId()
{
	uint8_t buffer[16];
	getUniqueID(buffer);
	uint32_t *parts = (uint32_t *)buffer;
	node_id = parts[0] ^ parts[1] ^ parts[2];
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

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */
	// Claim the trace buffer before anything else can fault.
	DBG_INIT();
	DBG_EV(DBG_EV_BOOT, DBG_TRACE->boot_count, 0);
	DBG_EV(DBG_EV_HAL_INIT_DONE, 0, 0);
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	DBG_EV(DBG_EV_CLOCK_CONFIG_DONE, SystemCoreClock, 0);
	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	DBG_EV(DBG_EV_GPIO_INIT_DONE, 0, 0);
	MX_DMA_Init();
	DBG_EV(DBG_EV_DMA_INIT_DONE, 0, 0);
	MX_CAN1_Init();
	DBG_EV(DBG_EV_CAN1_INIT_DONE, 0, 0);
	MX_TIM1_Init();
	DBG_EV(DBG_EV_TIM1_INIT_DONE, 0, 0);
	MX_TIM6_Init();
	DBG_EV(DBG_EV_TIM6_INIT_DONE, 0, 0);
	MX_TIM7_Init();
	DBG_EV(DBG_EV_TIM7_INIT_DONE, 0, 0);
	MX_TIM2_Init();
	DBG_EV(DBG_EV_TIM2_INIT_DONE, 0, 0);
	// here

	/* USER CODE BEGIN 2 */

	{
		const HAL_StatusTypeDef s6 = HAL_TIM_Base_Start_IT(&htim6);
		DBG_EV(DBG_EV_TIM6_START, s6, 0);
		(void) s6;
		// htim2 is the WS28xx PWM carrier (period 96 @ 48 MHz). Starting it with _IT
		// enables a ~495 kHz update ISR that HAL_TIM_IRQHandler cannot complete inside
		// one period at -O0, which starves the CPU before main() reaches led_init().
		// The LED DMA is fed by the TIM2 CC1 DMA request (CC1DE), not the update
		// interrupt, so UIE is not needed here. HAL_TIM_PWM_Start_DMA() enables the
		// counter itself. See .kiro/specs/efs-can-lighting-build-config/debugging.md
		const HAL_StatusTypeDef s2 = HAL_TIM_Base_Start(&htim2);
		DBG_EV(DBG_EV_TIM2_START, s2, 0);
		(void) s2;
	}

	initializeNodeId();
	DBG_EV(DBG_EV_NODE_ID_DONE, node_id, 0);

	DBG_EV(DBG_EV_INITCAN_ENTER, 0, 0);
	{
		// Capture the status before branching, so a failure is still visible in the
		// trace even though Error_Handler() never returns.
		const PineCAN_Status canStatus = initCAN();
		DBG_EV(DBG_EV_INITCAN_RESULT, canStatus, 0);

		if (canStatus != PINECAN_OK)
		{
			Error_Handler();
		}
	}

	// initialise the LED PWM/DMA output (bank/DMA buffers, HAL_TIM_PWM_Start_DMA)
	// and load the GROUND pattern as the boot default, so the board shows a
	// valid pattern before the first CAN message arrives.
	DBG_EV(DBG_EV_LED_INIT_ENTER, 0, 0);
	led_init();
	DBG_EV(DBG_EV_LED_INIT_DONE, 0, 0);

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */

	uint64_t raw_vehicle_state = 0;
	bool new_data = false;
	bool stay_in_loop = true;
	uint8_t flight_state = 0;

	DBG_EV(DBG_EV_MAIN_LOOP_ENTER, 0, 0);

	while (1)
	{
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */

		while (stay_in_loop)
		{
		
			static uint32_t lastLedTick = 0;
			static uint32_t tick = 0;
			DBG_CNT(DBG_CNT_INNER_LOOP);

			// Proof-of-life: shows whether the main loop progresses at all, and how
			// many iterations it manages per 500 ms of HAL_GetTick().
			{
				static uint32_t lastBeatTick = 0;
				static uint32_t beatIters = 0;
				beatIters++;
				if (HAL_GetTick() - lastBeatTick >= 500)
				{
					lastBeatTick = HAL_GetTick();
					DBG_EV(DBG_EV_HEARTBEAT, lastBeatTick, beatIters);
					beatIters = 0;
				}
			}

			if (HAL_GetTick() - lastLedTick >= 20)
			{
				lastLedTick = HAL_GetTick();
				if (tick == 0) { DBG_EV(DBG_EV_FIRST_GENERATE, lastLedTick, 0); }
				Generate_Leds(tick); // advance animations, expand active zones into colour_buffer
				if (tick == 0) { DBG_EV(DBG_EV_FIRST_PUSH, lastLedTick, 0); }
				Push_Leds();		 // bit-encode colour_buffer into the DMA-streamed bank buffer
				tick++;
			}

			// Service PineCAN housekeeping (1ms tick-gated pinecan1ms call)
			DBG_CNT(DBG_CNT_SERVICE);
			CANManager::service();

			// Re-read the cache each inner iteration so new CAN data is visible
			raw_vehicle_state = CANManager::getLatestVehicleState();

			// Step 2: check if data changed since last call
			new_data = CANManager::vehicleStateChanged(raw_vehicle_state);

			if (new_data)
			{
				DBG_EV(DBG_EV_STATE_CHANGED,
					   (uint32_t)(raw_vehicle_state & 0xFFFFFFFFu),
					   (uint32_t)(raw_vehicle_state >> 32));
				stay_in_loop = false;
			}
		}

		// read cache and decode — gives us the current flight state
		flight_state = interpretVehicleState(raw_vehicle_state);

		// a = flight_state; 0xFF is UNRECOGNIZED_STATE, which Select_Pattern ignores
		DBG_EV(DBG_EV_SELECT_PATTERN, flight_state, (uint32_t)(raw_vehicle_state & 0xFFFFFFFFu));
		Select_Pattern(flight_state);

		new_data = false;
		stay_in_loop = true;
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

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}
/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// canService() (Core/Src/can.c) self-gates on HAL_GetTick() now, so no
	// flag needs to be set here for PineCAN servicing.
	(void)htim;
}

/* USER CODE END 4 */

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

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
