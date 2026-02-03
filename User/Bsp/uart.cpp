#include "uart.hpp"
#include "string.h"


extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;

// 全局对象实例（用户将在自己的代码中创建）
extern SerialDriver<256, 64, sizeof(uint32_t)> g_serial_driver_uart1;
extern SerialDriver<256, 64, sizeof(uint32_t)> g_serial_driver_uart2;
extern SerialDriver<256, 64, sizeof(uint32_t)> g_serial_driver_uart3;
extern SerialDriver<256, 64, sizeof(uint32_t)> g_serial_driver_uart6;

// 构造函数实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::SerialDriver(UART_HandleTypeDef *huart, ReceiveMode rx_mode, bool transmit_signal)
  : _huart(huart), _receive_mode(rx_mode), _rx_active(false), _current_buffer(false), _buffer_size(BUFFER_SIZE), _msg_item_size(MSG_SIZE), _transmit_enable(transmit_signal)
{
  // 创建互斥锁 (使用CMSIS-RTOS2 API)
  const osMutexAttr_t mutex_attr =
    {
      .name      = "SerialMutex",
      .attr_bits = 0,
      .cb_mem    = NULL,
      .cb_size   = 0};

  _mutex_id = osMutexNew(&mutex_attr);

  // 创建消息队列（使用CMSIS-RTOS2 API），MSG_SIZE通过模板参数传入
  const osMessageQueueAttr_t msgq_attr =
    {
      .name      = "SerialMsgQ",
      .attr_bits = 0,
      .cb_mem    = NULL,
      .cb_size   = 0,
      .mq_mem    = NULL,
      .mq_size   = 0};

  _msg_queue_id = osMessageQueueNew(10, _msg_item_size, &msgq_attr); // 消息邮箱

  // 根据接收模式创建相应的缓冲区 (使用FreeRTOS API - 流缓冲区)
  if (_receive_mode == ReceiveMode::SINGLE_BUFFER)
  {
    _rx_stream_buffer = xStreamBufferCreate(BUFFER_SIZE, 1);
  }
  else if (_receive_mode == ReceiveMode::DOUBLE_BUFFER)
  {
    // 创建两个流缓冲区用于双缓冲机制
    _rx_stream_buffer1 = xStreamBufferCreate(BUFFER_SIZE, 1);
    _rx_stream_buffer2 = xStreamBufferCreate(BUFFER_SIZE, 1);
  }

  if (_receive_mode == ReceiveMode::LATEST_ONLY)
  {
    _latest_data_size = LATEST_DATA_SIZE;
    // 使用预定义的缓冲区，无需动态分配
  }

  if(_transmit_enable)
  {
    // 创建发送流缓冲区 (使用FreeRTOS API - 流缓冲区)
    _tx_stream_buffer = xStreamBufferCreate(BUFFER_SIZE, 1);
  }

  // 启动接收
  startReception();
}

// 析构函数实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::~SerialDriver()
{
  stopReception();

  if (_mutex_id != NULL)
  {
    osMutexDelete(_mutex_id);
  }

  if (_msg_queue_id != NULL)
  {
    osMessageQueueDelete(_msg_queue_id);
  }

  if (_rx_stream_buffer != NULL && _receive_mode == ReceiveMode::SINGLE_BUFFER)
  {
    vStreamBufferDelete(_rx_stream_buffer);
  }

  if (_rx_stream_buffer1 != NULL && _receive_mode == ReceiveMode::DOUBLE_BUFFER)
  {
    vStreamBufferDelete(_rx_stream_buffer1);
  }

  if (_rx_stream_buffer2 != NULL && _receive_mode == ReceiveMode::DOUBLE_BUFFER)
  {
    vStreamBufferDelete(_rx_stream_buffer2);
  }

  if (_tx_stream_buffer != NULL)
  {
    vStreamBufferDelete(_tx_stream_buffer);
  }
}

// 发送数据实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
int SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::sendData(const uint8_t *data, size_t size, uint32_t timeout)
{
  if(!_transmit_enable)
  {
    return -1; // 没使能发送 返回错误
  }

  osStatus_t status = osMutexAcquire(_mutex_id, timeout);
  if (status != osOK)
  {
    return -1; // 获取互斥锁失败
  }

  // 将数据写入发送流缓冲区
  size_t bytes_written = xStreamBufferSend(_tx_stream_buffer, data, size, 0);

  // 如果发送缓冲区中有数据，启动发送
  if (bytes_written > 0)
  {
    startTransmission();
  }

  osMutexRelease(_mutex_id);
  return bytes_written;
}

// 接收数据实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
int SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::receiveData(uint8_t *buffer, size_t size, uint32_t timeout)
{
  if (_receive_mode == ReceiveMode::LATEST_ONLY)
  {
    // 在LATEST_ONLY模式下，复制最新的数据
    osStatus_t status = osMutexAcquire(_mutex_id, timeout);
    if (status != osOK)
    {
      return -1;
    }

    // 这里简化处理，实际应用中需要更复杂的同步机制
    size_t copy_size = (size < _latest_data_size) ? size : _latest_data_size;
    memcpy(buffer, _latest_data, copy_size);

    osMutexRelease(_mutex_id);
    return copy_size;
  }
  else if (_receive_mode == ReceiveMode::SINGLE_BUFFER)
  {
    // 从流缓冲区读取数据
    size_t bytes_read = xStreamBufferReceive(_rx_stream_buffer, buffer, size, timeout);
    return bytes_read;
  }
  else if (_receive_mode == ReceiveMode::DOUBLE_BUFFER)
  {
    // 从当前活动的流缓冲区读取数据
    StreamBufferHandle_t active_buffer = _current_buffer ? _rx_stream_buffer2 : _rx_stream_buffer1;
    size_t               bytes_read    = xStreamBufferReceive(active_buffer, buffer, size, timeout);
    return bytes_read;
  }

  return -1; // 未知模式
}

// 获取传感器数据实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
osStatus_t SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::getSensorData(void *data, uint32_t timeout)
{
  return osMessageQueueGet(_msg_queue_id, data, NULL, timeout);
}

// 获取发送缓冲区剩余空间实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
size_t SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::getTxFreeSpace()
{ 
    return xStreamBufferSpacesAvailable(_tx_stream_buffer); 
}

// 获取接收缓冲区可用数据量实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
size_t SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::getRxAvailableData()
{
  if (_receive_mode == ReceiveMode::SINGLE_BUFFER)
  {
    return xStreamBufferBytesAvailable(_rx_stream_buffer);
  }
  else if (_receive_mode == ReceiveMode::DOUBLE_BUFFER)
  {
    // 返回两个流缓冲区中的总数据量
    return xStreamBufferBytesAvailable(_rx_stream_buffer1) + xStreamBufferBytesAvailable(_rx_stream_buffer2);
  }
  return 0;
}

// 开始接收数据实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::startReception()
{
  if (_receive_mode == ReceiveMode::DOUBLE_BUFFER)
  {
    // 双流缓冲区模式下启动接收 - 使用临时DMA缓冲区
    HAL_UART_Receive_DMA(_huart, (uint8_t *)_rx_dma_buffer, 1);
    _current_buffer = false;
  }
  else
  {
    // 单缓冲模式下启动接收 - 使用临时DMA缓冲区
    HAL_UART_Receive_DMA(_huart, (uint8_t *)_rx_dma_buffer, 1);
  }
  _rx_active = true;
}

// 停止接收数据实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::stopReception()
{
  _rx_active = false;
  HAL_UART_DMAStop(_huart);
}

// 开始传输数据实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::startTransmission()
{
  if (!isTransmitting())
  {
    // 临时缓冲区用于发送单个字节
    uint8_t temp_tx_buf[1];
    // 从发送缓冲区获取数据准备发送
    size_t bytes_to_send = xStreamBufferReceive(_tx_stream_buffer, temp_tx_buf, 1, 0);
    if (bytes_to_send > 0)
    {
      HAL_UART_Transmit_DMA(_huart, temp_tx_buf, bytes_to_send);
    }
  }
}

// 检查是否正在传输实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
bool SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::isTransmitting()
{ 
    return (_huart->gState == HAL_UART_STATE_BUSY_TX); 
}

// DMA传输完成回调函数实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::dmaTransferCompleteCallback(UART_HandleTypeDef *huart)
{
  if (huart->gState == HAL_UART_STATE_BUSY_RX)
  {
    // 接收完成处理
    handleReceiveComplete();
  }
  else if (huart->gState == HAL_UART_STATE_BUSY_TX)
  {
    // 发送完成处理
    handleTransmitComplete();
  }
}

// DMA错误回调函数实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::dmaErrorCallback(UART_HandleTypeDef *huart)
{
  // 记录错误状态并尝试恢复
  handleDmaError();
}

// 处理接收完成事件实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::handleReceiveComplete()
{
  if (_receive_mode == ReceiveMode::LATEST_ONLY)
  {
    // 在LATEST_ONLY模式下，将接收到的数据保存为最新数据
    osStatus_t status = osMutexAcquire(_mutex_id, 0); // Non-blocking
    if (status == osOK)
    {
      // 这里简化处理，实际应用中需要根据具体协议判断数据边界
      memcpy(_latest_data, _huart->pRxBuffPtr, 1); // 假设只接收一个字节
      osMutexRelease(_mutex_id);
    }
  }
  else if (_receive_mode == ReceiveMode::SINGLE_BUFFER)
  {
    // 将数据写入接收流缓冲区
    uint8_t received_byte = *(_huart->pRxBuffPtr);
    xStreamBufferSend(_rx_stream_buffer, &received_byte, 1, 0);

    // 重新启动接收 - 使用内部缓冲区
    HAL_UART_Receive_DMA(_huart, (uint8_t *)_rx_dma_buffer, 1);
  }
  else if (_receive_mode == ReceiveMode::DOUBLE_BUFFER)
  {
    // 双流缓冲区处理
    uint8_t received_byte = *(_huart->pRxBuffPtr);

    // 将数据写入当前流缓冲区
    StreamBufferHandle_t target_buffer = _current_buffer ? _rx_stream_buffer2 : _rx_stream_buffer1;
    xStreamBufferSend(target_buffer, &received_byte, 1, 0);

    // 切换到另一个流缓冲区
    _current_buffer = !_current_buffer;

    // 重启DMA接收
    HAL_UART_Receive_DMA(_huart, (uint8_t *)_rx_dma_buffer, 1);
  }
}

// 处理发送完成事件实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::handleTransmitComplete()
{
  // 检查是否还有更多数据需要发送
  if (xStreamBufferSpacesAvailable(_tx_stream_buffer) < BUFFER_SIZE)
  {
    // 有更多数据待发送，继续发送过程
    startTransmission();
  }
}

// 处理DMA错误实现
template <size_t BUFFER_SIZE, size_t LATEST_DATA_SIZE, size_t MSG_SIZE>
void SerialDriver<BUFFER_SIZE, LATEST_DATA_SIZE, MSG_SIZE>::handleDmaError()
{
  // 停止当前传输
  HAL_UART_DMAStop(_huart);

  // 记录错误日志（实际应用中应使用适当的日志系统）
  // Error_Handler(); // 或其他错误处理逻辑

  // 尝试重启接收
  if (_rx_active)
  {
    startReception();
  }
}

// C语言回调函数实现
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart == &huart1)
  {
    g_serial_driver_uart1.dmaTransferCompleteCallback(huart);
  }
  else if(huart == &huart2)
  {
    g_serial_driver_uart2.dmaTransferCompleteCallback(huart);
  }
  else if(huart == &huart3)
  {
    g_serial_driver_uart3.dmaTransferCompleteCallback(huart);
  }
  else if(huart == &huart6)
  {
    g_serial_driver_uart6.dmaTransferCompleteCallback(huart);
  }
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart == &huart1)
  {
    g_serial_driver_uart1.dmaTransferCompleteCallback(huart);
  }
  else if(huart == &huart2)
  {
    g_serial_driver_uart2.dmaTransferCompleteCallback(huart);
  }
  else if(huart == &huart3)
  {
    g_serial_driver_uart3.dmaTransferCompleteCallback(huart);
  }
  else if(huart == &huart6)
  {
    g_serial_driver_uart6.dmaTransferCompleteCallback(huart);
  }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if(huart == &huart1)
  {
    g_serial_driver_uart1.dmaErrorCallback(huart);
  }
  else if(huart == &huart2)
  {
    g_serial_driver_uart2.dmaErrorCallback(huart);
  }
  else if(huart == &huart3)
  {
    g_serial_driver_uart3.dmaErrorCallback(huart);
  }
  else if(huart == &huart6)
  {
    g_serial_driver_uart6.dmaErrorCallback(huart);
  }
}