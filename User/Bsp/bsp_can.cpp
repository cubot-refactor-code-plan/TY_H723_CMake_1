/**
 * @file bsp_can.cpp
 * @author Rh
 * @brief 实现了一个简易的CAN2.0收发驱动。
 * @version 0.1
 * @date 2026-02-11
 *
 * @todo 此处只写了CAN1，以后要是有帮忙写下CAN2,CAN3。以及CANFD的就更好了
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief 使用示例（必须要在freertos的任务中运行收发 中断中不要进行此操作 中断不能阻塞）
 *
 * @note 类的实例化 以及初始化
 *
 *   bsp_can bsp_can1(&hfdcan1); // 全局实例化类
 *   bsp_can1.init();            // 需要在freertos内核开启之后去init
 *   can_rx_msg_t can1_msg;      // 实例化一个接收消息用的msg
 *
 * @note extern好之后，在任务中使用
 *
 *   bsp_can1.send(0x602, data1, 8);           // 发送报头为0x602，数组指针为data1，长度为8（uint8_t）
 *   bsp_can1.receive(&can1_msg,osWaitForever) // 使用此结构体接收数据，等待时间无穷久，使用中断接收
 *
 */

#include "bsp_can.hpp"
#include <stdio.h>
#include <string.h>

/* 全局实例化类对象 */

bsp_can bsp_can1(&hfdcan1, 1); 


/* 中断回调函数 */

// 接收完成中断回调
extern "C" void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs)
{
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
  {
    can_rx_msg_t rxMsg;
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxMsg.header, rxMsg.data) == HAL_OK)
    {
      // 注意：CMSIS osMessageQueuePut 在 ISR 中使用时，timeout 必须为 0
      if (bsp_can1.rxQueueHandle != NULL)
      {
        osMessageQueuePut(bsp_can1.rxQueueHandle, &rxMsg, 0, 0);
      }
    }
    // 之后可拓展其他CAN......
    else
    {
    }
  }
}

// 发送完成中断回调
extern "C" void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef* hfdcan, uint32_t BufferIndexes)
{
  // 发送完成处理，这里可以根据需要添加逻辑
  // 例如，可以释放与发送相关的资源或更新状态
}


/**
 * @brief 以下是bsp_can类型的定义
 *
 * 发送接收均使用fifo
 * 接收方式采用中断
 * 目前只支持CAN2.0协议
 *
 * @note 经过测试，1ms速率收发无问题
 *
 * @param hfdcan 传入到构造函数的句柄
 */
bsp_can::bsp_can(FDCAN_HandleTypeDef* hfdcan, int instanceId) : _hfdcan(hfdcan), _instanceId(instanceId)
{
  rxQueueHandle = NULL;
  txMutexHandle = NULL;
}

// 析构函数
bsp_can::~bsp_can()
{
  // 删除消息队列
  if (rxQueueHandle != NULL)
  {
    osMessageQueueDelete(rxQueueHandle);
    rxQueueHandle = NULL;
  }

  // 删除互斥锁
  if (txMutexHandle != NULL)
  {
    osMutexDelete(txMutexHandle);
    txMutexHandle = NULL;
  }

  // 停止FDCAN外设
  if (_hfdcan != nullptr)
  {
    HAL_FDCAN_Stop(_hfdcan);
  }
}

// 初始化成员，需要在freertos内核初始化之后进行
HAL_StatusTypeDef bsp_can::init()
{
  // 创建 FreeRTOS 资源，使用实例ID作为唯一标识
  snprintf(queue_name, sizeof(queue_name), "CAN%dRx_Queue", _instanceId);

  const osMessageQueueAttr_t queue_attr =
    {
      .name      = queue_name,
      .attr_bits = 0,
      .cb_mem    = NULL,
      .cb_size   = 0,
      .mq_mem    = NULL,
      .mq_size   = 0};

  rxQueueHandle = osMessageQueueNew(16, sizeof(can_rx_msg_t), &queue_attr);

  snprintf(mutex_name, sizeof(mutex_name), "CAN%dTx_Mutex", _instanceId);

  const osMutexAttr_t mutex_attr =
    {
      .name      = mutex_name,
      .attr_bits = 0,
      .cb_mem    = NULL,
      .cb_size   = 0};

  txMutexHandle = osMutexNew(&mutex_attr);

  // 2. FDCAN 硬件配置 (过滤器等)
  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0;
  sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = 0x000;
  sFilterConfig.FilterID2    = 0x7FF;
  if (HAL_FDCAN_ConfigFilter(_hfdcan, &sFilterConfig) != HAL_OK)
  {
    return HAL_ERROR;
  }

  // 配置全局过滤器：拒绝未定义的 ID（默认全部接收）
  // 或者设置为 FDCAN_ACCEPT_IN_RX_FIFO0 以确保万无一失
  if (HAL_FDCAN_ConfigGlobalFilter(_hfdcan, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  // 3. 启动硬件
  if (HAL_FDCAN_Start(_hfdcan) != HAL_OK)
  {
    return HAL_ERROR;
  }

  // 4. 开启接收中断
  if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
  {
    return HAL_ERROR;
  }

  // 5. 开启发送完成中断
  if (HAL_FDCAN_ActivateNotification(_hfdcan, FDCAN_IT_TX_COMPLETE, 0) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/**
 * @brief CAN发送函数
 *
 * @note 只写了CAN2.0 CANFD模式大家需要再去写
 *
 * @param stdId CANID
 * @param pData 待发送数组指针
 * @param len   长度
 * @return HAL_StatusTypeDef 返回值同hal
 */
HAL_StatusTypeDef bsp_can::send(uint32_t stdId, uint8_t* pData, uint8_t len)
{
  FDCAN_TxHeaderTypeDef txHeader;

  txHeader.Identifier  = stdId;
  txHeader.IdType      = FDCAN_STANDARD_ID;
  txHeader.TxFrameType = FDCAN_DATA_FRAME;

  // 正确设置数据长度字段
  switch (len)
  {
    case 0:
      txHeader.DataLength = FDCAN_DLC_BYTES_0;
      break;
    case 1:
      txHeader.DataLength = FDCAN_DLC_BYTES_1;
      break;
    case 2:
      txHeader.DataLength = FDCAN_DLC_BYTES_2;
      break;
    case 3:
      txHeader.DataLength = FDCAN_DLC_BYTES_3;
      break;
    case 4:
      txHeader.DataLength = FDCAN_DLC_BYTES_4;
      break;
    case 5:
      txHeader.DataLength = FDCAN_DLC_BYTES_5;
      break;
    case 6:
      txHeader.DataLength = FDCAN_DLC_BYTES_6;
      break;
    case 7:
      txHeader.DataLength = FDCAN_DLC_BYTES_7;
      break;
    case 8:
      txHeader.DataLength = FDCAN_DLC_BYTES_8;
      break;
    default:
      // 对于超出范围的长度，限制为8字节，或者其他模式，可以去自定义
      txHeader.DataLength = FDCAN_DLC_BYTES_8;
      break;
  }

  txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
  txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
  txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  txHeader.MessageMarker       = stdId; // 使用ID作为消息标记

  // 使用互斥锁防止多任务同时写发送寄存器
  if (txMutexHandle != NULL)
  {
    osMutexAcquire(txMutexHandle, osWaitForever);
  }

  // 检查 FIFO 是否有空间，如果有这个问题的去加大发送区域fifo
  // 测试：1ms发两次数据
  // 结果：未进入等待，完全够用
  uint32_t       timeoutCount = 0;
  const uint32_t maxTimeout   = 100; // 最大等待时间
  while (HAL_FDCAN_GetTxFifoFreeLevel(_hfdcan) == 0)
  {
    if (++timeoutCount > maxTimeout)
    {
      if (txMutexHandle != NULL)
      {
        osMutexRelease(txMutexHandle);
      }
      return HAL_TIMEOUT; // 超时返回
    }
    osDelay(1); // 简单退避，也可配合中断信号
  }

  HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(_hfdcan, &txHeader, pData);

  if (txMutexHandle != NULL)
  {
    osMutexRelease(txMutexHandle);
  }
  return status;
}


/**
 * @brief can接收函数，在中断中读取，此处为获得存入的消息队列的数据
 *
 * @note 使用接收完成中断测试，接收1ms1次的报文，无问题，读取无误
 *       消息队列长度单位为：can_rx_msg_t的大小，可缓存16个数据
 *
 * @param msg 接收结构体指针
 * @param timeout 等待数据的时间
 * @return osStatus_t CMSIS_OS2的返回值
 */
osStatus_t bsp_can::receive(can_rx_msg_t* msg, uint32_t timeout)
{
  // 封装 CMSIS 队列接收
  if (rxQueueHandle == NULL)
  {
    return osErrorResource;
  }
  return osMessageQueueGet(rxQueueHandle, msg, NULL, timeout);
}
