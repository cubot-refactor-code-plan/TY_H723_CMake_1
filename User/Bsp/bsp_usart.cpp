/**
 * 串口驱动组件实现：
 * 设计要点：使用FreeRTOS的流缓冲区，实现单缓冲区和多缓冲区的处理
 * 不用阻塞：DMA收发，规范中断回调函数
 * 线程安全：在多任务环境下的互斥访问控制，防止多个任务同时操作串口导致的问题
 * 错误处理：如何处理DMA传输错误、缓冲区溢出、硬件故障等情况
 * 接口设计：对外接口的易用性和一致性
 */

#include "bsp_usart.hpp"
#include <stdio.h>

/**
 * @brief ARM_GCC UART1 串口重定向、但阻塞 (printf)、使用了cubemx自带的设置，为重定向自动加锁
 * @note extern "C" 的原因是，这些函数是覆盖在原来weak弱定义上的，不能被cpp进行名称修饰
 */
extern "C"
{
  int __io_putchar(int ch)
  {
    HAL_UART_Transmit(PRINT_UART, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
  }

  int _write(int fd, char *ptr, int len)
  {
    HAL_UART_Transmit(PRINT_UART, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
  }
}
