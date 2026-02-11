#ifndef __JC2804_HPP__
#define __JC2804_HPP__

#include "bsp_can.hpp"

class JC2804
{
public:
  // 构造函数，需要传入 bsp_can 对象
  JC2804(bsp_can& can_driver, int offset_id);

  // 读取电源电压 (单位: V)
  float read_power_voltage();

  // 读取母线电流 (单位: A)
  float read_bus_current();

  // 读取实时速度 (单位: rpm)
  float read_real_time_speed();

  // 读取实时位置 (单位: 度)
  float read_real_time_position();

  // 读取驱动器温度 (单位: °C)
  float read_driver_temperature();

  // 读取电机温度 (单位: °C)
  float read_motor_temperature();

  // 读取错误信息
  uint32_t read_error_info();

  // 设置扭矩 (单位: Nm)
  void set_torque(float torque);

  // 设置速度 (单位: rpm)
  void set_speed(float speed);

  // 设置绝对位置 (单位: 度)
  void set_absolute_position(float position);

  // 设置相对位置 (单位: 度)
  void set_relative_position(float position);

  // 设置低速 (单位: rpm)
  void set_low_speed(float speed);

  // PV指令（以速度v转到位置p）
  void pv_command(int32_t position, float speed);

  // PVT指令（以速度v力矩t转到位置p）
  void pvt_command(int32_t position, float speed, float torque_percent);

  
  /**
   * @brief 切换控制模式
   *
   * 0 力矩模式
   * 1 速度模式
   * 2 位置梯形轨迹
   * 3 位置滤波模式
   * 4 位置直通模式
   * 5 低速大扭模式
   */
  void set_control_mode(uint8_t mode);

  // 空闲状态
  void idle();

  // 进入闭环
  void enter_closed_loop();

  // 擦除
  void erase();

  // 保存
  void save();

  // 重启
  void restart();

  // 设置原点
  void set_origin();

  // 设置临时原点
  void set_temporary_origin();

private:
  // 发送指令并等待回复
  /**
   * @brief 
   * 
   * @param command data数组的第一个命令
   * @param data 发送命令的数组
   * @param len  命令长度
   * @param response 报文
   * @param response_len 报文长度
   * @return true 收发命令成功
   * @return false 收发命令失败
   */
  bool send_command(uint8_t command, uint8_t* data, uint8_t len, uint8_t* response, uint8_t response_len);

  // 驱动器ID
  uint8_t _device_id;

  // CAN驱动
  bsp_can& _can_driver;

  // 通信超时时间（毫秒）
  static const uint32_t COMM_TIMEOUT = 10;

  // 数据缩放因子
  static const float VOLTAGE_SCALE;     // 10倍放大
  static const float CURRENT_SCALE;     // 100倍放大
  static const float SPEED_SCALE;       // 100倍放大
  static const float POSITION_SCALE;    // 100倍放大
  static const float TEMPERATURE_SCALE; // 10倍放大
};


/* 外部声明类对象 */

extern JC2804 motor_yaw;
extern JC2804 motor_pitch;


#endif // __JC2804_HPP__