#ifndef __PROTOCOL_UART_HPP__
#define __PROTOCOL_UART_HPP__


#include "stdint.h"
#include "stddef.h"
#include "cmsis_os2.h"


/* 前向声明 */
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
class bsp_usart;


/**
 * @brief 协议帧结构体定义，取消对齐
 * @note 对应上位机 AA 55 CMD LEN DATA... SUM 0D 当然帧头帧尾可改
 */
#pragma pack(1)
typedef struct protocol_frame
{
  uint8_t header1;  // 0xAA 默认
  uint8_t header2;  // 0x55 默认
  uint8_t cmd;      // 指令码
  uint8_t len;      // 有效负载长度
  uint8_t data[64]; // 数据负载 (根据需求调整大小)
  uint8_t checksum; // 校验和
  uint8_t tail;     // 0x0D 默认
} protocol_frame_t;
#pragma pack()


class UartProtocolHandler
{
private:
  protocol_frame_t   rx_frame;        // 接收用结构体
  bsp_usart<256, 8> *uart_instance;   // 使用的串口驱动实例
  uint8_t            header1;         // 自定义帧头1
  uint8_t            header2;         // 自定义帧头2
  uint8_t            tail;            // 自定义帧尾
  char               taskName[32];    // 任务名称
  osThreadAttr_t     task_attributes; // 任务属性成员变量

  /**
   * @brief 计算校验和
   * @param data 数据缓冲区
   * @param len 长度
   * @return uint8_t 校验结果
   */
  uint8_t calculate_checksum(uint8_t *data, uint8_t len);

  /**
   * @brief 逻辑分发：根据指令执行具体动作
   * @param frame 完整的协议包
   */
  void protocol_handle_cmd();

public:
  /**
   * @brief 构造函数
   * @param h1 帧头1，默认0xAA
   * @param h2 帧头2，默认0x55
   * @param t  帧尾，默认0x0C
   */
  UartProtocolHandler(bsp_usart<256, 8> *uart_ptr, uint8_t name, uint8_t h1 = 0xAA, uint8_t h2 = 0x55, uint8_t t = 0x0C);

  /**
   * @brief 协议处理初始化
   * @param uart_ptr 指向bsp_usart实例的指针
   */
  void init(osThreadFunc_t _uart_protocol_task);

  /**
   * @brief 发送协议数据包给上位机
   * @param cmd 指令码
   * @param data 数据指针
   * @param len 数据长度
   */
  void send(uint8_t cmd, uint8_t *data, uint8_t len);

  /**
   * @brief 协议解析任务函数 (由 api_main.cpp 调用)
   */
  void task(void *argument);
};


/* 声明全局类实例化对象 */
extern UartProtocolHandler protocal_uart_6;


/* 任务函数需要extern "C"封装，会有多个任务函数 */
extern "C" void _uart_protocol_task6(void *argument);


#endif // __PROTOCOL_UART_HPP__