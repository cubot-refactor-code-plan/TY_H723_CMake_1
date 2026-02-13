#ifndef __JC2804_HPP__
#define __JC2804_HPP__


#include "bsp_can.hpp"
#include "cmsis_os2.h"


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
class JC2804
{
public:
  /* 构造与初始化 */

  // 构造函数
  JC2804(bsp_can* can_interface, uint8_t device_id);

  // 析构函数
  ~JC2804();

  // 初始化，在FreeRTOS内核初始化之后
  void init();

  /* 公有成员变量（外部要使用 懒得封装） */

  uint8_t _device_id;


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

  RequestType _last_request_type;
  bsp_can*    _can;
  char        mutex_name[32]; // 实例互斥锁的名字，用于调试时看到名字
  MotorData   latest_data;    // 最新数据存储（使用结构体）
  osMutexId_t _mutex_id;      // 互斥锁id


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


  /* 互斥锁操作辅助函数 */

  // 加锁
  inline void lock()
  {
    if (_mutex_id)
      osMutexAcquire(_mutex_id, osWaitForever);
  }

  // 解锁
  inline void unlock()
  {
    if (_mutex_id)
      osMutexRelease(_mutex_id);
  }
};


/* 全局变量声明处 */

extern JC2804 motor_pitch;
extern JC2804 motor_yaw;


#endif // __JC2804_HPP__