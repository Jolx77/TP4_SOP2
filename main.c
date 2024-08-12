/*
 * FreeRTOS V202212.01
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 */

#include "header.h"

/*-----------------------------------------------------------*/

int main(int argc, char *argv[])
{
    /* Configure the clocks, UART, and GPIO. */
    prvSetupHardware();

    /* Create the queues. */
    xTemperatureQueue = xQueueCreate(10, sizeof(int));
    xFilteredQueue = xQueueCreate(10, sizeof(int));
    xNQueue = xQueueCreate(1, sizeof(int));  // Queue for sending N
    xSeedQueue = xQueueCreate(1, sizeof(unsigned int));  // Queue for sending the seed
    unsigned int seed = 91218; // Initialize the seed with an arbitrary value
    xQueueSend(xSeedQueue, &seed, portMAX_DELAY);

    /* Define buffers for the static task. */
    static StackType_t xTopTaskStack[configMINIMAL_STACK_SIZE * 2];
    static StaticTask_t xTopTaskTCB;

    /* Start the tasks. */
    xTaskCreate(vTemperatureSensorTask, "Temps", (configMINIMAL_STACK_SIZE)-48, NULL, mainTEMP_TASK_PRIORITY, NULL);
    xTaskCreate(vFilterTask, "Filter", (configMINIMAL_STACK_SIZE)-42, NULL, mainFILTER_TASK_PRIORITY, NULL);
    xTaskCreate(vGraphTask, "Graph", (configMINIMAL_STACK_SIZE)-2, NULL, mainGRAPH_TASK_PRIORITY, NULL);
    xTaskCreate(vTopTask, "Top", (configMINIMAL_STACK_SIZE*2)-56, NULL, mainTOP_TASK_PRIORITY, NULL);   

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* If the scheduler starts, the following code should never be reached. */
    return 0;
}

static void prvSetupHardware(void)
{
    /* Setup the system clock. */
    SysCtlClockSet(SYSCTL_SYSDIV_10 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_6MHZ);

    configureTimerForRunTimeStats();

    /* Enable the UART for communication. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    
    /* Configure UART pins. */
    GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_DIR_MODE_HW);
    UARTConfigSet(UART0_BASE, mainBAUD_RATE, UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE);

    /* Enable UART interrupts. */
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
    IntEnable(INT_UART0);
    UARTIntRegister(UART0_BASE, vUART_ISR);  // Register the ISR
}


/*-----------------------------------------------------------*/
tBoolean UARTBusy(unsigned long ulBase)
{
    // Check if UART is busy
    return (HWREG(ulBase + UART_O_FR) & UART_FR_BUSY) ? true : false;
}

size_t my_strlen(const char *str) {
    size_t len = 0;
    while (*str++) len++;
    return len;
}

void my_itoa(int num, char *str) {
    int i = 0;
    int isNegative = 0;

    // Handle the case for 0
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    // Handle negative numbers
    if (num < 0) {
        isNegative = 1;
        num = -num;
    }

    // Process each digit
    while (num != 0) {
        int rem = num % 10;
        str[i++] = rem + '0';
        num = num / 10;
    }

    // Add sign for negative numbers
    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void intToGraph(unsigned char graph[2*MAX_WIDTH], int value)
{
    // Shift the graph buffer to the right to make space for the new value
    for(int i = MAX_WIDTH - 1; i > 0; i--)
    {
        graph[i] = graph[i - 1]; // Shift the upper part of the graph
        graph[i + MAX_WIDTH] = graph[i + MAX_WIDTH - 1]; // Shift the lower part of the graph
    }

    // Clear the first column of the graph
    graph[MAX_WIDTH] = 0; // Clear the upper part
    graph[0] = 0; // Clear the lower part

    // Update the graph with the new value
    if(value < 8)
    {
        // If the value is less than 8, update the upper part of the graph
        graph[MAX_WIDTH] = (1 << (7 - value));
    }
    else
    {
        // If the value is 8 or greater, update the lower part of the graph
        graph[0] = (1 << (15 - value));
    }
}

void padString(char *dest, const char *src, int width)
{
    int len = 0;
    // Copy characters from the source string to the destination string
    while (*src && len < width)
    {
        *dest++ = *src++;
        len++;
    }
    // Fill the remaining space with spaces until the specified width is reached
    while (len < width)
    {
        *dest++ = ' ';
        len++;
    }
    // Null-terminate the destination string
    *dest = '\0';
}

unsigned int simple_rand(void)
{
    unsigned int seed;

    // Receive the seed from the queue
    if (xQueueReceive(xSeedQueue, &seed, portMAX_DELAY) == pdPASS)
    {
        // Generate the new seed value
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;

        // Send the new seed back to the queue
        xQueueSend(xSeedQueue, &seed, portMAX_DELAY);
    }

    return seed;
}

void UARTSend(const char *pucBuffer, unsigned long ulCount)
{
    while (ulCount--)
    {
        // Wait until the UART is ready to transmit
        while (UARTBusy(UART0_BASE));
        
        // Send the character
        UARTCharPut(UART0_BASE, *pucBuffer++);
    }
}

void UARTSendString(const char *str)
{
    while (*str)
    {
        // Wait until the UART is ready to transmit
        while (UARTBusy(UART0_BASE));
        
        // Send the character
        UARTCharPut(UART0_BASE, *str++);
    }
}

const char* getTaskStateString(eTaskState state)
{
    switch (state)
    {
        case eRunning:   return "Running";
        case eReady:     return "Ready";
        case eBlocked:   return "Blocked";
        case eSuspended: return "Suspended";
        case eDeleted:   return "Deleted";
        default:         return "Unknown";
    }
}

static void vTopTask(void *pvParameters)
{
    char buffer[128];
    char temp[32];
    UBaseType_t uxArraySize, x;
    TaskStatus_t pxTaskStatusArray[configMAX_PRIORITIES]; 
    uint32_t ulTotalRunTime;
    size_t xFreeHeapSize;
    TickType_t xLastWakeTime;

    #if WATERMARK_MIN == 1
    TaskHistory_t xTaskHistoryArray[configMAX_PRIORITIES];
    #endif

    uxArraySize = uxTaskGetNumberOfTasks();
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // Initialize the task history array if WATERMARK_MIN is defined as 1
        #if WATERMARK_MIN == 1
        for (x = 0; x < configMAX_PRIORITIES; x++) {
            xTaskHistoryArray[x].xTaskHandle = NULL;
            xTaskHistoryArray[x].usLowestStack = 0xFFFF; // Initial high value
        }
        #endif

        // Get the state of all tasks
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

        // Send header via UART depending on the value of WATERMARK_MIN
        #if WATERMARK_MIN == 1
        UARTSendString("Task Name      State        Priority   Stack(Words)  Stack-Min(words)  Task Number      TimeOfCpu(ms)\r\n");
        #else
        UARTSendString("Task Name      State        Priority   Stack(Words)  Task Number  TimeOfCpu(ms)\r\n");
        #endif

        // Iterate through all tasks and send their statistics via UART
        for (x = 0; x < uxArraySize; x++)
        {
            // Format the task name
            padString(buffer, pxTaskStatusArray[x].pcTaskName, 15);

            // Format the task state
            padString(buffer + 15, getTaskStateString(pxTaskStatusArray[x].eCurrentState), 13);

            // Format the task priority
            my_itoa(pxTaskStatusArray[x].uxCurrentPriority, temp);
            padString(buffer + 28, temp, 12);

            // Format the task stack
            my_itoa(pxTaskStatusArray[x].usStackHighWaterMark, temp);
            padString(buffer + 40, temp, 14);

            // Update and format the lowest historical stack value if WATERMARK_MIN is defined as 1
            #if WATERMARK_MIN == 1
            for (int i = 0; i < configMAX_PRIORITIES; i++) {
                if (xTaskHistoryArray[i].xTaskHandle == pxTaskStatusArray[x].xHandle || xTaskHistoryArray[i].xTaskHandle == NULL) {
                    if (xTaskHistoryArray[i].xTaskHandle == NULL) {
                        xTaskHistoryArray[i].xTaskHandle = pxTaskStatusArray[x].xHandle;
                    }
                    if (pxTaskStatusArray[x].usStackHighWaterMark < xTaskHistoryArray[i].usLowestStack) {
                        xTaskHistoryArray[i].usLowestStack = pxTaskStatusArray[x].usStackHighWaterMark;
                    }
                    my_itoa(xTaskHistoryArray[i].usLowestStack, temp);
                    padString(buffer + 54, temp, 14);
                    break;
                }
            }
            #endif

            // Format the task number
            #if WATERMARK_MIN == 1
            my_itoa(pxTaskStatusArray[x].xTaskNumber, temp);
            padString(buffer + 68, temp, 16);
            #else
            my_itoa(pxTaskStatusArray[x].xTaskNumber, temp);
            padString(buffer + 54, temp, 12);
            #endif

            // Format the task CPU time
            #if WATERMARK_MIN == 1
            my_itoa(pxTaskStatusArray[x].ulRunTimeCounter, temp);
            padString(buffer + 84, temp, 17);
            #else
            my_itoa(pxTaskStatusArray[x].ulRunTimeCounter, temp);
            padString(buffer + 66, temp, 15);
            #endif

            // End the string with "\r\n"
            #if WATERMARK_MIN == 1
            buffer[101] = '\r';
            buffer[102] = '\n';
            buffer[103] = '\0';
            #else
            buffer[81] = '\r';
            buffer[82] = '\n';
            buffer[83] = '\0';
            #endif

            // Send the formatted line via UART
            UARTSendString(buffer);
        }

        // Send the total run time
        UARTSendString("Total Run Time: ");
        my_itoa(ulTotalRunTime, temp);
        UARTSendString(temp);
        UARTSendString(" ms\r\n");

        // Get and send the free heap size
        xFreeHeapSize = xPortGetFreeHeapSize();
        UARTSendString("Free heap: ");
        my_itoa(xFreeHeapSize, temp);
        UARTSendString(temp);
        UARTSendString(" bytes\r\n");

        // Wait before the next update
        vTaskDelayUntil(&xLastWakeTime, mainTOP_TASK_DELAY);
    }
}

static void vTemperatureSensorTask(void *pvParameters)
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

void vUART_ISR(void)
{
    uint32_t ulStatus;
    char c;
    int newN;

    /* Get the interrupt status. */
    ulStatus = UARTIntStatus(UART0_BASE, true);

    /* Clear the asserted interrupts. */
    UARTIntClear(UART0_BASE, ulStatus);

    /* Check if it's a receive interrupt. */
    if (ulStatus & (UART_INT_RX | UART_INT_RT))
    {
        /* Get the character from UART. */
        c = UARTCharGet(UART0_BASE);

        /* Check if the character is a valid number between '1' and '9'. */
        if (c >= '1' && c <= '9')
        {
            newN = c - '0'; // Convert ASCII to integer

            /* Send the new value of N to the filter task via the queue. */
            xQueueSendFromISR(xNQueue, &newN, NULL);

            UARTCharPut(UART0_BASE, c);

            /* Exit the ISR to free the UART. */
            return;
        }
        else
        {
            UARTCharPut(UART0_BASE, 'E'); // Echo 'E' for error
        }
    }
}

static void vFilterTask(void *pvParameters)
{
    int N = 3; // Initial value of N
    int buffer[MAX_N] = {0}; // Static buffer of fixed size
    int sum = 0;
    int receivedN = 3;
    int temperature = 0;
    int count = 0; // Variable that allows calculating the average even when the number of samples is less than N.

    for (;;)
    {
        /* Check if a new value for N has been received */
        if (xQueueReceive(xNQueue, &receivedN, 0) == pdPASS)
        {
            /* Adjust the value of N */
            N = receivedN;
        }

        if (xQueueReceive(xTemperatureQueue, &temperature, portMAX_DELAY) == pdPASS)
        {
            for (int i = MAX_N - 1; i > 0; i--)
            {
                buffer[i] = buffer[i - 1];
            }
            buffer[0] = temperature;
            if (count < MAX_N) count++;
            sum = 0; // Reset sum before adding the values from the buffer
            int valuesToAverage = (count < N) ? count : N;
            for (int i = 0; i < valuesToAverage; i++)
            {
                sum += buffer[i];
            }
            int filteredValue = sum / valuesToAverage;
            xQueueSend(xFilteredQueue, &filteredValue, portMAX_DELAY);
        }
    }
}

static void vGraphTask(void *pvParameters)
{
    int filteredValue;
    unsigned char graph[2 * MAX_WIDTH] = {0}; // Buffer to store the graph on the two lines of the screen

    // Initialize the LCD screen
    OSRAMInit(false);
    OSRAMClear();

    for (;;)
    {
        /* Wait for a filtered value to arrive. */
        xQueueReceive(xFilteredQueue, &filteredValue, portMAX_DELAY);
    
        /* Scale the filtered value to the height of the graph. */
        int scaledValue = (filteredValue * (MAX_HEIGHT)) / 99;        
        /* Call the intToGraph function to update the graph buffer. */
        intToGraph(graph, scaledValue);
        /* Clear the LCD screen before drawing the graph */
        OSRAMClear();
        /* Draw the graph on the LCD screen. */
        OSRAMImageDraw(graph, 0, 0, MAX_WIDTH, 2);
    }
}

void configureTimerForRunTimeStats(void)
{
    // Enable the timer peripheral
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_32_BIT_TIMER);

    // Configure the timer to generate interrupts every 1 ms
    uint32_t ui32Period = (SysCtlClockGet()/1000);
    TimerLoadSet(TIMER0_BASE, TIMER_A, ui32Period - 1);

    // Register the interrupt handler
    TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0IntHandler);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Enable the timer
    TimerEnable(TIMER0_BASE, TIMER_A);
}

void Timer0IntHandler(void)
{
    // Clear the timer interrupt
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Increment the runtime counter
    ulHighFrequencyTimerTicks++;
}

uint32_t getRunTimeCounterValue(void)
{
    // Return the current runtime counter value
    return ulHighFrequencyTimerTicks;
}
