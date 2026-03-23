#include "bsp_usart.hpp"
#include "protocol_maixcam.hpp"
#include "protocol_usart.hpp"
#include "string.h"
#include <stdio.h>



/* USER CODE BEGIN */

/* ==================== 全局类对象实例化 ==================== */

protocol_usart protocal_usart_9(&bsp_usart9, 9);

/* ==================== C函数实现 ==================== */
static inline void protocol_usart_callback(protocol_usart* p_usart)
{
  if (p_usart == &protocal_usart_9)
  {
    /* 调用MaixCam协议解析函数 */
    maixcam.parse(p_usart->get_rx_cmd(), p_usart->get_rx_data(), p_usart->get_rx_len());
  }
}

/* USER CODE END */


/**
 * @brief UART协议任务（C函数）
 * @param argument 任务参数（传入类指针this）
 */
void _uart_protocol_task_entry(void* argument)
{
  /* 通过argument获取类指针 */
  protocol_usart* self = static_cast<protocol_usart*>(argument);

  if (self->uart_instance == nullptr)
  {
    return;
  }

  printf("UART Protocol Task Started\n");

  /* 临时缓冲区 */
  uint8_t header_buf[4];   ///< 帧头缓冲区
  uint8_t payload_buf[70]; ///< 数据缓冲区（64字节数据+校验和+帧尾）

  for (;;)
  {
    /* 1. 寻找帧头：先同步第一个包头 */
    if (self->uart_instance->receive(&header_buf[0], 1, osWaitForever) <= 0)
    {
      continue;
    }
    if (header_buf[0] != self->header1)
    {
      continue;
    }

    /* 2. 读取剩余的帧头部分 */
    if (self->uart_instance->receive(&header_buf[1], 3, 1000) < 3)
    {
      continue;
    }

    /* 校验第二个包头 */
    if (header_buf[1] != self->header2)
    {
      continue;
    }

    /* 解析协议参数 */
    self->rx_frame.cmd = header_buf[2];
    self->rx_frame.len = header_buf[3];

    /* 3. 长度合法性检查 */
    if (self->rx_frame.len > 64)
    {
      continue;
    }

    /* 4. 批量读取后续内容 */
    uint8_t remaining_len = self->rx_frame.len + 2;
    if (self->uart_instance->receive(payload_buf, remaining_len, 1000) < remaining_len)
    {
      continue;
    }

    /* 5. 校验和验证 */
    uint8_t check_buf[70];
    memcpy(check_buf, header_buf, 4);
    memcpy(&check_buf[4], payload_buf, self->rx_frame.len);

    uint8_t calculated_sum = self->calculate_checksum(check_buf, 4 + self->rx_frame.len);
    uint8_t received_sum   = payload_buf[self->rx_frame.len];
    uint8_t received_tail  = payload_buf[self->rx_frame.len + 1];

    /* 6. 最终判定与处理 */
    if (calculated_sum == received_sum && received_tail == self->tail)
    {
      /* 拷贝数据到帧结构体 */
      memcpy(self->rx_frame.data, &payload_buf[0], self->rx_frame.len);
      /* 处理业务逻辑 */
      self->protocol_handle_cmd();
    }
  }
}


/* ==================== 类函数实现 ==================== */

/**
 * @brief 构造函数
 *
 * @attention 校验和计算：(header1+header2+CMD+LEN+DATA) & 0xFF
 *
 * @param uart_ptr 串口实例指针
 * @param name 实例名称编号
 * @param h1 帧头1
 * @param h2 帧头2
 * @param t 帧尾
 */
protocol_usart::protocol_usart(bsp_usart<256, 8>* uart_ptr, uint8_t name, uint8_t h1, uint8_t h2, uint8_t t)

  : uart_instance(uart_ptr),
    header1(h1),
    header2(h2),
    tail(t)
{
  /* 初始化任务属性成员变量 */
  snprintf(task_name, sizeof(task_name), "uart_protocol_%d", name);
  task_attributes.name       = task_name;
  task_attributes.stack_size = 256 * 4;
  task_attributes.priority   = (osPriority_t)osPriorityNormal;
}


/**
 * @brief 协议层初始化
 */
void protocol_usart::init()
{
  memset(&rx_frame, 0, sizeof(rx_frame));

  /* 创建串口协议处理任务，传入this指针 */
  osThreadNew(_uart_protocol_task_entry, this, &task_attributes);
}


/**
 * @brief 计算校验和
 * @param data 数据缓冲区
 * @param len 长度
 * @return uint8_t 校验结果
 */
uint8_t protocol_usart::calculate_checksum(uint8_t* data, uint8_t len)
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
 * @param cmd 指令码
 * @param data 数据指针
 * @param len 数据长度
 */
void protocol_usart::send(uint8_t cmd, uint8_t* data, uint8_t len)
{
  if (uart_instance == nullptr)
  {
    return;
  }

  static uint8_t tx_buf[128];
  tx_buf[0] = header1;
  tx_buf[1] = header2;
  tx_buf[2] = cmd;
  tx_buf[3] = len;
  memcpy(&tx_buf[4], data, len);

  /* 计算校验和 */
  tx_buf[4 + len] = calculate_checksum(tx_buf, 4 + len);
  tx_buf[5 + len] = tail;

  uart_instance->send(tx_buf, 6 + len, 10);
}


/**
 * @brief 逻辑分发：根据指令执行具体动作
 */
void protocol_usart::protocol_handle_cmd()
{
  /* 具体的指令码及数据解析 */
  protocol_usart_callback(this);
}
