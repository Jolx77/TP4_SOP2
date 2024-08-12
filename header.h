/*
 * cortex_LM3S811.h
 *
 * Header file for the cortex_LM3S811 program.
 *
 * FreeRTOS V202212.01
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 */

#ifndef CORTEX_LM3S811_H
#define CORTEX_LM3S811_H

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
#define MAX_HEIGHT 16  // Maximum graph height in pixels
#define MAX_WIDTH 96   // Maximum graph width in pixels
#define WATERMARK_MIN 1 // Shows the lowest historical free stack space value
#define MAX_N 9 // Max number of N
#define MIN_N 1 // Min number of N
 
/* Task priorities. */
#define mainCHECK_TASK_PRIORITY      ( tskIDLE_PRIORITY + 5 )
#define mainGRAPH_TASK_PRIORITY      ( tskIDLE_PRIORITY + 2 )
#define mainTEMP_TASK_PRIORITY       ( tskIDLE_PRIORITY + 4 )
#define mainFILTER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 3 )
#define mainTOP_TASK_PRIORITY        ( tskIDLE_PRIORITY + 1 )
#define mainTOP_TASK_DELAY           ( pdMS_TO_TICKS(5000) ) // 5 seconds


/* Queue handles. */
QueueHandle_t xFilteredQueue;
QueueHandle_t xTemperatureQueue;
QueueHandle_t xNQueue;
QueueHandle_t xSeedQueue;

volatile unsigned long ulHighFrequencyTimerTicks = 0;


/**
 * @brief Structure to store the lowest historical stack value of a task.
 */
typedef struct {
    TaskHandle_t xTaskHandle; /**< Handle of the task */
    uint16_t usLowestStack;   /**< Lowest stack value recorded for the task */
} TaskHistory_t;

/*-----------------------------------------------------------*/

/**
 * @brief Configures the hardware components like clocks, UART, and GPIO.
 */
static void prvSetupHardware(void);

/**
 * @brief Temperature sensor task to generate random temperature values.
 * 
 * @param pvParameters Pointer to the parameters passed to the task (unused).
 */
static void vTemperatureSensorTask(void *pvParameters);

/**
 * @brief Task to apply a low-pass filter on the temperature values.
 * 
 * @param pvParameters Pointer to the parameters passed to the task (unused).
 */
static void vFilterTask(void *pvParameters);

/**
 * @brief Task to display a graphical representation of filtered temperature values.
 * 
 * @param pvParameters Pointer to the parameters passed to the task (unused).
 */
static void vGraphTask(void *pvParameters);

/**
 * @brief Task to show the top tasks information periodically.
 * 
 * @param pvParameters Pointer to the parameters passed to the task (unused).
 */
static void vTopTask(void *pvParameters);

/**
 * @brief Converts an integer value to a character array representing the graph.
 * 
 * @param graph Character array to store the graph data.
 * @param value The value to be converted into graph representation.
 */
void intToGraph(unsigned char graph[2 * MAX_WIDTH], int value);

/**
 * @brief Sends a specified number of characters from a buffer over UART.
 *
 * This function sends `ulCount` characters from the buffer pointed to by `pucBuffer`
 * over the UART interface. It waits until the UART is ready to transmit before sending
 * each character.
 *
 * @param pucBuffer Pointer to the buffer containing the characters to send.
 * @param ulCount The number of characters to send from the buffer.
 */
void UARTSend(const char *pucBuffer, unsigned long ulCount);

/**
 * @brief Sends a null-terminated string over UART.
 *
 * This function sends each character of the null-terminated string pointed to by `str`
 * over the UART interface. It waits until the UART is ready to transmit before sending
 * each character.
 *
 * @param str Pointer to the null-terminated string to send.
 */
void UARTSendString(const char *str);

/**
 * @brief Converts an integer to a string.
 * 
 * @param num Integer value to be converted.
 * @param str Pointer to the buffer to store the converted string.
 */
void my_itoa(int num, char *str);

/**
 * @brief Calculates the length of a string.
 * 
 * @param str Pointer to the string.
 * @return size_t Length of the string.
 */
size_t my_strlen(const char *str);

/**
 * @brief Formats a string with various task information.
 * 
 * @param buffer Buffer to store the formatted string.
 * @param size Size of the buffer.
 * @param format Format string.
 * @param taskName Name of the task.
 * @param state State of the task.
 * @param priority Priority of the task.
 * @param stack Stack size of the task.
 * @param taskNumber Task number.
 */
void my_snprintf(char *buffer, size_t size, const char *format, const char *taskName, char state, unsigned int priority, unsigned int stack, unsigned int taskNumber);

/**
 * @brief Copies characters from the source string to the destination string, padding the remaining space with spaces until the specified width is reached.
 *
 * @param dest Pointer to the destination string.
 * @param src Pointer to the source string.
 * @param width The desired width of the destination string.
 */
void padString(char *dest, const char *src, int width);

/**
 * @brief Checks if the UART is busy.
 * 
 * @param ulBase Base address of the UART.
 * @return tBoolean True if UART is busy, false otherwise.
 */
tBoolean UARTBusy(unsigned long ulBase);

/**
 * @brief UART Interrupt Service Routine (ISR).
 */
void vUART_ISR(void);

/**
 * @brief Timer interrupt handler.
 */
void Timer0IntHandler(void);

/**
 * @brief Configures the timer for runtime statistics.
 */
void configureTimerForRunTimeStats(void);

/**
 * @brief Retrieves the current runtime counter value.
 * 
 * @return uint32_t Current runtime counter value.
 */
uint32_t getRunTimeCounterValue(void);

/**
 * @brief Returns a string representation of the given task state.
 *
 * This function takes a task state as input and returns a string representation
 * of the state. The possible task states are:
 * - eRunning:   Running state
 * - eReady:     Ready state
 * - eBlocked:   Blocked state
 * - eSuspended: Suspended state
 * - eDeleted:   Deleted state
 * - Unknown:    Unknown state
 *
 * @param state The task state to be converted to a string.
 * @return A string representation of the task state.
 */
const char* getTaskStateString(eTaskState state);

/**
 * @brief Generates a pseudo-random number using a linear congruential generator (LCG).
 *
 * This function retrieves a seed from a queue, uses it to generate a new pseudo-random number,
 * updates the seed, and then places the updated seed back into the queue.
 *
 * @return The generated pseudo-random number.
 */
unsigned int simple_rand(void);

#endif /* CORTEX_LM3S811_H */
