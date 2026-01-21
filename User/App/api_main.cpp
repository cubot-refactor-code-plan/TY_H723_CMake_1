/**
 * @file api_main.cpp
 * @author Rh
 * @brief 应用层与stm32的接口，放着最直接运行的函数
 * @version 0.1
 * @date 2026-01-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "api_main.h"
#include "main.h"
#include <stdio.h>

/**
 * @brief 总初始化
 * 
 */
void app_init()
{
  HAL_Delay(10);
}

/**
 * @brief 总while循环
 * 
 */
void app_while()
{
  static int count = 0;
  count++;
  HAL_Delay(500);

  // HAL_UART_Transmit(&huart1, a, 7, 100);
  printf("%d\n", count);
}

/**
 * @brief 串口发送完成回调
 * 
 * @param huart 对应串口句柄
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
}

/**
 * @brief 串口接收完成回调
 *
 * @param huart 对应串口句柄
 * 
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
}

/**
 * @brief 定时器计数溢出中断回调
 *
 * @param htim 对应定时器句柄
 * 
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
}
