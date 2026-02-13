#ifndef __BSP_CAN_HPP__
#define __BSP_CAN_HPP__

#include "fdcan.h"    // IWYU pragma: keep
#include "cmsis_os.h" // IWYU pragma: keep


// CAM接收信息结构体接口
typedef struct
{
  FDCAN_RxHeaderTypeDef header;
  uint8_t               data[8];
} can_rx_msg_t;


// CAN类
class bsp_can
{
public:
  osMessageQueueId_t rxQueueHandle; // CMSIS_OS2的队列
  osMutexId_t        txMutexHandle; // CMSIS_OS2的互斥锁

  // 构造函数，增加instanceId参数用于区分不同的实例
  bsp_can(FDCAN_HandleTypeDef* hfdcan, int instanceId);

  // 析构函数
  ~bsp_can();

  // 启动 FDCAN 并配置过滤器
  HAL_StatusTypeDef init();

  // 发送标准数据帧
  HAL_StatusTypeDef send(uint32_t stdId, uint8_t* pData, uint8_t len);

  // 从队列接收数据
  osStatus_t receive(can_rx_msg_t* msg, uint32_t timeout);

private:
  FDCAN_HandleTypeDef* _hfdcan;        // CAN句柄
  int                  _instanceId;    // 实例ID，
  char                 queue_name[32]; // 实例队列的名字，用于调试时看到名字
  char                 mutex_name[32]; // 实例互斥锁的名字，用于调试时看到名字
};


// 外部声明这些类实例化的对象

extern bsp_can bsp_can1;


#endif // __BSP_CAN_HPP__