/**
 * @file bsp_usart.cpp
 * @author Rh
 * @brief 实现了串口1的串口重定向的阻塞发送。以及实现双缓冲区和环形缓冲区的DMA收发
 * @version 0.1
 * @date 2026-01-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "bsp_usart.hpp"
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
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
  }

  int _write(int fd, char *ptr, int len)
  {
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
  }
}

// 初始化这些常规数据
circle_buffer::circle_buffer() : head(0), tail(0), is_full(false)
{
}

// 判断是否为空
bool circle_buffer::empty() const
{
  return (!is_full && (head == tail));
}

// 判断是否为满
bool circle_buffer::full() const
{
  return is_full;
}

// 获取当前有效数据长度
size_t circle_buffer::size() const
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
size_t circle_buffer::write(const uint8_t *data, size_t len, RWMode mode)
{
  if (len == 0)
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
      head                 = (head + need_to_clear) % BUFFER_SIZE;
      is_full              = false; // 因为 head 移动了，不再满
    }
  }

  // 写入 len 字节
  for (size_t i = 0; i < len; ++i)
  {
    buffer[tail] = data[i];
    tail         = (tail + 1) % BUFFER_SIZE;
  }

  // 更新满状态
  if (tail == head)
  {
    is_full = true;
  }

  return len; // 总是返回请求的长度（Wait 模式保证成功，Overwrite 模式强制成功）
}

// 读取数据
size_t circle_buffer::read(uint8_t *data, size_t len)
{
  if (len == 0 || empty())
    return 0;

  // 计算实际能读取的长度（不能超过当前有效数据量）
  size_t available_data = size();
  size_t read_len       = fmin(len, available_data);

  for (size_t i = 0; i < read_len; ++i)
  {
    data[i] = buffer[head];
    head    = (head + 1) % BUFFER_SIZE; // 环形递增
  }

  // 只要读取了数据，缓冲区就不可能为满
  if (read_len > 0)
  {
    is_full = false;
  }

  return read_len;
}


