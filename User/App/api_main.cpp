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
#include "bsp_usart.hpp"

int a;

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
  HAL_Delay(100);
  a = printf_dma("%d\n", count);
}

/**
 * @brief 串口发送完成回调
 * 
 * @param huart 对应串口句柄
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart == PRINT_UART)
  {
    printf("ok\n");
  }
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

/**
 * @brief DMA半传输完成回调
 * 
 * @param hdma DMA句柄指针
 */
void HAL_DMA_HalfCpltCallback(DMA_HandleTypeDef *hdma)
{
}

/**
 * @brief DMA传输完成回调
 * 
 * @param hdma DMA句柄指针
 */
void HAL_DMA_CpltCallback(DMA_HandleTypeDef *hdma)
{
}

/**
 * @brief 串口接收空闲中断回调（IDLE中断）
 * 
 * @param huart 对应串口句柄
 * 
 * @note 注意：此回调需要在USART初始化时通过以下方式启用：
 * __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
 * 并在NVIC中使能对应中断
 */
void HAL_UART_RxIdleCallback(UART_HandleTypeDef *huart)
{
  // // 1. 获取当前已接收数据长度（需配合DMA使用）
  // uint32_t dma_counter = __HAL_DMA_GET_COUNTER(huart->hdmarx);
  // uint32_t received_len = huart->RxXferSize - dma_counter;

  // // 2. 处理接收到的完整数据帧
  // // process_received_frame((uint8_t*)huart->pRxBuffPtr, received_len);

  // // 3. 重新启动DMA接收（双缓冲模式需要特殊处理）
  // HAL_UARTEx_ReceiveToIdle_DMA(huart, (uint8_t*)huart->pRxBuffPtr, huart->RxXferSize);
}