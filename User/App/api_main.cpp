/**
 * @file api_main.cpp
 * @author Rh
 * @brief 应用层与stm32的接口，放着最直接运行的函数
 * @version 0.1
 * @date 2026-01-20
 * 
 * @note 在C++中使用FreeRTOS的Task函数时
 *       需要将任务函数声明为extern "C"格式
 *       同时函数参数必须是void *pvParameters。
 *       所以，我直接把任务放到一个转接文件，然后extern
 *
 * @copyright Copyright (c) 2026
 * 
 */
#include "api_main.h"
#include "stdio.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"

/**
 * @brief 总硬件初始化
 * 
 */
void app_init()
{
  HAL_Delay(10);
}

/*
  因为CMSIS_OS2这个封装，导致很多东西和原生的FreeRTOS不一样，所以写法也会有的不一样
  CMSIS_OS2很多句柄不对外声明，如果想用只能extern出来单独用
  因为stm32提供了CubeMX配置的部分，故而使用Cubemx配置了，只有中间转接的，必须使用cpp的内容
  以下均为FreeRTOS的任务定义，因为使用c调用cpp，所以只能在这边定义，在freertos.c中的任务函数调用此函数就好
*/

/* 声明需要使用的句柄 */

extern osMessageQueueId_t messageQueueHandle;

/* 声明和CMSIS_OS2近乎同名的函数 */

void receive()
{
  osStatus_t xReturn = osOK;
  uint32_t   r_queue; /* 接收消息的变量 */
  /* Infinite loop */
  for (;;)
  {
    xReturn = osMessageQueueGet(messageQueueHandle, &r_queue, 0, osWaitForever);

    if (osOK == xReturn)
      printf("本次接收到的数据为%lu\n\n", r_queue);
    else
      printf("数据接收出错,错误代码0x%lu\n", xReturn);
    osDelay(10);
  }
}
