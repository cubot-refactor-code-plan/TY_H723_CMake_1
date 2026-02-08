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

#include "bsp_usart.hpp"

/**
 * @brief main中初始化（无freertos）
 * @note  也就是在main.c中写了一个函数调用，这样转嫁就可以使用cpp了 
 *
 */
void app_init()
{
  printf("app_init\n");
}

/**
 * @brief 和freertos有关的初始化
 * @note  也就是在main.c中的MX_FREERTOS_INIT里，写了一个函数调用，这样转嫁就可以使用cpp了 
 * 
 */
void freertos_init()
{
  bsp_usart6.init();
  printf("freertos_init\n");
}


/**
 * 因为CMSIS_OS2这个封装，导致很多东西和原生的FreeRTOS不一样，所以写法也会有的不一样
 * CMSIS_OS2很多句柄不对外声明，如果想用只能extern出来用
 * 虽然说CMSIS_OS2做了层封装，方便使用。但是原生的FreeRTOS在以后要用的时候，还是要花时间适应
 * 默认任务只能weak声明，其他的可以使用外部声明
 * CubeMX提供了FreeRTOS配置的部分，故而使用Cubemx配置了，只有一些内容，必须使用cpp来写
 * 以下均为FreeRTOS的内容定义，因为使用c调用cpp，所以只能在这边定义，然后外部声明让官方接口调用
 * printf要加\n
 */


/* 声明需要使用的句柄 */

/* CMSIS_OS2中使用的是声明，我只需要外部定义同名的函数，然后exteren "C"即可 */


/**
 * @brief 默认任务
 * 
 * @param argument 默认参数
 */
void _defaultTask(void *argument)
{
  uint8_t buffer[256] = {0};
  for (;;)
  {
    int count = bsp_usart6.receiveData(buffer,256,osWaitForever);
    if(count > 0)
    {
      bsp_usart6.sendData(buffer,256);
    }
  }
}
