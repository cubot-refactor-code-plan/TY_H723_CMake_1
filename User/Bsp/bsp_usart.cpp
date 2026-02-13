/**
 * @file bsp_usart.cpp
 * @author Rh
 * @brief 实现了一个简易的串口驱动（FreeRTOS）（只接收最新数据不能用FIFO）
 * @version 0.1
 * @date 2026-02-08
 *
 * @todo 回调函数中只写了UART6，具体的处理需要自己添加进来
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief 使用示例：（必须要在freertos的任务中运行收发 中断中不行 中断不能阻塞）
         （使用的IDLE中断进行接收 发送也是同理）
 *
 * @note 模板实例化实现 以及类的实例化 第一个数字为缓冲区大小（uint8_t） 第二个数字为消息队列的长度（uint8_t）
 *
 *   // 全局实例化模板
 *   template class bsp_usart<256, 8>;  
 *
 *   // 全局实例化类
 *   __attribute__((section(".dma_buffer")))
 *   bsp_usart<256, 8> bsp_usart6(&huart6, bsp_usart<256, 8>::ReceiveMode::LATEST_ONLY, true);
 *
 *    bsp_usart6.init();                              // 需要freertos内核初始化成功之后使用
 *
 * @note extern好之后，在任务中使用
 *
 *    bsp_usart6.receiveData(buffer,8,osWaitForever); // 这样就存到buffer中了 时间是一直等
 *    bsp_usart6.sendData(buffer,8);                  // 就把buffer中的数据发送出去了
 *
 */

#include "bsp_usart.hpp"
#include <stdio.h>
#include "string.h"


/* 声明串口句柄 */

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;

#define PRINT_UART &huart6


/**
 * @brief ARM_GCC UART6 串口重定向、但阻塞 (printf)、使用了cubemx自带的设置，为重定向自动加锁
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


/**
 * @brief 此函数为IDLE中断触发函数，需要添加到it.c中，从而实现IDLE中断回调。在.h中extern "C"
 *
 * @param huart
 */
void HAL_BSP_UART_IRQHandler(UART_HandleTypeDef *huart)
{
  // 检查是否是IDLE中断
  if ((__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET) && (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_IDLE) != RESET))
  {
    // 清除IDLE中断标志
    __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_IDLE);

    // 写不了switch case
    if (huart == &huart6)
    {
      bsp_usart6.handleIdleInterruptInternal(huart); // 让类内部处理DMA计数器和BUFFER_SIZE
    }
    else
    {
      // 其他UART句柄的处理
    }
  }
}


/* 模板实例化实现 以及类的实例化 第一个数字为缓冲区大小（uint8_t） 第二个数字为消息队列的长度（uint8_t） 参数第一个串口句柄 第二个是模式 第三个是是否启用发送逻辑 */
template class bsp_usart<256, 8>;

// 这个 __attribute__((section(".dma_buffer"))) 是把他放到dtcm区域外，在.ld格式文件下实现的

// __attribute__((section(".dma_buffer"))) bsp_usart<256, 8> bsp_usart6(&huart6, bsp_usart<256, 8>::ReceiveMode::LATEST_ONLY, true);
__attribute__((section(".dma_buffer"))) bsp_usart<256, 8> bsp_usart6(&huart6, bsp_usart<256, 8>::ReceiveMode::SINGLE_BUFFER, true, 6); // 添加实例ID为6
// __attribute__((section(".dma_buffer"))) bsp_usart<256, 8> bsp_usart6(&huart6, bsp_usart<256, 8>::ReceiveMode::DOUBLE_BUFFER, true);


/**
 * @brief 以下是 bsp_usart<BUFFER_SIZE, MSG_SIZE>::bsp_usart 这个类的函数定义
 *
 * 串口驱动组件实现：（只使用了IDLE中断，其他未使用）
 * 设计要点：使用FreeRTOS的流缓冲区，实现单缓冲区和多缓冲区的处理
 * 不用阻塞：DMA收发，规范中断回调函数（DMA都开普通模式 FIFO在只取最新模式会有问题）
 * 线程安全：在多任务环境下的互斥访问控制，防止多个任务同时操作串口导致的问题
 * 错误处理：如何处理DMA传输错误、缓冲区溢出、硬件故障等情况
 * 接口设计：对外接口的易用性和一致性
 * 该类封装了基于STM32 HAL库、FreeRTOS和CMSIS_OS2的串口驱动功能，
 * 支持DMA传输、流缓冲区管理、互斥锁保护和错误处理等功能（错误处理那些回调有写，但是没放到回调函数中）
 *
 * @note 经过测试，无任何测试问题。
 * @note 单缓冲区在自己串口发送的时候串口助手显示有问题，但是逻辑是对的。内容可以正常的存入缓冲区然后等待一个一个的读取。
 * @note 双缓冲区逻辑无误，但是应用场景需要经过自己测试，理清他的逻辑，有点反直觉
 *
 * @param BUFFER_SIZE 存储的缓冲区大小（单双缓冲区）
 * @param MSG_SIZE 消息队列的大小（消息邮箱）
 *
 * @param huart 串口句柄
 * @param rx_mode 接收模式
 * @param transmit_signal 是否启用发送
 * @param instance_id 实例ID，用于生成唯一资源名称
 */
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
bsp_usart<BUFFER_SIZE, MSG_SIZE>::bsp_usart(UART_HandleTypeDef *huart, ReceiveMode rx_mode, bool transmit_signal, int instance_id)
  : _huart(huart), _receive_mode(rx_mode), _rx_active(false), _current_buffer(false), _buffer_size(BUFFER_SIZE), _msg_item_size(MSG_SIZE), _transmit_enable(transmit_signal), _last_received_length(0), _instance_id(instance_id)
{
  // 初始化成员变量但不执行资源分配
  _mutex_id             = nullptr;
  _msg_queue_id         = nullptr;
  _rx_stream_buffers[0] = nullptr;
  _rx_stream_buffers[1] = nullptr;
  _tx_stream_buffer     = nullptr;
}

template <size_t BUFFER_SIZE, size_t MSG_SIZE>
bool bsp_usart<BUFFER_SIZE, MSG_SIZE>::init()
{
  // 创建互斥锁，使用实例ID作为唯一标识
  snprintf(mutex_name, sizeof(mutex_name), "USART%d_Mutex", _instance_id);

  const osMutexAttr_t mutex_attr =
    {
      .name      = mutex_name,
      .attr_bits = 0,
      .cb_mem    = nullptr,
      .cb_size   = 0};

  _mutex_id = osMutexNew(&mutex_attr);
  if (_mutex_id == nullptr)
  {
    return false; // 互斥锁创建失败
  }

  // 根据接收模式创建消息队列 - 只有LATEST_ONLY模式才创建
  if (_receive_mode == ReceiveMode::LATEST_ONLY)
  {
    snprintf(msgq_name, sizeof(msgq_name), "USART%d_MsgQ", _instance_id);

    const osMessageQueueAttr_t msgq_attr =
      {
        .name      = msgq_name,
        .attr_bits = 0,
        .cb_mem    = nullptr,
        .cb_size   = 0,
        .mq_mem    = nullptr,
        .mq_size   = 0};

    // 对于LATEST_ONLY模式，消息队列长度为1，只保留最新数据
    _msg_queue_id = osMessageQueueNew(1, _msg_item_size, &msgq_attr);
    if (_msg_queue_id == nullptr)
    {
      cleanupResources(); // 清理已创建的资源
      return false;       // 消息队列创建失败
    }
  }
  else
  {
    _msg_queue_id = nullptr; // 非LATEST_ONLY模式不需要消息队列
  }

  // 初始化接收流缓冲区数组
  for (int i = 0; i < 2; i++)
  {
    _rx_stream_buffers[i] = nullptr;
  }

  // 根据接收模式创建相应的缓冲区
  switch (_receive_mode)
  {
    case ReceiveMode::SINGLE_BUFFER:
      _rx_stream_buffers[0] = xStreamBufferCreate(BUFFER_SIZE, 1);
      if (_rx_stream_buffers[0] == nullptr)
      {
        cleanupResources(); // 清理已创建的资源
        return false;       // 流缓冲区创建失败
      }
      break;
    case ReceiveMode::DOUBLE_BUFFER:
      // 创建两个流缓冲区用于双缓冲机制
      _rx_stream_buffers[0] = xStreamBufferCreate(BUFFER_SIZE, 1);
      if (_rx_stream_buffers[0] == nullptr)
      {
        cleanupResources(); // 清理已创建的资源
        return false;       // 流缓冲区创建失败
      }

      _rx_stream_buffers[1] = xStreamBufferCreate(BUFFER_SIZE, 1);
      if (_rx_stream_buffers[1] == nullptr)
      {
        cleanupResources(); // 清理已创建的资源
        return false;       // 流缓冲区创建失败
      }
      break;
    case ReceiveMode::LATEST_ONLY:
    default: // LATEST_ONLY
      // 不需要流缓冲区
      break;
  }

  if (_transmit_enable)
  {
    // 创建发送流缓冲区
    _tx_stream_buffer = xStreamBufferCreate(BUFFER_SIZE, 1);
    if (_tx_stream_buffer == nullptr)
    {
      cleanupResources(); // 清理已创建的资源
      return false;       // 发送流缓冲区创建失败
    }
  }
  else
  {
    _tx_stream_buffer = nullptr;
  }

  // 启用IDLE中断
  __HAL_UART_CLEAR_FLAG(_huart, UART_FLAG_IDLE);
  SET_BIT(_huart->Instance->CR1, USART_CR1_IDLEIE);

  // 启动接收
  startReception();

  return true; // 初始化成功
}

// 析构函数实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
bsp_usart<BUFFER_SIZE, MSG_SIZE>::~bsp_usart()
{
  stopReception();

  cleanupResources();
}

template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::cleanupResources()
{
  if (_mutex_id != nullptr)
  {
    osMutexDelete(_mutex_id);
    _mutex_id = nullptr;
  }

  // 只有LATEST_ONLY模式才创建了消息队列，需要删除
  if (_msg_queue_id != nullptr)
  {
    osMessageQueueDelete(_msg_queue_id);
    _msg_queue_id = nullptr;
  }

  // 统一释放接收流缓冲区
  for (int i = 0; i < 2; i++)
  {
    if (_rx_stream_buffers[i] != nullptr)
    {
      vStreamBufferDelete(_rx_stream_buffers[i]);
      _rx_stream_buffers[i] = nullptr;
    }
  }

  if (_tx_stream_buffer != nullptr)
  {
    vStreamBufferDelete(_tx_stream_buffer);
    _tx_stream_buffer = nullptr;
  }
}

// 发送数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
int bsp_usart<BUFFER_SIZE, MSG_SIZE>::sendData(const uint8_t *data, size_t size, uint32_t timeout)
{
  if (!_transmit_enable)
  {
    return -1; // 没使能发送 返回错误
  }

  if (_tx_stream_buffer == nullptr)
  {
    return -1; // 发送缓冲区未初始化
  }

  osStatus_t status = osMutexAcquire(_mutex_id, timeout); // 如果给了forever会一直等待直到成功
  if (status != osOK)
  {
    return -1; // 获取互斥锁失败 超时会这样
  }

  // 将数据写入发送流缓冲区
  size_t bytes_written = xStreamBufferSend(_tx_stream_buffer, data, size, pdMS_TO_TICKS(timeout));

  // 如果发送缓冲区中有数据，启动发送
  if (bytes_written > 0)
  {
    startTransmission();
  }

  osMutexRelease(_mutex_id);
  return bytes_written;
}

// 接收数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
int bsp_usart<BUFFER_SIZE, MSG_SIZE>::receiveData(uint8_t *buffer, size_t size, uint32_t timeout)
{
  // 根据接收模式进行不同的处理
  switch (_receive_mode)
  {
    case ReceiveMode::LATEST_ONLY:
    {
      // 在LATEST_ONLY模式下，从消息邮箱获取最新数据
      osStatus_t status = osMutexAcquire(_mutex_id, timeout);
      if (status != osOK)
      {
        return -1;
      }

      // 获取最新消息
      status = osMessageQueueGet(_msg_queue_id, buffer, nullptr, timeout);
      osMutexRelease(_mutex_id);

      if (status == osOK)
      {
        return (size < MSG_SIZE) ? size : MSG_SIZE; // 返回实际读取的字节数
      }
      else
      {
        return -1; // 没有数据或超时
      }
    }

    case ReceiveMode::SINGLE_BUFFER:
    {
      // 单缓冲处理
      if (_rx_stream_buffers[0] != nullptr)
      {
        size_t bytes_read = xStreamBufferReceive(_rx_stream_buffers[0], buffer, size, pdMS_TO_TICKS(timeout));
        return bytes_read;
      }
      return -1;
    }

    case ReceiveMode::DOUBLE_BUFFER:
    {
      // 双缓冲处理
      StreamBufferHandle_t target_buffer = _current_buffer ? _rx_stream_buffers[1] : _rx_stream_buffers[0];
      if (target_buffer != nullptr)
      {
        size_t bytes_read = xStreamBufferReceive(target_buffer, buffer, size, pdMS_TO_TICKS(timeout));
        return bytes_read;
      }
      return -1;
    }

    default:
      return -2; // 未定义的接收模式
  }
}

// 获取发送缓冲区剩余空间实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
size_t bsp_usart<BUFFER_SIZE, MSG_SIZE>::getTxFreeSpace()
{
  if (_tx_stream_buffer != nullptr)
  {
    return xStreamBufferSpacesAvailable(_tx_stream_buffer);
  }
  return 0;
}

// 获取接收缓冲区可用数据量实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
size_t bsp_usart<BUFFER_SIZE, MSG_SIZE>::getRxAvailableData()
{
  switch (_receive_mode)
  {
    case ReceiveMode::LATEST_ONLY:
    {
      // 对于LATEST_ONLY模式，检查消息队列是否有数据
      if (_msg_queue_id != nullptr)
      {
        uint32_t count = osMessageQueueGetCount(_msg_queue_id);
        return count * _msg_item_size;
      }
      return 0;
    }

    case ReceiveMode::SINGLE_BUFFER:
    {
      if (_rx_stream_buffers[0] != nullptr)
      {
        return xStreamBufferBytesAvailable(_rx_stream_buffers[0]);
      }
      return 0;
    }

    case ReceiveMode::DOUBLE_BUFFER:
    {
      size_t total_bytes = 0;
      for (int i = 0; i < 2; i++)
      {
        if (_rx_stream_buffers[i] != nullptr)
        {
          total_bytes += xStreamBufferBytesAvailable(_rx_stream_buffers[i]);
        }
      }
      return total_bytes;
    }

    default:
      return 0; // 未知接收模式
  }
}

// 开始接收数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::startReception()
{
  // 启动多字节DMA接收
  HAL_UART_Receive_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
  _rx_active = true;
}

// 停止接收数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::stopReception()
{
  _rx_active = false;
  HAL_UART_DMAStop(_huart);
}

// 开始传输数据实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::startTransmission()
{
  if (_tx_stream_buffer == nullptr)
  {
    return; // 发送缓冲区未初始化
  }

  if (!isTransmitting())
  {
    // 从发送缓冲区获取数据准备发送
    size_t bytes_to_send = xStreamBufferReceive(_tx_stream_buffer, _tx_dma_buffer, BUFFER_SIZE, 0);
    if (bytes_to_send > 0)
    {
      HAL_UART_Transmit_DMA(_huart, _tx_dma_buffer, bytes_to_send);
    }
  }
}

// 检查是否正在传输实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
bool bsp_usart<BUFFER_SIZE, MSG_SIZE>::isTransmitting()
{
  return (_huart->gState == HAL_UART_STATE_BUSY_TX);
}

// IDLE中断处理函数
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::handleIdleInterrupt(uint32_t received_length)
{
  _last_received_length = received_length;

  switch (_receive_mode)
  {
    case ReceiveMode::LATEST_ONLY:
    {
      // 处理整组数据，而非单个字节
      if (received_length >= _msg_item_size)
      {
        // 将整组数据的最后MSG_SIZE个字节作为最新数据
        uint8_t *latest_data_ptr = &_rx_dma_buffer[received_length - _msg_item_size];

        // 清空消息队列中已有的消息，然后发送新消息（确保只保留最新数据）
        uint8_t temp_data[MSG_SIZE];
        while (osMessageQueueGet(_msg_queue_id, temp_data, 0, 0) == osOK)
          ;

        osMessageQueuePut(_msg_queue_id, latest_data_ptr, 0, 0);
      }
      else if (received_length > 0)
      {
        // 如果接收的数据不足MSG_SIZE，将整个数据复制到一个临时缓冲区
        uint8_t temp_latest_data[MSG_SIZE] = {0}; // 初始化为0
        memcpy(temp_latest_data, _rx_dma_buffer, received_length);

        // 清空消息队列中已有的消息，然后发送新消息
        uint8_t temp_data[MSG_SIZE];
        while (osMessageQueueGet(_msg_queue_id, temp_data, 0, 0) == osOK)
          ;

        osMessageQueuePut(_msg_queue_id, temp_latest_data, 0, 0);
      }

      // 重新启动DMA接收
      if (_rx_active)
      {
        HAL_UART_Receive_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
      }
    }
    break;

    case ReceiveMode::SINGLE_BUFFER:
    case ReceiveMode::DOUBLE_BUFFER:
    {
      // 对于其他模式，将整个数据包放入流缓冲区
      StreamBufferHandle_t target_buffer = nullptr;

      if (_receive_mode == ReceiveMode::SINGLE_BUFFER)
      {
        target_buffer = _rx_stream_buffers[0];
      }
      else // DOUBLE_BUFFER
      {
        target_buffer   = _current_buffer ? _rx_stream_buffers[1] : _rx_stream_buffers[0];
        _current_buffer = !_current_buffer; // 双缓冲切换
      }

      // 1. 停止当前 DMA 传输，确保状态干净
      HAL_UART_DMAStop(_huart);

      // 2. 将数据发送到流缓冲区
      if (target_buffer != nullptr)
      {
        xStreamBufferSendFromISR(target_buffer, _rx_dma_buffer, received_length, nullptr);
      }

      // 3. 重新启动接收
      if (_rx_active)
      {
        HAL_UART_Receive_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
      }
    }
    break;
    default:
      return; // 未知情况
  }
}

// DMA传输完成回调函数实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::dmaTransferCompleteCallback(UART_HandleTypeDef *huart)
{
  if (huart->gState == HAL_UART_STATE_BUSY_RX)
  {
    // 接收完成处理 - 这种情况不应该发生，因为我们使用IDLE中断
    // 主要是用于错误恢复
    handleReceiveComplete();
  }
  else if (huart->gState == HAL_UART_STATE_BUSY_TX)
  {
    // 发送完成处理
    handleTransmitComplete();
  }
}

// IDLE中断处理函数
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::handleIdleInterruptInternal(UART_HandleTypeDef *huart)
{
  // 获取DMA剩余计数值，计算已接收的数据长度
  uint32_t dma_counter     = __HAL_DMA_GET_COUNTER(huart->hdmarx);
  uint32_t received_length = BUFFER_SIZE - dma_counter;

  // 处理接收到的数据包
  if (received_length > 0)
  {
    handleIdleInterrupt(received_length);
  }
}

// 处理发送完成事件实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::handleTransmitComplete()
{
  // 检查流缓冲区是否还有剩余数据需要继续通过 DMA 发送
  if (_tx_stream_buffer != nullptr)
  {
    size_t available = xStreamBufferBytesAvailable(_tx_stream_buffer);
    if (available > 0)
    {
      startTransmission();
    }
  }
}

// DMA错误回调函数实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::dmaErrorCallback(UART_HandleTypeDef *huart)
{
  // 记录错误状态并尝试恢复
  handleDmaError();
}

// 处理接收完成事件实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::handleReceiveComplete()
{
  // 对于多字节DMA+IDLE中断模式，接收完成主要是为了错误恢复
  // 实际的数据处理在IDLE中断中完成
  if (_rx_active)
  {
    HAL_UART_Receive_DMA(_huart, _rx_dma_buffer, BUFFER_SIZE);
  }
}

// 处理DMA错误实现
template <size_t BUFFER_SIZE, size_t MSG_SIZE>
void bsp_usart<BUFFER_SIZE, MSG_SIZE>::handleDmaError()
{
  // 停止当前传输
  HAL_UART_DMAStop(_huart);

  // 记录错误日志（实际应用中应使用适当的日志系统）
  printf("\n\n Error \n\n");

  // 尝试重启接收
  if (_rx_active)
  {
    startReception();
  }
}
