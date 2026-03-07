#ifndef __DJIMOTOR_H_HPP__
#define __DJIMOTOR_H_HPP__

#include "bsp_can.hpp"
#include <cstdint>

/**
 * @brief 电机返回的初始数据，由CAN中断回调更新
 */
struct RawData_t
{
  int16_t speed_rpm;      //< 每分钟所转圈数
  int16_t torque_current; //< 实际转矩电流
  uint8_t temperature;    //< 温度
  int16_t raw_ecd;        //< 原始编码器数据
} ;

/**
 * @brief  处理后的电机动态数据，电机运行中产生的数据，由receive(can_rx_msg_t rxMsg)更新
 */
struct TreatedData_t
{
  int32_t ecd;              //< 当前编码器返回值处理值
  int32_t last_ecd;         //< 上一时刻编码器返回值处理值
  float angle;              //< 解算后的输出轴角度
  float last_angle;		      //< 上一时解算后的输出轴角
  // int32_t angle_demarcate;
  int16_t angle_speed;      //< 解算后的输出轴角速度
  int32_t round_cnt;        //< 累计转动圈数
  int16_t axis_round_cnt;   //< 累计输出轴转动圈数
  int32_t total_ecd;        //< 编码器累计增量值
  float total_angle;        //< 累计旋转角度
  float axis_total_angle;   //< 累计输出轴旋转角度
  int32_t motor_output;     //< 输出给电机的值，通常为控制电流或电压
  int16_t filter_speed_rpm; //< 滑动平均滤波器后的转速
  uint16_t fps;
} ;

/**
 * @brief   电机参数，在初始化函数中确定
 */
struct MotorParam_t
{
  uint8_t motor_id;         //< 电机ID
  uint16_t ecd_offset;      //< 电机初始零点
  uint16_t ecd_range;       //< 编码器分度值
  float reduction_ratio; //< 减速比（电机转子转多少圈输出轴输出一圈）
  int16_t current_limit;    //< 电调能承受的最大电流
} ;

class RM_Motor : public CanItem {
public:
  RawData_t rawData;
  TreatedData_t treatedData;
  MotorParam_t motorParam;

  RM_Motor(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio);
  void receive(can_rx_msg_t rxMsg) override;
  void send(uint8_t* data, uint8_t len) override;
  void filldata(int32_t output);
protected:
  can_tx_msg_t* txBuffer;
};

class Motor3508 : public RM_Motor{
public:
  Motor3508(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio);
};

enum Motor6020ControlMode{
  Voltage,
  Current,
};

class Motor6020 : public RM_Motor{
public:
  Motor6020(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio, Motor6020ControlMode mode);
private:
  Motor6020ControlMode controlMode;
};

class Motor2006 : public RM_Motor{
public:
  Motor2006(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio);
};

#endif // __DJIMOTOR_HPP__