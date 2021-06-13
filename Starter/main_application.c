/* Standard includes. */
#include <stdio.h>
#include <conio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH (0)

/* TASK PRIORITIES */
#define	TASK_SERIAL_SEND_PRI		( tskIDLE_PRIORITY + 2 )
#define TASK_SERIAL_REC_PRI			( tskIDLE_PRIORITY + 3 )
#define	SERVICE_TASK_PRI		( tskIDLE_PRIORITY + 1 )

/* TASKS: FORWARD DECLARATIONS */
void led_bar_tsk(void* pvParameters);
void SerialSend_Task(void* pvParameters);
void SerialReceive_Task(void* pvParameters);

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
const char trigger[] = "XYZ";
unsigned volatile t_point;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];
unsigned volatile r_point;

typedef struct _led_s
{
	uint8_t stubac;
	uint8_t vrednost;
} led_s;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

/* GLOBAL OS-HANDLES */
SemaphoreHandle_t LED_INT_BinarySemaphore;
SemaphoreHandle_t TBE_BinarySemaphore;
SemaphoreHandle_t RXC_BinarySemaphore;
QueueHandle_t led_q;
TimerHandle_t tH;

void led_bar_tsk(void* pvParameters) {

	led_s rec_buf;

	for (;;) {
		xQueueReceive(led_q, &rec_buf, portMAX_DELAY);

		uint8_t tmp = 0;

		for (uint8_t i = 0; i < rec_buf.vrednost; i++)
		{
			tmp <<= 1;
			tmp |= 1;
		}

		set_LED_BAR(rec_buf.stubac, tmp);
	}
}

void SerialReceive_Task(void* pvParameters) {

	unsigned char cc;
	led_s send_tmp;

	for (;;) {
		// Cekamo semafor od interapta
		xSemaphoreTake(RXC_BinarySemaphore, portMAX_DELAY);

		get_serial_character(COM_CH, &cc);

		if (cc == 0xef)
		{
			r_point = 0;//prvi karakter, postavlja r_point na nula da moze poceti ubacivati u bafer
		}
		else if (cc == 0xff) {
			send_tmp.stubac = r_buffer[0];
			send_tmp.vrednost = r_buffer[1];

			xQueueSend(led_q, &send_tmp, 0);
		}
		else
		{
			r_buffer[r_point++] = cc;
		}
	}
}


uint32_t prvProcessRXCInterrupt(void) {
	BaseType_t higher_priority_task_woken;

	xSemaphoreGiveFromISR(RXC_BinarySemaphore, &higher_priority_task_woken);

	portYIELD_FROM_ISR(higher_priority_task_woken);
}


/* MAIN - SYSTEM STARTUP POINT */
void main_demo(void)
{
	// Init peripherals
	init_LED_comm();
	//init_7seg_comm();
	//init_serial_uplink(COM_CH); // inicijalizacija serijske TX na kanalu 0
	init_serial_downlink(COM_CH);// inicijalizacija serijske RX na kanalu 0
	/* SERIAL RECEPTION INTERRUPT HANDLER */
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);

	// Semaphores
	RXC_BinarySemaphore = xSemaphoreCreateBinary();

	// Queues
	led_q = xQueueCreate(2, sizeof(led_s));

	// Tasks
	BaseType_t status;
	status = xTaskCreate(
		led_bar_tsk,
		"led task",
		configMINIMAL_STACK_SIZE,
		NULL,
		SERVICE_TASK_PRI,
		NULL
	);
	/* SERIAL RECEIVER TASK */
	xTaskCreate(SerialReceive_Task, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_REC_PRI, NULL);
	r_point = 0;

	vTaskStartScheduler();
	while (1);
}
