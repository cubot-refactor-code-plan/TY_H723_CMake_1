// uart.hpp
#ifndef UART_HPP
#define UART_HPP

#include "main.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "queue.h"
#include "semphr.h"

/**
 * @brief 串口驱动组件类
 *
 * 该类封装了基于STM32 HAL库、FreeRTOS和CMSIS_OS2的串口驱动功能，
 * 支持DMA传输、流缓冲区管理、互斥锁保护和错误处理等功能。
 */
template <size_t BUFFER_SIZE = 256, size_t LATEST_DATA_SIZE = 64, size_t MSG_SIZE = sizeof(uint32_t)>
class SerialDriver
{
public:
  /**
   * @brief 接收模式枚举
   *
   */
  enum class ReceiveMode
  {
    LATEST_ONLY,   // 仅保留最新一次接收到的数据
    SINGLE_BUFFER, // 使用单个流缓冲区
    DOUBLE_BUFFER  // 使用双流缓冲区机制
  };

private:
  UART_HandleTypeDef  *_huart;                         ///< UART句柄指针，指向底层硬件接口
  osMutexId_t          _mutex_id;                      ///< CMSIS-RTOS2互斥锁ID，用于线程安全访问
  osMessageQueueId_t   _msg_queue_id;                  ///< CMSIS-RTOS2消息队列ID，用于传感器数据传递
  StreamBufferHandle_t _rx_stream_buffer;              ///< FreeRTOS接收流缓冲区句柄（单缓冲模式）
  StreamBufferHandle_t _rx_stream_buffer1;             ///< FreeRTOS接收流缓冲区句柄1（双缓冲模式）
  StreamBufferHandle_t _rx_stream_buffer2;             ///< FreeRTOS接收流缓冲区句柄2（双缓冲模式）
  StreamBufferHandle_t _tx_stream_buffer;              ///< FreeRTOS发送流缓冲区句柄
  ReceiveMode          _receive_mode;                  ///< 接收模式，指定数据接收策略
  uint8_t              _latest_data[LATEST_DATA_SIZE]; ///< 最新数据存储缓冲区
  size_t               _latest_data_size;              ///< 最新数据缓冲区大小
  bool                 _rx_active;                     ///< 接收状态标志，指示是否正在接收数据
  uint8_t              _rx_dma_buffer[1];              ///< DMA接收临时缓冲区
  bool                 _current_buffer;                ///< 当前使用的流缓冲区标识，true表示使用buffer2，false表示buffer1
  size_t               _buffer_size;                   ///< 缓冲区大小，单位字节
  size_t               _msg_item_size;                 ///< 消息队列中每个项目的大小
  bool                 _transmit_enable;               ///< 是否启用发送

public:
  /**
   * @brief 构造函数
   *
   * 初始化串口驱动对象，配置必要的FreeRTOS对象
   *
   * @param huart UART句柄指针
   * @param rx_mode 接收模式
   * @param transmit_signal 是否启用发送功能
   */
  SerialDriver(UART_HandleTypeDef *huart, ReceiveMode rx_mode, bool transmit_signal);

  /**
   * @brief 析构函数
   *
   * 释放所有分配的资源
   */
  ~SerialDriver();

  /**
   * @brief 发送数据
   *
   * 将数据放入发送缓冲区，并启动DMA传输
   *
   * @param data 要发送的数据指针
   * @param size 数据大小
   * @param timeout 超时时间（ticks）
   * @return int 返回发送的数据字节数，负值表示错误
   */
  int sendData(const uint8_t *data, size_t size, uint32_t timeout = osWaitForever);

  /**
   * @brief 接收数据
   *
   * 根据接收模式从相应的缓冲区读取数据
   *
   * @param buffer 接收数据的缓冲区
   * @param size 请求读取的数据大小
   * @param timeout 超时时间（ticks）
   * @return int 实际读取的数据字节数，-1表示超时或无数据
   */
  int receiveData(uint8_t *buffer, size_t size, uint32_t timeout = osWaitForever);

  /**
   * @brief 从消息队列获取传感器数据
   *
   * @param data 存储传感器数据的指针
   * @param timeout 超时时间（ticks）
   * @return osStatus_t 操作结果
   */
  osStatus_t getSensorData(void *data, uint32_t timeout = osWaitForever);

  /**
   * @brief 获取发送缓冲区剩余空间
   *
   * @return size_t 剩余空间大小
   */
  size_t getTxFreeSpace();

  /**
   * @brief 获取接收缓冲区可用数据量
   *
   * @return size_t 可用数据量
   */
  size_t getRxAvailableData();

  /**
   * @brief DMA传输完成回调函数
   *
   * 由HAL库调用，处理DMA传输完成事件
   *
   * @param huart UART句柄
   */
  void dmaTransferCompleteCallback(UART_HandleTypeDef *huart);

  /**
   * @brief DMA错误回调函数
   *
   * 由HAL库调用，处理DMA错误事件
   *
   * @param huart UART句柄
   */
  void dmaErrorCallback(UART_HandleTypeDef *huart);

private:
  /**
   * @brief 开始接收数据
   *
   * 启动DMA接收
   */
  void startReception();

  /**
   * @brief 停止接收数据
   *
   * 停止DMA接收
   */
  void stopReception();

  /**
   * @brief 开始传输数据
   *
   * 启动DMA发送
   */
  void startTransmission();

  /**
   * @brief 检查是否正在传输
   *
   * @return true 正在传输
   * @return false 未在传输
   */
  bool isTransmitting();

  /**
   * @brief 处理接收完成事件
   *
   * 根据接收模式处理接收到的数据
   */
  void handleReceiveComplete();

  /**
   * @brief 处理发送完成事件
   *
   * 继续发送剩余数据或结束发送过程
   */
  void handleTransmitComplete();

  /**
   * @brief 处理DMA错误
   *
   * 记录错误并尝试重新初始化
   */
  void handleDmaError();
};

#endif // UART_HPP