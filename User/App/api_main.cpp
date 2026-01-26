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
#include "main.h"
#include <stdio.h>
#include "bsp_usart.hpp"


/**
 * @brief 总初始化
 * 
 */
void app_init()
{
  HAL_Delay(10);
}

/*
  因为沟槽的CMSIS_OS2这个封装，导致很多东西和原生的FreeRTOS不一样
  以下均为FreeRTOS的任务定义，因为使用c调用cpp，所以只能在这边定义，在freertos.c中的任务函数调用此函数就好
*/





