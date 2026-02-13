#ifndef __BSP_USART_HPP__
#define __BSP_USART_HPP__

#include "FreeRTOS.h" // IWYU pragma: keep
#include "usart.h"    // IWYU pragma: keep
#include "cmsis_os2.h"
#include "stream_buffer.h"


// C风格编译此部分（因为需要在stm32h7_it.c中调用）
#if __cplusplus
extern "C"
{
#endif // __cplusplus

  void HAL_BSP_UART_IRQHandler(UART_HandleTypeDef *huart);

#if __cplusplus
}
#endif // __cplusplus


// CPP才可编译此部分（因为需要在stm32h7_it.c中调用）
// 最下面有类实现的对象的extern位置，当然extern可以在别的地方写。定义在.cpp中
#if __cplusplus

// 模板的第一个数字为缓冲区大小（单位uint8_t） 第二个数字为消息队列的长度（uint8_t）
template <size_t BUFFER_SIZE = 256, size_t MSG_SIZE = 8>
class bsp_usart
{

public:
  /**
   * @brief 接收模式枚举
   *
   */
  enum class ReceiveMode
  {
    LATEST_ONLY   = 1, // 仅保留最新一次接收到的数据（使用消息邮箱）（不能开FIFO）
    SINGLE_BUFFER = 2, // 使用单个流缓冲区
    DOUBLE_BUFFER = 3  // 使用双流缓冲区机制
  };

private:
  UART_HandleTypeDef  *_huart;                      ///< UART句柄指针，指向底层硬件接口
  osMutexId_t          _mutex_id;                   ///< CMSIS-RTOS2互斥锁ID，用于线程安全访问
  osMessageQueueId_t   _msg_queue_id;               ///< CMSIS-RTOS2消息队列ID，用于LATEST_ONLY模式
  StreamBufferHandle_t _rx_stream_buffers[2];       ///< 接收流缓冲区数组，[0]为单缓冲或双缓冲第一个，[1]为双缓冲第二个
  StreamBufferHandle_t _tx_stream_buffer;           ///< FreeRTOS发送流缓冲区句柄
  ReceiveMode          _receive_mode;               ///< 接收模式，指定数据接收策略
  bool                 _rx_active;                  ///< 接收状态标志，指示是否正在接收数据
  uint8_t              _rx_dma_buffer[BUFFER_SIZE]; ///< DMA接收缓冲区，用于多字节接收
  uint8_t              _tx_dma_buffer[BUFFER_SIZE]; ///< DMA发送缓冲区，用于多字节发送
  bool                 _current_buffer;             ///< 当前使用的流缓冲区标识，true表示使用buffer2，false表示buffer1
  size_t               _buffer_size;                ///< 缓冲区大小，单位字节
  size_t               _msg_item_size;              ///< 消息队列中每个项目的大小
  bool                 _transmit_enable;            ///< 是否启用发送
  uint32_t             _last_received_length;       ///< 最后一次接收的数据长度
  int                  _instance_id;                ///< 实例ID，用于生成唯一资源名称
  char                 mutex_name[32];              ///< 实例互斥锁的名字，用于调试时看到名字
  char                 msgq_name[32];               ///< 实例消息队列的名字，用于调试时看到名字


public:
  /**
   * @brief 构造函数
   *
   * @note 初始化串口驱动对象，配置必要的FreeRTOS对象
   *
   * @param huart UART句柄指针
   * @param rx_mode 接收模式
   * @param transmit_signal 是否启用发送功能
   * @param instance_id 实例ID，用于生成唯一资源名称
   */
  bsp_usart(UART_HandleTypeDef *huart, ReceiveMode rx_mode, bool transmit_signal, int instance_id = 0);

  /**
   * @brief 初始化函数 初始化串口驱动对象，配置必要的FreeRTOS对象
   *
   * @return true 初始化成功
   * @return false 初始化失败
   */
  bool init();

  // 析构函数 释放所有分配的资源
  ~bsp_usart();

  /**
   * @brief 发送数据 将数据放入发送缓冲区，并启动DMA传输
   *
   * @param data 要发送的数据指针
   * @param size 数据大小
   * @param timeout 超时时间（ticks）
   * @return int 返回发送的数据字节数，负值表示错误
   */
  int sendData(const uint8_t *data, size_t size, uint32_t timeout = osWaitForever);

  /**
   * @brief 接收数据 根据接收模式从相应的缓冲区读取数据
   *
   * @param buffer 接收数据的缓冲区
   * @param size 请求读取的数据大小
   * @param timeout 超时时间（ticks）
   * @return int 实际读取的数据字节数，-1表示超时或无数据
   */
  int receiveData(uint8_t *buffer, size_t size, uint32_t timeout = osWaitForever);

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
   * @brief DMA传输完成回调函数 由HAL库调用，处理DMA传输完成事件
   *
   * @param huart UART句柄
   */
  void dmaTransferCompleteCallback(UART_HandleTypeDef *huart);

  /**
   * @brief DMA错误回调函数 由HAL库调用，处理DMA错误事件
   *
   * @param huart UART句柄
   */
  void dmaErrorCallback(UART_HandleTypeDef *huart);

  /**
   * @brief IDLE中断处理函数 处理由IDLE中断检测到的数据包
   *
   * @param received_length 接收到的数据长度
   */
  void handleIdleInterrupt(uint32_t received_length);

  /**
   * @brief 内部IDLE中断处理函数 内部处理IDLE中断，自动计算接收到的数据长度
   *
   * @param huart UART句柄
   */
  void handleIdleInterruptInternal(UART_HandleTypeDef *huart);

private:
  // 开始接收数据 启动DMA接收
  void startReception();

  // 清理资源 清理所有分配的资源
  void cleanupResources();

  // 停止接收数据 停止DMA接收
  void stopReception();

  // 开始传输数据 启动DMA发送
  void startTransmission();

  /**
   * @brief 检查是否正在传输
   *
   * @return true 正在传输
   * @return false 未在传输
   */
  bool isTransmitting();

  // 处理接收完成事件 根据接收模式处理接收到的数据
  void handleReceiveComplete();

  // 处理发送完成事件 继续发送剩余数据或结束发送过程
  void handleTransmitComplete();

  // 处理DMA错误 记录错误并尝试重新初始化
  void handleDmaError();
};


// 外部声明这些类实例化的对象

extern bsp_usart<256, 8> bsp_usart6;

#endif // __cplusplus


#endif // __BSP_USART_HPP__