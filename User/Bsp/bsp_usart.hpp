#ifndef __BSP_USART_HPP__
#define __BSP_USART_HPP__


#include <stdint.h>
#include <stdio.h>
#include "usart.h"
#include "string.h"


#define PRINT_UART &huart6


/**
 * @brief 
 * 
 */
enum class RWMode
{
  Wait,     // 阻塞等待直到空间足够
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
 * 这个存入原始数据会很难处理，建议包头包尾等等东西接收了、处理了再存入缓冲区（定长 2^n）
 *
 * 这种缓冲区的收发建议，因为在冲突时的写法不稳健
 * 使用该缓冲区发送时：建议定时器中断/空闲中断读数据，while中写数据（写数据如果确保全部发送应该会阻塞）（类似打印pid值 其他数据等等）。如果不会爆缓冲区就可以随意写
 * 使用该缓冲区接收时：写数据是一定是自动的，爆了就默认覆盖，阻塞会出大事。读取的话是正常读取，但是可能遇到覆盖问题。
 * 
 * 使用示例：
 * circle_buffer rx_buffer;
 * rx_buffer.write(data, len);
 * size_t read_len = rx_buffer.read(buffer, 8);
 * buffer/data中为你读取/写入的数据
 */
template <size_t BUFFER_SIZE = 512>
class circle_buffer
{
private:
  uint8_t         buffer[BUFFER_SIZE]; ///< 数据存储数组
  volatile size_t head;                ///< 读指针位置（下一个可读位置）
  volatile size_t tail;                ///< 写指针位置（下一个可写位置）
  bool            is_full;             ///< 满状态标志（解决head==tail二义性）

public:
  /**
     * @brief 构造函数（自动初始化缓冲区状态）
     */
  circle_buffer() : head(0), tail(0), is_full(false) { memset(buffer, 0, sizeof(buffer)); }
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
 * - 避免数据覆盖风险（其实不容易避免，就只能提醒）
 * - 适用于高速数据流场景
 * 
 * 双缓冲区如果可以，应该是能做成双环形的，但是实在懒得套，应用场景不多，要是有这个需求 记得写好类提交一下
 *
 * 双缓冲区：需要大概了解清楚这个是干嘛的，然后可能一开始用会出一点问题
 *
 * 使用示例：
 * double_buffer rx_buffer;
 * rx_buffer.on_buffer_swap = [](uint8_t* buf, size_t len) 
 * {
 *     process_data(buf, len);
 * };
 */
template <size_t BUFFER_SIZE = 512>
class double_buffer
{
private:
  uint8_t         buffer[2][BUFFER_SIZE]; ///< 双缓冲区存储
  volatile size_t write_index;            ///< 当前写入缓冲区索引
  volatile size_t write_pos;              ///< 当前写入位置
  volatile bool   is_busy;                ///< 是否繁忙

public:
  /**
   * @brief 缓冲区交换回调函数指针
   * 当缓冲区满或主动交换时触发
   * @param buf 交换出的缓冲区指针
   * @param len 有效数据长度
   *
   * @note 只需要区分好串口收/串口发 然后给这个on_buffer_swap指向对应的DMA发送/读取数据的处理就可以了，记得让他把is_busy在处理完事了变成false
   *
   */
  void (*on_buffer_swap)(uint8_t *buf, size_t len);

  /**
   * @brief 构造函数
   * @param func 
   */
  double_buffer(void (*func)(uint8_t *, size_t))
    : write_index(0), write_pos(0), on_buffer_swap(nullptr)
  {
    // 初始化缓冲区为0
    memset(buffer, 0, sizeof(buffer));
    on_buffer_swap = func;
  }

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

  /**
   * @brief 释放缓冲区的占用，类比于互斥锁的解锁，上锁需要手动在回调函数加
   * 
   */
  void release_buffer();
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
template <size_t RX_SIZE = 512, size_t TX_SIZE = 512>
class bsp_usart
{
private:
  size_t              rx_size;
  UART_HandleTypeDef *huart; ///< 串口寄存器基地址

  // 这六个指针指向不指向是构造函数干的事，因而不会占据空间

  circle_buffer<RX_SIZE> *rx_buffer_circle; ///< 接收缓冲区指针（可切换实现）
  double_buffer<RX_SIZE> *rx_buffer_double; ///< 双缓冲区实现（可选方案）
  uint8_t (*rx_buffer_array)[RX_SIZE];      ///< 不搞缓冲区实现（可选方案）
  circle_buffer<TX_SIZE> *tx_buffer_circle; ///< 接收缓冲区指针（可切换实现）
  double_buffer<TX_SIZE> *tx_buffer_double; ///< 双缓冲区实现（可选方案）
  uint8_t (*tx_buffer_array)[TX_SIZE];      ///< 不搞缓冲区实现（可选方案）


public:
  /**
   * @brief 构造函数
   * @param usart 串口外设实例（如huart1）
   */
  bsp_usart(UART_HandleTypeDef *usart, circle_buffer<RX_SIZE> *rx_c, double_buffer<RX_SIZE> *rx_d,
            uint8_t (*rx_a)[RX_SIZE], circle_buffer<TX_SIZE> *tx_c, double_buffer<TX_SIZE> *tx_d,
            uint8_t (*tx_a)[TX_SIZE])
    : huart(usart), rx_buffer_circle(rx_c), rx_buffer_double(rx_d), rx_buffer_array(rx_a),
      tx_buffer_circle(tx_c), tx_buffer_double(tx_d), tx_buffer_array(tx_a)
  {
    if (rx_a != nullptr)
    {
      memset(*rx_buffer_array, 0, RX_SIZE);
    }

    if (tx_a != nullptr)
    {
      memset(*tx_buffer_array, 0, TX_SIZE);
    }
  }

  /**
   * @brief 发送数据
   * @param data 数据指针
   * @param len 数据长度
   * @note 自动选择是哪一个缓冲区发送，你初始化哪一个就是哪一个
   */
  void send(const uint8_t *data, size_t len);

  /**
  * @brief 读取数据
  * @param data 数据指针
  * @param len 数据长度
  * @note 自动选择是哪一个缓冲区读取，你初始化哪一个就是哪一个
  */
  void receive(const uint8_t *data, size_t len);

  /**
   * @brief 接收中断处理
   * @note 需在HAL_UART_RxCpltCallback中调用
   */
  void receive_irq_handler();

  /**
  * @brief 接收中断处理
  * @note 需在HAL_UART_TxCpltCallback中调用
  */
 void send_irq_handler();
};

#endif // __BSP_USART_H__
