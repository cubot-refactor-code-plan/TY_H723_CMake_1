/**
 * @file bsp_usart.cpp
 * @author Rh
 * @brief 实现了串口1的串口重定向的阻塞发送。以及实现双缓冲区和环形缓冲区的DMA收发
 * @version 0.1
 * @date 2026-01-20
 *
 * @todo 希望有个大手子把这里面的运算和算法优化了
 * @todo 串口类的实现：包括使用缓冲区和only使用简单数组的版本。测试代码。写出读取缓冲区最新x位的程序
 * 
 * @copyright Copyright (c) 2026
 * 
 */

/*
 串口驱动组件实现
 包含环形缓冲区、双缓冲区实现及串口外设操作封装
 设计要点：
 1. 发送机制：当缓冲区满时阻塞等待空闲（超时机制需后续扩展）
 2. 接收机制：仅保留最新数据，旧数据被覆盖时触发回调提醒
 3. 缓冲区配置：通过constexpr指定固定大小（默认1KB），支持编译期配置
 4. 缓冲区选择：可通过模板参数在环形/双缓冲区间切换
 5. 要实现正常读数据和读最新的给定长度的数据

 关于使用需求：如果只要最新的，就别用缓冲区了，直接volatile一个结构体存着用

 缓冲区的目的是尽可能的接收到一大堆数据，实在不行了就丢弃，是一个权衡之举
*/

#include "bsp_usart.hpp"
#include <stdio.h>

/**
 * @brief ARM_GCC UART1 串口重定向、但阻塞 (printf)、使用了cubemx自带的设置，为重定向自动加锁
 * 
 * @note extern "C" 的原因是，这些函数是覆盖在原来weak弱定义上的，不能被cpp进行名称修饰
 *
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

