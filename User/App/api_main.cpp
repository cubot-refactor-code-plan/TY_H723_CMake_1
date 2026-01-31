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
  CMSIS_OS2很多句柄不对外声明，如果想用只能extern出来用
  默认任务不能extern出来，但是可以改ioc文件里面的部分，让他可以外部定义这个任务
  CubeMX提供了FreeRTOS配置的部分，故而使用Cubemx配置了，只有一些内容，必须使用cpp来写
  以下均为FreeRTOS的内容定义，因为使用c调用cpp，所以只能在这边定义，然后外部声明让官方接口调用
*/

/* 声明需要使用的句柄 */

extern osSemaphoreId_t sendSemHandle;

/* CMSIS_OS2中使用的是声明，我只需要外部定义同名的函数，然后exteren "C" */

void _defaultTask(void *argument)
{
  int count = 0;
  for(;;)
  {
    count++;
    if(count == 100)
    {
      count = 0;
      osSemaphoreRelease(sendSemHandle);
      printf("release\n");
    }
    osDelay(10);
  }
}

void _receiveTask(void *argument)
{

  /* Infinite loop */
  for (;;)
  {
    if(osSemaphoreAcquire(sendSemHandle,osWaitForever) == osOK)
    {
      printf("accquire\n");
    }
    
  }

}

void _transmitTask(void *argument)
{


  /* Infinite loop */
  for (;;)
  {
    osDelay(1000);
    printf("task_t delay 1s\n");
  }
}
