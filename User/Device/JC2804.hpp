/**
 * @file jc2804.cpp
 * @author Rh
 * @brief 这是一个俱瓷科技的jc2804的电机驱动
 *        使用的是原本封装好的bsp_can和FreeRTOS
 *        多数功能已经测试，功能90%完善
 *        适用于电赛云台，价格100左右
 *        目前只测试了写数据，没读数据
 * @version 0.1
 * @date 2026-02-13
 *
 * @todo 感觉半辈子用不上他的返回报文功能，直接发就行，用视觉闭环。
 *       电机内部会自己闭环
 *
 * @copyright Copyright (c) 2026
 *
 *
 * @brief 使用示例：
 *
 * @note 初始化
 *
 *   jc2804 motor_yaw(&bsp_can1, 2);    // 创建类对象（提前创建好bsp）
 *   motor_yaw.init();                  // 初始化电机 在内核初始化之后使用
 *   motor_yaw.on_can_message(&rx_msg); // 在canrx回调处理任务中调用
 *
 * @note 任务调用，大概时间间隔要自己测，2ms一次发送数据是极限
 *
 *   motor_yaw.enter_closed_loop(); // 进入闭环模式
 *   osDelay(100);
 *   motor_yaw.set_control_mode(1); // 进入速度模式
 *   osDelay(100);
 *   motor_yaw.set_speed(0);        // 设置速度0 防止疯
 *   osDelay(100);
 *
 *   motor_yaw.set_speed(100);      // 设置速度100 正常操控 其他模式同理
 *
 */
#ifndef __JC2804_HPP__
#define __JC2804_HPP__


#include "bsp_can.hpp"


/* 电机数据结构体 */
struct MotorData
{
  float    voltage     = 0.0f;
  float    current     = 0.0f;
  float    speed       = 0.0f;
  float    position    = 0.0f;
  float    driver_temp = 0.0f;
  float    motor_temp  = 0.0f;
  uint32_t error_info  = 0;
};


/* 类定义 */
class jc2804
{
public:
  /* 构造与初始化 */

  // 构造函数
  jc2804(bsp_can* can_interface, uint8_t device_id);

  // 析构函数
  ~jc2804();

  /* 公有成员变量（外部要使用 懒得封装） */

  uint32_t _device_id;


  /* 控制指令（写入） */

  // 设置扭矩（电流）
  void set_torque(float torque);

  // 设置速度
  void set_speed(float speed);

  // 设置绝对位置
  void set_absolute_position(float position);

  // 设置相对位置
  void set_relative_position(float position);

  // 低速模式下设置低速
  void set_low_speed(float speed);

  /**
   * @brief 速度位置控制（不知道哪个模式可以用）
   * @param position 位置值
   * @param speed 速度值
   */
  void pv_command(int32_t position, float speed);

  /**
   * @brief 速度位置力矩模式
   * @param position 位置值
   * @param speed 速度值
   * @param torque_percent 力矩百分比
   */
  void pvt_command(int32_t position, float speed, float torque_percent);

  /**
   * @brief 设置控制模式
   * @param mode 模式值
   * 0 力矩
   * 1 速度
   * 2 位置梯形
   * 3 位置滤波
   * 4 位置直通
   * 5 低速
   */
  void set_control_mode(uint8_t mode);


  /* 系统控制指令 */

  // 系统空闲
  void idle();

  // 进入闭环模式
  void enter_closed_loop();

  // 擦除数据
  void erase();

  // 保存数据
  void save();

  // 重启
  void restart();

  // 设置零点
  void set_origin();

  // 设置临时零点
  void set_temporary_origin();


  /* 读取请求（触发） */

  // 读取电压
  void request_power_voltage();

  // 读取？
  void request_bus_current();

  // 读取实时速度
  void request_real_time_speed();

  // 读取实时位置
  void request_real_time_position();

  // 读取驱动器温度
  void request_driver_temperature();

  // 读取电机温度
  void request_motor_temperature();

  // 读取错误日志
  void request_error_info();


  /* 获取最新数据（返回结构体） */

  // 获取最新数据（返回结构体）
  MotorData get_latest_data_struct();


  /* CAN消息回调接口 */

  // CAN回调调用内容
  void on_can_message(can_rx_msg_t* rx_msg);

private:
  /* 静态常量 */

  static const float VOLTAGE_SCALE;
  static const float CURRENT_SCALE;
  static const float SPEED_SCALE;
  static const float POSITION_SCALE;
  static const float TEMPERATURE_SCALE;


  /* 成员变量 */

  /* 接收类型枚举 */
  enum RequestType
  {
    NONE_REQUEST = 0,
    VOLTAGE_REQUEST,
    CURRENT_REQUEST,
    SPEED_REQUEST,
    POSITION_REQUEST,
    DRIVER_TEMP_REQUEST,
    MOTOR_TEMP_REQUEST,
    ERROR_INFO_REQUEST
  };

  RequestType     _last_request_type;
  bsp_can*       _can;
  MotorData       latest_data; // 最新数据存储（使用结构体）


  /* 私有辅助函数 */

  // 异步发送函数
  void send_async_command(uint8_t cmd, const uint8_t* data, uint8_t len);

  /**
   * @brief 用于发送请求+记录请求类型，以便后续解析响应
   * @param cmd 命令
   * @param reg_addr 寄存器地址
   * @param req_type 请求类型
   */
  void send_read_request(uint8_t cmd, uint16_t reg_addr, RequestType req_type); // 新增

  /**
   * @brief 响应验证
   * @param expected_cmd 期望命令
   * @param rx_msg 接收消息指针
   * @return 验证结果
   */
  bool validate_response(uint8_t expected_cmd, can_rx_msg_t* rx_msg);

  // 解析数据
  void store_received_data(uint8_t* data);
};


/* 全局变量声明处 */

extern jc2804 motor_pitch;
extern jc2804 motor_yaw;


#endif // __JC2804_HPP__