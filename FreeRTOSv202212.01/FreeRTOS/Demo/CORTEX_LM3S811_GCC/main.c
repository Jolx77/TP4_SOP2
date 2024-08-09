/*
 * FreeRTOS V202212.01
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 */

/* Environment includes. */
#include "DriverLib.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "osram96x16.h"  


/* Configuration for the hardware and tasks. */
#define mainBAUD_RATE                ( 19200 )
#define MAX_HEIGHT 16  // Altura máxima del gráfico en píxeles
#define MAX_WIDTH 96   // Ancho máximo del gráfico en píxeles

/* Task priorities. */
#define mainCHECK_TASK_PRIORITY      ( tskIDLE_PRIORITY + 3 )
#define mainGRAPH_TASK_PRIORITY      ( tskIDLE_PRIORITY + 1 )
#define mainSENSOR_TASK_PRIORITY     ( tskIDLE_PRIORITY + 2 )

/* Hardware setup function. */
static void prvSetupHardware( void );

/* Task declarations. */
static void vTemperatureSensorTask(void *pvParameters);
static void vFilterTask(void *pvParameters);
static void vGraphTask(void *pvParameters);
static void vUARTTask(void *pvParameters);


/* Queue handles. */
QueueHandle_t xFilteredQueue;
QueueHandle_t xTemperatureQueue;
QueueHandle_t xNQueue;

/*-----------------------------------------------------------*/

int main(void)
{
    /* Configure the clocks, UART, and GPIO. */
    prvSetupHardware();

    /* Create the queues. */
    xTemperatureQueue = xQueueCreate(10, sizeof(int));
    xFilteredQueue = xQueueCreate(10, sizeof(int));
    xNQueue = xQueueCreate(1, sizeof(int));  // Queue for sending N

    /* Start the tasks. */
    xTaskCreate(vTemperatureSensorTask, "Temperature", configMINIMAL_STACK_SIZE, NULL, mainSENSOR_TASK_PRIORITY, NULL);
    xTaskCreate(vFilterTask, "Filter", configMINIMAL_STACK_SIZE, NULL, mainSENSOR_TASK_PRIORITY, NULL);
    xTaskCreate(vGraphTask, "Graph", configMINIMAL_STACK_SIZE, NULL, mainGRAPH_TASK_PRIORITY, NULL);
    xTaskCreate(vUARTTask, "UART", configMINIMAL_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, NULL);

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* If the scheduler starts, the following code should never be reached. */
    return 0;
}
/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
    /* Setup the system clock. */
    SysCtlClockSet(SYSCTL_SYSDIV_10 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_6MHZ);

    /* Enable the UART for communication. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_DIR_MODE_HW);
    UARTConfigSet(UART0_BASE, mainBAUD_RATE, UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE);
}

/*-----------------------------------------------------------*/

/* Pseudo-random number generator function for simulating the temperature sensor. */
static unsigned int seed = 123456789;

unsigned int simple_rand(void)
{
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed;
}

/* Temperature sensor task to generate random temperature values. */
void vTemperatureSensorTask(void *pvParameters)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10Hz frequency
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for(;;)
    {
        int temperature = simple_rand() % 100;  // Simulate temperature between 0 and 99 degrees
        xQueueSend(xTemperatureQueue, &temperature, portMAX_DELAY);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void vUARTTask(void *pvParameters)
{
    char c;
    int newN;

    for (;;)
    {
        /* Wait for a character from UART. */
        if (UARTCharsAvail(UART0_BASE))
        {
            c = UARTCharGet(UART0_BASE);
            
            /* Check if the character is a valid number between '1' and '9'. */
            if (c >= '1' && c <= '9')
            {
                newN = c - '0'; // Convert ASCII to integer
            }
            else if (c == '1' && UARTCharsAvail(UART0_BASE))
            {
                char nextChar = UARTCharGet(UART0_BASE);
                if (nextChar == '0')
                {
                    newN = 10;
                }
            }
            
            /* Send the new value of N to the filter task via the queue */
            xQueueSend(xNQueue, &newN, portMAX_DELAY);

            /* Optional: Echo the received character back to UART */
            UARTCharPut(UART0_BASE, c);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to avoid busy-waiting
    }
}

/* Filtering task to apply a low-pass filter on the temperature values. */
void vFilterTask(void *pvParameters)
{
    int N = 3; // Initial value of N
    int *buffer = pvPortMalloc(N * sizeof(int)); // Dynamically allocate the buffer based on N
    int index = 0;
    int sum = 0;
    int count = 0;

    for (;;)
    {
        /* Check if a new value for N has been received */
        int receivedN;
        if (xQueueReceive(xNQueue, &receivedN, 0) == pdPASS)
        {
            /* Adjust the buffer size based on the new value of N */
            N = receivedN;
            vPortFree(buffer);
            buffer = pvPortMalloc(N * sizeof(int));
            index = 0;
            sum = 0;
            count = 0;
        }

        int temperature;
        if (xQueueReceive(xTemperatureQueue, &temperature, portMAX_DELAY) == pdPASS)
        {
            sum -= buffer[index];
            buffer[index] = temperature;
            sum += temperature;
            index = (index + 1) % N;
            if (count < N) count++;

            int filteredValue = sum / count;
            xQueueSend(xFilteredQueue, &filteredValue, portMAX_DELAY);
        }
    }
}

// Función para convertir un entero a una cadena
void intToStr(int num, char* str, int len) {
    int i = len - 1;
    str[i--] = '\0';
    if (num == 0) {
        str[i] = '0';
        return;
    }
    while (num != 0 && i >= 0) {
        str[i--] = (num % 10) + '0';
        num /= 10;
    }
    while (i >= 0) {
        str[i--] = ' ';
    }
}

void intToChar(unsigned char graph[2*MAX_WIDTH],int value)
{
	for(int i = MAX_WIDTH - 1; i > 0;i--)
	{
		graph[i] = graph[i-1];
		graph[i + MAX_WIDTH] = graph[i + MAX_WIDTH - 1];
	}

	graph[MAX_WIDTH] = 0;
	graph[0] = 0;

	if(value < 8)
	{
		graph[MAX_WIDTH] = (1 << (7 - value));
	}
	else
	{
		graph[0] = (1 << (15 - value));
	}
}

static void vGraphTask(void *pvParameters)
{
    int filteredValue;
    unsigned char graph[2 * MAX_WIDTH] = {0}; // Buffer para almacenar el gráfico en las dos líneas de la pantalla

    // Inicializar la pantalla LCD
    OSRAMInit(false);
    OSRAMClear();

    for (;;)
    {
        /* Esperar a que llegue un valor filtrado. */
        xQueueReceive(xFilteredQueue, &filteredValue, portMAX_DELAY);
    
        /* Escalar el valor filtrado a la altura del gráfico. */
        int scaledValue = (filteredValue * (MAX_HEIGHT)) / 99;
        /* Escribir el valor filtrado en la pantalla LCD. */
        
        /* Llamar a la función intToChar para actualizar el buffer del gráfico. */
        intToChar(graph, scaledValue);
        // Limpiar la pantalla LCD antes de dibujar el gráfico
        OSRAMClear();
        /* Dibujar el gráfico en la pantalla LCD. */
        OSRAMImageDraw(graph, 0, 0, MAX_WIDTH, 2);
    }
}


//static void vGraphTask(void *pvParameters)
//{
//    int filteredValue;
//    unsigned portBASE_TYPE uxLine = 0, uxRow = 0;
//
//    // Inicializar la pantalla LCD
//    OSRAMInit(false);
//    OSRAMClear();
//
//    for(;;)
//    {
//        /* Esperar a que llegue un valor filtrado. */
//        xQueueReceive(xFilteredQueue, &filteredValue, portMAX_DELAY);
//
//        /* Convertir el valor filtrado a una cadena. */
//        char strValue[10];
//        intToStr(filteredValue, strValue, sizeof(strValue));
//
//        /* Escribir el valor filtrado en la pantalla LCD. */
//        OSRAMClear();
//        OSRAMStringDraw(strValue, uxLine & 0x3f, uxRow & 0x01);
//    }
//}
//
//
//