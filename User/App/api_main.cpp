/**
 * @file api_main.cpp
 * @author Rh
 * @brief 应用层与stm32的接口，放着最直接运行的任务
 * @version 0.1
 * @date 2026-01-20
 *
 * @note 各个外设的中断回调函数，放到了各个的BSP中实现，方便查找
 *       对于中断得到的数据，在这里创建任务等待消息队列 + 处理就可以
 *
 * @copyright Copyright (c) 2026
 *
 */


#include "api_main.h"
#include "cmsis_os2.h"
#include "main.h" // IWYU pragma: keep
#include "stdio.h"

/* BSP */
#include "bsp_usart.hpp"

/* DVC */
#include "dm_imu.hpp"
#include "jc2804.hpp"

/* SVC */
#include "protocol_usart.hpp"


/**
 * @brief main中初始化（无freertos）
 * @note  也就是在main.c中写了一个函数调用，这样转嫁就可以使用cpp了
 *
 */
void app_init()
{
  printf("\napp_init_ok\n");
}


/* 创建对应句柄 handle */

osThreadId_t         can_rx_task_handle; // CAN 接收后处理任务
const osThreadAttr_t can_rx_handler_task_attributes = {
  .name       = "can_rx_task",
  .stack_size = 128 * 4,
  .priority   = (osPriority_t)osPriorityNormal,
};


/**
 * @brief 和freertos有关的初始化
 * @note  也就是在main.c中的MX_FREERTOS_INIT里，写了一个函数调用，这样转嫁就可以使用cpp了
 *
 */
void freertos_init()
{
  // 初始化bsp设备
  bsp_usart6.init();
  bsp_can1.init();

  // 初始化协议
  protocal_uart_6.init(_uart_protocol_task6);
  imu_bmi088.init();

  // 创建 CAN 接收后处理任务
  can_rx_task_handle = osThreadNew(_can_rx_handler_task, nullptr, &can_rx_handler_task_attributes);

  printf("freertos_init_ok\n");
}


/**
 * 以下均为FreeRTOS的内容定义，使用c调用cpp，需要extern "C"定义，让RTOS接管
 * CubeMX提供了FreeRTOS配置，故而使用它的CMSIS_OS2
 * CMSIS_OS2封装了一层，导致很多东西和原生的FreeRTOS不一样，所以写法也会有的不一样
 * CMSIS_OS2的初始句柄不对外声明，如果想用只能单独extern出来用
 * CMSIS_OS2做了层封装，方便使用。但是原生的FreeRTOS在以后要用的时候，还是要花时间适应
 * CMSIS_OS2的默认任务只能weak声明，其他的可以使用外部声明
 * printf要加\n
 *
 * @note 在C++中使用FreeRTOS的Task函数时
 *       需要将任务函数声明为extern "C"格式
 *       同时函数参数必须是void *pvParameters。
 *       所以，我直接把任务放到一个转接文件，然后extern
 */


/**
 * @brief 默认任务
 *
 * @param argument 默认参数
 */
extern "C" void _defaultTask(void *argument)
{
  osDelay(1000);
  printf("Default Task Started\n");

  for (;;)
  {

    osDelay(10);
  }
}


// canItem注册表
extern CanItem* can1Item_ptr[MAX_DIVICE_IN_ONE_CAN];
extern CanItem* can2Item_ptr[MAX_DIVICE_IN_ONE_CAN];
// 已注册数量
extern uint8_t can1Item_num, can2Item_num;
/**
 * @brief 用于处理CAN接收后的数据处理任务
 *
 */
extern "C" void _can_rx_handler_task(void *argument)
{
  printf("CAN RX Task Started\n");
  can_rx_msg_t rx_msg;

  for (;;)
  {
    osStatus_t status = bsp_can1.receive(&rx_msg, osWaitForever);

    if (status == osOK)
    {
      // 根据 ID 判断是哪个设备
      uint32_t device_id = rx_msg.header.Identifier & 0x0F; // 0x600+ID -> ID 是低4位

      // 查找对应的 JC2804 实例
      if (device_id == motor_yaw._device_id)
      {
        motor_yaw.on_can_message(&rx_msg);
      }
      else if (device_id == motor_pitch._device_id)
      {
        motor_pitch.on_can_message(&rx_msg);
      }
      // 查找对应的dm_imu 示例
      else if (device_id == (imu_bmi088._master_id & 0x0F))
      {
        imu_bmi088.on_can_message(&rx_msg);
      }
      else {
        for (int i = 0; i < can1Item_num && can1Item_ptr[i] != nullptr; i++){
          can1Item_ptr[i]->receive(rx_msg);   // 调用can1上的所有canItem的接收回调，轮询rx_msg信息
        }
      }
    }
  }
}

// 串口协议处理函数，在串口协议处创建并定义
