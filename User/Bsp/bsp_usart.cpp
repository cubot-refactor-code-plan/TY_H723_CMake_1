/**
 * @file bsp_uart.cpp
 * @author Rh
 * @brief 实现了串口1的串口重定向的阻塞发送。以及实现双缓冲区和环形缓冲区的DMA收发
 * @version 0.1
 * @date 2026-01-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "bsp_usart.hpp"
#include "usart.h"


/**
 * @brief ARM_GCC UART1 串口重定向、但阻塞 (printf)
 * 
 * @note extern "C" 的原因是，这些函数是覆盖在原来weak弱定义上的，不能被cpp进行名称修饰
 *
 */
extern "C"
{
  int __io_putchar(int ch)
  {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
  }

  int _write(int fd, char *ptr, int len)
  {
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
  }
}

// @TODO 环形缓冲区 双缓冲区 串口 对应的3个类 以及收发实现

