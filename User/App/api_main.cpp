/**
 * @file api_main.cpp
 * @author Rh
 * @brief 应用层与stm32的接口，放着最直接运行的函数
 * @version 0.1
 * @date 2026-01-20
 *
 * @copyright Copyright (c) 2026
 *
 */


#include "api_main.h"
#include "main.h" // IWYU pragma: keep

#include "stdio.h"
#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"
#include "cmsis_os2.h"

#include "bsp_usart.hpp"
#include "JC2804.hpp"


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
osThreadId_t can_rx_task_handle; // CAN 接收后处理任务


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


  // 初始化云台双电机
  motor_yaw.init();
  motor_pitch.init();


  // 创建 CAN 接收后处理任务 局部变量 传入就好
  const osThreadAttr_t can_rx_handler_task_attributes = {
    .name       = "can_rx_task",
    .stack_size = 128 * 4,
    .priority   = (osPriority_t)osPriorityNormal,
  };
  can_rx_task_handle = osThreadNew(_can_rx_handler_task, nullptr, &can_rx_handler_task_attributes);


  printf("freertos_init_ok\n");
}


/**
 * 以下均为FreeRTOS的内容定义，使用c调用cpp，需要extern "C"定义，让RTOS接管
 * CubeMX提供了FreeRTOS配置，故而使用它的CMSIS_OS2
 * CMSIS_OS2封装了一层，导致很多东西和原生的FreeRTOS不一样，所以写法也会有的不一样
 * CMSIS_OS2的初始句柄不对外声明，如果想用只能extern出来用
 * CMSIS_OS2做了层封装，方便使用。但是原生的FreeRTOS在以后要用的时候，还是要花时间适应
 * CMSIS_OS2的默认任务只能weak声明，其他的可以使用外部声明
 * printf要加\n
 *
 * @note 在C++中使用FreeRTOS的Task函数时
 *       需要将任务函数声明为extern "C"格式
 *       同时函数参数必须是void *pvParameters。
 *       所以，我直接把任务放到一个转接文件，然后extern
 */

/* 声明需要使用的句柄 */

/* CMSIS_OS2中使用的是声明，我只需要外部定义同名的函数，然后exteren "C"即可 */


/**
 * @brief 默认任务
 *
 * @param argument 默认参数
 */
extern "C" void _defaultTask(void *argument)
{
  motor_yaw.enter_closed_loop();
  osDelay(100);
  motor_yaw.set_control_mode(1);
  osDelay(100);
  // motor_yaw.set_speed(0);
  // osDelay(100);

  for (;;)
  {
    motor_yaw.set_speed(30);
    osDelay(3);
    motor_yaw.set_speed(-30);
    osDelay(3);
  }
}


/**
 * @brief 用于处理CAN接收后的数据处理
 *
 */
// api_main.cpp
extern "C" void _can_rx_handler_task(void *argument)
{
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
    }
  }
}