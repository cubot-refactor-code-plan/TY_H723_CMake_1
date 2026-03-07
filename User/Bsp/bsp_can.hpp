#ifndef __BSP_CAN_HPP__
#define __BSP_CAN_HPP__

#include "fdcan.h"    // IWYU pragma: keep
#include "cmsis_os2.h" // IWYU pragma: keep
#include "stm32h7xx_hal_fdcan.h"
#include <cstdint>


// CAM接收信息结构体接口
struct can_rx_msg_t
{
  FDCAN_RxHeaderTypeDef header;
  uint8_t               data[8];
};

struct can_tx_msg_t
{
  FDCAN_TxHeaderTypeDef header;
  uint8_t               data[8];
};


// CAN类
class BspCan
{
public:
  osMessageQueueId_t rxQueueHandle; // CMSIS_OS2的队列
  osMutexId_t        txMutexHandle; // CMSIS_OS2的互斥锁

  // 构造函数，增加instanceId参数用于区分不同的实例
  BspCan(FDCAN_HandleTypeDef* hfdcan, int instanceId);

  // 析构函数
  ~BspCan();

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

extern BspCan bsp_can1;
extern BspCan bsp_can2;

// 一条can线上的can设备数上限
#define MAX_DIVICE_IN_ONE_CAN 10 

/**
 * @brief 抽象类CanItem定义了任何通过Can通信运行的对象
 * 
 */
class CanItem 
{
protected:
  BspCan* bsp_can;
  can_rx_msg_t rxMsg;
  uint32_t canId; // 该设备监听的CAN ID
public:
  CanItem(BspCan* can) ;
  void receive(can_rx_msg_t rxMsg);
  virtual void callback(uint8_t* data) = 0; // 正确接收到数据后执行的回调函数，纯虚函数需要子类实现
  virtual void send(uint8_t* data, uint8_t len) = 0;
};

#endif // __BSP_CAN_HPP__