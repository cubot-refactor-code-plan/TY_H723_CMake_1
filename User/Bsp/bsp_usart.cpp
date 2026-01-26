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
#include <stdarg.h>
#include "math.h"

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
    HAL_UART_Transmit(PRINT_UART, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
  }

  int _write(int fd, char *ptr, int len)
  {
    HAL_UART_Transmit(PRINT_UART, (uint8_t *)ptr, len,HAL_MAX_DELAY);
    return len;
  }
}


/**
 * @brief Construct a new circle buffer::circle buffer object
 * 
 */
// 判断是否为空
template <>
bool circle_buffer<>::empty() const
{
  return (!is_full && (head == tail));
}

// 判断是否为满
template <>
bool circle_buffer<>::full() const
{
  return is_full;
}

// 获取当前有效数据长度
template <size_t BUFFER_SIZE>
size_t circle_buffer<BUFFER_SIZE>::size() const
{
  if (is_full)
  {
    return BUFFER_SIZE;
  }
  if (tail >= head)
  {
    return tail - head;
  }
  return BUFFER_SIZE + tail - head;
}

// 写入缓冲区，默认覆盖旧数据
template <size_t BUFFER_SIZE>
size_t circle_buffer<BUFFER_SIZE>::write(const uint8_t *data, size_t len, RWMode mode)
{
  if (len == 0 || len > BUFFER_SIZE)
    return 0;

  if (mode == RWMode::Wait)
  {
    printf("wait buffer is full \n");
    //! 这里，如果在中断中进入了这个阻塞会崩掉，我觉得这种场景很大程度上就是主while里面写入，DMA空闲中断去发，不会死在里面
    while (BUFFER_SIZE - size() < len)
    {
      // 可选：加入喂狗或 yield
      // __WFI(); 或 HAL_Delay(1);
    }
    // 此时空间足够，直接写入全部 len
  }
  else if (mode == RWMode::Overwrite)
  {
    size_t free_space = BUFFER_SIZE - size();
    if (len > free_space)
    {
      printf("Buffer overflow! Overwriting %u bytes.\n", (unsigned int)(len - free_space));
      // 覆盖旧数据：移动 head
      size_t need_to_clear = len - free_space;
      head                 = (head + need_to_clear) & (BUFFER_SIZE - 1);
      is_full              = false; // 因为 head 移动了，不再满
    }
  }

  // 写入 len 字节
  for (size_t i = 0; i < len; ++i)
  {
    buffer[tail] = data[i];
    tail         = (tail + 1) & (BUFFER_SIZE - 1);
  }

  // 更新满状态
  if (tail == head)
  {
    is_full = true;
  }

  return len;
}

// 读取数据
template <size_t BUFFER_SIZE>
size_t circle_buffer<BUFFER_SIZE>::read(uint8_t *data, size_t len)
{
  if (len == 0 || empty())
    return 0;

  // 计算实际能读取的长度（不能超过当前有效数据量）
  size_t available_data = size();
  size_t read_len       = fmin(len, available_data);

  for (size_t i = 0; i < read_len; ++i)
  {
    data[i] = buffer[head];
    head    = (head + 1) & (BUFFER_SIZE - 1); // 环形递增
  }

  // 只要读取了数据，缓冲区就不可能为满
  if (read_len > 0)
  {
    is_full = false;
  }

  return read_len;
}

/**
 * @brief Construct a new double buffer::double buffer object
 * 
 */
// 写入缓冲区
template <size_t BUFFER_SIZE>
size_t double_buffer<BUFFER_SIZE>::write(const uint8_t *data, size_t len)
{
  size_t bytes_written = 0;

  while (bytes_written < len)
  {
    size_t remaining_space = BUFFER_SIZE - write_pos;
    size_t chunk_size =
      (len - bytes_written) > remaining_space ? remaining_space : (len - bytes_written);

    memcpy(&buffer[write_index][write_pos], &data[bytes_written], chunk_size);

    write_pos += chunk_size;
    bytes_written += chunk_size;

    // 当写满一个缓冲区时
    if (write_pos >= BUFFER_SIZE)
    {
      if (is_busy)
      {
        // 如果另一半缓冲区还在发送中(busy)
        // 我们不交换缓冲区，而是直接把当前缓冲区的写指针重置
        // 这样后续的数据会从当前缓冲区的 0 位置开始覆盖旧数据
        write_pos = 0;
      }
      else
      {
        // 后台空闲，正常交换
        swap_buffers();
      }
    }
  }
  return bytes_written;
}
template <>
void double_buffer<>::swap_buffers()
{
  uint8_t *finished_buf = buffer[write_index];
  size_t   finished_len = write_pos;

  // 切换索引
  write_index = (write_index == 0) ? 1 : 0;
  write_pos   = 0;

  if (on_buffer_swap != nullptr && finished_len > 0)
  {
    is_busy = true; // 标记后台开始处理
    on_buffer_swap(finished_buf, finished_len);
  }
}

template <>
void double_buffer<>::release_buffer()
{
  is_busy = false;
}
