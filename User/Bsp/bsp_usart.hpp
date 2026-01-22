#ifndef __BSP_USART_HPP__
#define __BSP_USART_HPP__

#include <stdint.h>
#include <stdio.h>
#include "usart.h"

/*
  串口驱动组件实现
  包含环形缓冲区、双缓冲区实现及串口外设操作封装
  设计要点：
  1. 发送机制：当缓冲区满时阻塞等待空闲（超时机制需后续扩展）
  2. 接收机制：仅保留最新数据，旧数据被覆盖时触发回调提醒
  3. 缓冲区配置：通过constexpr指定固定大小（默认1KB），支持编译期配置
  4. 缓冲区选择：可通过模板参数在环形/双缓冲区间切换
*/

/**
 * @brief 
 * 
 */
enum class RWMode {
  Wait,    // 阻塞等待直到空间足够
  Overwrite // 覆盖旧数据
};

/**
 * @class circle_buffer
 * @brief 环形缓冲区实现（单缓冲区方案）
 * 
 * 采用经典环形缓冲区设计，通过头尾指针管理数据流
 * 特点：
 * - 固定内存分配（constexpr指定大小）
 * - 空/满状态通过额外标志位判断
 * - 适用于单生产者-单消费者场景
 *
 * 这个存入原始数据会很难处理，建议包头包尾等等东西接收了、处理了再存入缓冲区
 * 
 * 使用示例：
 * circle_buffer<1024> rx_buffer;
 * rx_buffer.write(data, len);
 * size_t read_len = rx_buffer.read(buffer, sizeof(buffer));
 */
class circle_buffer
{
private:
  static constexpr size_t BUFFER_SIZE = 1024;  ///< 缓冲区总容量（1KB）
  uint8_t                 buffer[BUFFER_SIZE]; ///< 数据存储数组
  size_t                  head;                ///< 读指针位置（下一个可读位置）
  size_t                  tail;                ///< 写指针位置（下一个可写位置）
  bool                    is_full;             ///< 满状态标志（解决head==tail二义性）

public:
  /**
     * @brief 构造函数（自动初始化缓冲区状态）
     */
  circle_buffer();

  /**
     * @brief 检查缓冲区是否为空
     * @return true 空状态，false 有数据
     */
  bool empty() const;

  /**
     * @brief 检查缓冲区是否已满
     * @return true 满状态，false 可写入
     */
  bool full() const;

  /**
     * @brief 获取当前有效数据长度
     * @return 有效字节数
     */
  size_t size() const;

  /**
     * @brief 写入数据到缓冲区，写入处理之后的数据
     * @param data 要写入数据的指针
     * @param len 数据长度(不要比缓冲区大，没写保护)
     * @return 实际写入长度（可能因空间不足而截断）
     */
  size_t write(const uint8_t *data, size_t len, RWMode mode = RWMode::Overwrite);

  /**
     * @brief 从缓冲区读取数据，读取的也是处理之后的
     * @param data 要读取数据存入的区域
     * @param len 请求读取长度
     * @return 实际读取长度
     */
  size_t read(uint8_t *data, size_t len);
};

/**
 * @class double_buffer
 * @brief 双缓冲区实现（乒乓缓冲方案）
 * 
 * 通过两个独立缓冲区实现数据交换：
 * - 前台缓冲区：当前正在接收数据的缓冲区
 * - 后台缓冲区：当前正在处理的缓冲区
 * 特点：
 * - 数据交换时触发回调通知
 * - 避免数据覆盖风险
 * - 适用于高速数据流场景
 * 
 * 使用示例：
 * double_buffer<1024> rx_buffer;
 * rx_buffer.on_buffer_swap = [](uint8_t* buf, size_t len) {
 *     process_data(buf, len);
 * };
 */
class double_buffer
{
private:
  static constexpr size_t BUFFER_SIZE = 1024;     ///< 单个缓冲区容量（1KB）
  uint8_t                 buffer[2][BUFFER_SIZE]; ///< 双缓冲区存储
  size_t                  write_index;            ///< 当前写入缓冲区索引
  size_t                  write_pos;              ///< 当前写入位置
  bool                    swap_requested;         ///< 缓冲区交换请求标志

public:
  /**
     * @brief 缓冲区交换回调函数指针
     * 当缓冲区满或主动交换时触发
     * 参数：交换出的缓冲区指针、有效数据长度
     */
  void (*on_buffer_swap)(uint8_t *buf, size_t len);

  /**
     * @brief 构造函数
     */
  double_buffer();

  /**
     * @brief 写入数据到当前缓冲区
     * @param data 数据指针
     * @param len 数据长度
     * @return 实际写入长度
     */
  size_t write(const uint8_t *data, size_t len);

  /**
     * @brief 主动触发缓冲区交换
     * 适用于非满但需要提前处理的场景
     */
  void swap_buffers();
};

/**
 * @class bsp_usart
 * @brief 串口外设操作封装类
 * 
 * 集成硬件操作与缓冲区管理：
 * - 支持环形/双缓冲区模式切换
 * - 接收数据覆盖检测
 * 
 * 典型用法：
 * bsp_usart uart(USART1);
 * uart.init(115200);
 * uart.send("Hello", 5);
 */
class bsp_usart
{
private:
  USART_TypeDef *instance;         ///< 串口寄存器基地址
  circle_buffer *rx_buffer_circle; ///< 接收缓冲区指针（可切换实现）
  double_buffer *rx_buffer_double; ///< 双缓冲区实现（可选方案）
  circle_buffer *tx_buffer_circle; ///< 接收缓冲区指针（可切换实现）
  double_buffer *tx_buffer_double; ///< 双缓冲区实现（可选方案）

public:
  /**
     * @brief 构造函数
     * @param usart 串口外设实例（如USART1）
     */
  bsp_usart(USART_TypeDef *usart);

  /**
     * @brief 初始化串口
     * @param baud_rate 波特率（如115200）
     */
  void init(uint32_t baud_rate);

  /**
     * @brief 发送数据（阻塞式）
     * @param data 数据指针
     * @param len 数据长度
     * @note 当缓冲区满时会阻塞等待空闲
     */
  void send(const uint8_t *data, size_t len);

  /**
     * @brief 接收中断处理
     * @note 需在HAL_UART_RxCpltCallback中调用
     */
  void irq_handler();
};


#endif // __BSP_USART_H__