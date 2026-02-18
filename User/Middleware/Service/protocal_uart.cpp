/**
 * @file protocol_uart.cpp
 * @author Rh
 * @attention 与其他模块通讯约定实现
 * @brief 串口协议解析实现，基于bsp_usart。可以实现创建多个串口协议
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief 使用示例
 *
 * @note 创建+初始化
 *
 *    // 先实例化bsp_usart（在bsp文件中）
 *
 *    // 全局类对象实例化（此文件中）
 *    UartProtocolHandler protocal_uart_6(&bsp_usart6, 6); // 此处可以添加帧头帧尾的信息
 *
 *    // 初始化协议（app_main的freertos_init中）
 *    // 协议中的_uart_protocol_task6在下面note中有写
 *    protocal_uart_6.init(_uart_protocol_task6);
 *
 * @note 定义C风格接口转换成FreeRTOS能看懂的任务，例如：_uart_protocol_task6
 *
 *    extern "C" void _uart_protocol_task6(void *argument)
 *    {
 *      protocal_uart_6.task(argument);
 *    }
 *
 * @note 回调函数处理：
 *
 *    static void uart_protocol_process_callback(bsp_usart<256, 8> *cur_uart, protocol_frame_t *rx_frame)
 *    {
 *      if (cur_uart == &bsp_usart6)
 *      {
 *      }
 *    }
 *
 */

#include "protocol_uart.hpp"
#include "bsp_usart.hpp"
#include "string.h"
#include <stdio.h>


/* 全局类对象实例化 */
UartProtocolHandler protocal_uart_6(&bsp_usart6, 6);


/* C接口函数实现(FreeRTOS)，因为每一个协议的处理方式都不一样，因此得extern好几个 */
extern "C" void _uart_protocol_task6(void *argument)
{
  protocal_uart_6.task(argument);
}


/**
 * @brief 串口协议处理回调函数
 *
 * @param cur_uart 使用的串口指针
 * @param rx_frame 收到的结构体指针
 */
static void uart_protocol_process_callback(bsp_usart<256, 8> *cur_uart, protocol_frame_t *rx_frame)
{
  if (cur_uart == &bsp_usart6)
  {
    // 具体的指令码，以及数据解析，如果到时候真的需要，可以写互斥锁来保护临界区资源，也可以自定义枚举量
    switch (rx_frame->cmd)
    {
      case 0x01:
        break;
      default:
        break;
    }
  }
  else // 其他这种类型的串口需要写的回调函数
  {
  }
}


/**
 * @brief 以下函数均为类的实现，此为构造函数，可以不需要管实现部分
 * @attention 校验和:(header1+header2+CMD+LEN+DATA) & 0xFF
 *
 * @param uart_ptr bsp层串口实例
 * @param h1 帧头1
 * @param h2 帧头2
 * @param t  帧尾
 * @param name FreeRTOS任务的编号
 */
UartProtocolHandler::UartProtocolHandler(bsp_usart<256, 8> *uart_ptr, uint8_t name, uint8_t h1, uint8_t h2, uint8_t t) : uart_instance(uart_ptr), header1(h1), header2(h2), tail(t)
{
  // 初始化任务属性成员变量，使用实例特定的名称
  snprintf(taskName, sizeof(taskName), "uart_protocol_%d", name);
  task_attributes.name       = taskName;
  task_attributes.stack_size = 256 * 4; // 适当增加栈大小以处理协议解析
  task_attributes.priority   = (osPriority_t)osPriorityNormal;
}

/**
 * @brief 协议层初始化，包含串口任务
 */
void UartProtocolHandler::init(osThreadFunc_t _uart_protocol_task)
{
  memset(&rx_frame, 0, sizeof(rx_frame));

  // 创建串口协议处理任务，会有多个
  osThreadNew(_uart_protocol_task, nullptr, &task_attributes);
}

/**
 * @brief 计算校验和
 * @param data 数据缓冲区
 * @param len 长度
 * @return uint8_t 校验结果
 */
uint8_t UartProtocolHandler::calculate_checksum(uint8_t *data, uint8_t len)
{
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    sum += data[i];
  }
  return sum;
}

/**
 * @brief 发送协议帧到上位机
 */
void UartProtocolHandler::send(uint8_t cmd, uint8_t *data, uint8_t len)
{
  if (uart_instance == nullptr)
    return;

  static uint8_t tx_buf[128];
  tx_buf[0] = header1;
  tx_buf[1] = header2;
  tx_buf[2] = cmd;
  tx_buf[3] = len;
  memcpy(&tx_buf[4], data, len);

  // 计算校验和 (header1 header2 CMD LEN DATA)
  tx_buf[4 + len] = calculate_checksum(tx_buf, 4 + len);
  tx_buf[5 + len] = tail;

  uart_instance->sendData(tx_buf, 6 + len, 10);
}

/**
 * @brief 逻辑分发：根据指令执行具体动作
 * @param frame 完整的协议包
 */
void UartProtocolHandler::protocol_handle_cmd()
{
  if (uart_instance == nullptr)
    return;

  uart_instance->sendData(rx_frame.data, rx_frame.len);
  uart_protocol_process_callback(uart_instance, &rx_frame);
}

/**
 * @brief 串口协议处理任务
 */
void UartProtocolHandler::task(void *argument)
{
  if (uart_instance == nullptr)
    return;

  printf("UART Protocol Task (Batch Mode) Started\n");

  // 临时缓冲区，用于存放帧头 (Header1, Header2, CMD, LEN)
  uint8_t header_buf[4];
  // 临时缓冲区，用于存放后续数据 (Data + Checksum + Tail)
  uint8_t payload_buf[70]; // 64 (max data) + 1 (sum) + 1 (tail)

  for (;;)
  {
    // 1. 寻找帧头：先同步第一个包头
    if (uart_instance->receiveData(&header_buf[0], 1, osWaitForever) <= 0)
      continue;
    if (header_buf[0] != header1)
      continue;

    // 2. 读取剩余的帧头部分 (Header2, CMD, LEN)
    // 使用较短的超时时间，防止因发送端异常导致的死等
    if (uart_instance->receiveData(&header_buf[1], 3, 10) < 3)
      continue;

    // 校验第二个包头
    if (header_buf[1] != header2)
      continue;

    // 解析协议参数
    rx_frame.cmd = header_buf[2];
    rx_frame.len = header_buf[3];

    // 3. 长度合法性检查 (防止内存越界)
    if (rx_frame.len > 64)
    {
      continue;
    }

    // 4. 批量读取后续内容：Data + Checksum (1 byte) + Tail (1 byte)
    uint8_t remaining_len = rx_frame.len + 2;
    if (uart_instance->receiveData(payload_buf, remaining_len, 20) < remaining_len)
    {
      continue;
    }

    // 5. 校验和验证
    // 构建临时缓冲区用于计算校验和 (header1 header2 CMD LEN DATA)
    uint8_t check_buf[70];
    memcpy(check_buf, header_buf, 4);
    memcpy(&check_buf[4], payload_buf, rx_frame.len);

    uint8_t calculated_sum = calculate_checksum(check_buf, 4 + rx_frame.len);
    uint8_t received_sum   = payload_buf[rx_frame.len];
    uint8_t received_tail  = payload_buf[rx_frame.len + 1];

    // 6. 最终判定与处理
    if (calculated_sum == received_sum && received_tail == tail)
    {
      // 将数据拷贝到全局帧结构体
      memcpy(rx_frame.data, &payload_buf[0], rx_frame.len);
      // 处理业务逻辑
      protocol_handle_cmd();
    }
  }
}
