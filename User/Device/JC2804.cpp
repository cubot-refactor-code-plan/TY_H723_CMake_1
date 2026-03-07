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
 * @copyright Copyright (c) 2026
 *
 */

/**
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

#include "jc2804.hpp"


/* 全局类成员实例化 实例化需要传入can对象和偏移id */

jc2804 motor_yaw(&bsp_can1, 2);
jc2804 motor_pitch(&bsp_can1, 1);


/* 类静态成员赋值 */

const float jc2804::VOLTAGE_SCALE     = 0.1f;
const float jc2804::CURRENT_SCALE     = 0.01f;
const float jc2804::SPEED_SCALE       = 0.01f;
const float jc2804::POSITION_SCALE    = 0.01f;
const float jc2804::TEMPERATURE_SCALE = 0.1f;


/**
 * @brief 类成员函数定义
 *
 * @param can_interface can对象
 * @param device_id 偏移id
 */
jc2804::jc2804(BspCan* can_interface, uint8_t device_id)

  : _device_id(device_id),
    _last_request_type(NONE_REQUEST),
    _can(can_interface)
{
}

// 析构函数
jc2804::~jc2804()
{
}

/* 底层发送与请求跟踪 */

// 异步发送函数
void jc2804::send_async_command(uint8_t cmd, const uint8_t* data, uint8_t len)
{
  if (!_can || len > 8)
    return;

  uint32_t tx_id = 0x600 | _device_id; // 标准帧ID格式：0x600 + DeviceID
  _can->send(tx_id, const_cast<uint8_t*>(data), len);
}

// 用于发送请求+记录请求类型，以便后续解析响应
void jc2804::send_read_request(uint8_t cmd, uint16_t reg_addr, RequestType req_type)
{
  uint8_t data[8] = {0};
  data[0]         = cmd;                    // 命令字
  data[1]         = (reg_addr >> 8) & 0xFF; // 寄存器地址高字节
  data[2]         = reg_addr & 0xFF;        // 寄存器地址低字节
  // 其余字节保持为0

  // 记录本次请求类型，以便响应时能正确解析
  _last_request_type = req_type;

  send_async_command(cmd, data, 8);
}


/* 控制指令（写入）*/

// 设置扭矩（电流）
void jc2804::set_torque(float torque)
{
  // 文档 5.8: 命令字 0x2B, 寄存器 0x0020, 2字节有符号电流值（单位：A × 100）
  // 注意：文档描述为电流。这里按电流处理
  int16_t current_raw = static_cast<int16_t>(torque / CURRENT_SCALE);
  uint8_t data[8]     = {
    0x2B, // Command: Write Register
    0x00,
    0x20, // Register Address: 0x0020 (Current/Torque)
    0x00, // Reserved
    static_cast<uint8_t>(current_raw >> 8),
    static_cast<uint8_t>(current_raw & 0xFF),
    0x00,
    0x00 // Padding
  };
  send_async_command(0x2B, data, 8);
}

// 设置速度
void jc2804::set_speed(float speed)
{
  // 文档 5.9: 0x23 + reg 0x21, 4字节有符号速度（rpm × 100）
  int32_t spd_raw = static_cast<int32_t>(speed / SPEED_SCALE);
  uint8_t data[8] = {
    0x23, // Command: Write Register
    0x00,
    0x21, // Register Address: 0x0021 (Speed)
    0x00, // Reserved
    static_cast<uint8_t>(spd_raw >> 24),
    static_cast<uint8_t>(spd_raw >> 16),
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF)};
  send_async_command(0x23, data, 8);
}

// 设置绝对位置
void jc2804::set_absolute_position(float position)
{
  // 文档 5.10: 0x23 + reg 0x23, 4字节有符号位置（度 × 100）
  int32_t pos_raw = static_cast<int32_t>(position / POSITION_SCALE);
  uint8_t data[8] = {
    0x23, // Command: Write Register
    0x00,
    0x23, // Register Address: 0x0023 (Absolute Position)
    0x00, // Reserved
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF)};
  send_async_command(0x23, data, 8);
}

// 设置相对位置
void jc2804::set_relative_position(float position)
{
  // 文档 5.11: 0x23 + reg 0x25, 4字节有符号相对位置（度 × 100）
  int32_t pos_raw = static_cast<int32_t>(position / POSITION_SCALE);
  uint8_t data[8] = {
    0x23, // Command: Write Register
    0x00,
    0x25, // Register Address: 0x0025 (Relative Position)
    0x00, // Reserved
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF)};
  send_async_command(0x23, data, 8);
}

// 低速模式下设置低速
void jc2804::set_low_speed(float speed)
{
  // 文档 5.12: 0x2B + reg 0x27, 2字节无符号低速（rpm）
  uint16_t spd_raw = static_cast<uint16_t>(speed); // 文档描述不清晰，假设为整数rpm
  uint8_t  data[8] = {
    0x2B, // Command: Write Register
    0x00,
    0x27, // Register Address: 0x0027 (Low Speed)
    0x00, // Reserved
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF),
    0x00,
    0x00 // Padding
  };
  send_async_command(0x2B, data, 8);
}

// 速度位置控制（不知道哪个模式可以用）
void jc2804::pv_command(int32_t position, float speed)
{
  // 文档 5.13: 命令字 0x24
  // 数据格式：[cmd][pos H][M][L][L] (4B signed, ×100), [spd H][L] (2B unsigned rpm), [null]
  int32_t  pos_raw = static_cast<int32_t>(position * 100); // 放大100倍
  uint16_t spd_raw = static_cast<uint16_t>(speed);         // rpm 整数
  uint8_t  data[8] = {
    0x24, // Command: PV Command
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF),
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF),
    0x00 // Padding
  };
  send_async_command(0x24, data, 8);
}

// 速度位置力矩模式
void jc2804::pvt_command(int32_t position, float speed, float torque_percent)
{
  // 文档 5.14: 命令字 0x25
  // 格式：[cmd][pos H][M][L][L] (4B pos ×100), [spd H][L] (2B speed rpm), [torque% (1B 0~100)]
  int32_t  pos_raw    = static_cast<int32_t>(position * 100);
  uint16_t spd_raw    = static_cast<uint16_t>(speed);
  uint8_t  torque_raw = static_cast<uint8_t>(torque_percent);
  uint8_t  data[8]    = {
    0x25, // Command: PVT Command
    static_cast<uint8_t>(pos_raw >> 24),
    static_cast<uint8_t>(pos_raw >> 16),
    static_cast<uint8_t>(pos_raw >> 8),
    static_cast<uint8_t>(pos_raw & 0xFF),
    static_cast<uint8_t>(spd_raw >> 8),
    static_cast<uint8_t>(spd_raw & 0xFF),
    torque_raw // Torque Percentage
  };
  send_async_command(0x25, data, 8);
}

/**
 * @brief 设置控制模式
 *
 * @param mode 模式值
 * 0 力矩
 * 1 速度
 * 2 位置梯形
 * 3 位置滤波
 * 4 位置直通
 * 5 低速
 *
 */
void jc2804::set_control_mode(uint8_t mode)
{
  // 文档 5.15: 0x2B + reg 0x60, 写入模式值（0~5）
  uint8_t data[8] = {
    0x2B, // Command: Write Register
    0x00,
    0x60, // Register Address: 0x0060 (Control Mode)
    0x00, // Reserved
    0x00, // Padding
    mode, // Mode Value (0-5)
    0x00,
    0x00,

  };
  send_async_command(0x2B, data, 8);
}


/* 系统控制指令（5.16~5.22）*/

// 系统空闲
void jc2804::idle()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA0, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}

// 进入闭环模式
void jc2804::enter_closed_loop()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA2, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}

// 擦除数据
void jc2804::erase()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA3, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}

// 保存数据
void jc2804::save()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA4, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}

// 重启
void jc2804::restart()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA5, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}

// 设置零点
void jc2804::set_origin()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA6, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}

// 设置临时零点
void jc2804::set_temporary_origin()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA7, 0x00, 0x00, 0x01, 0x00, 0x00};
  send_async_command(0x2B, data, 8);
}


/* 读取请求（修改：记录请求类型） */

// 读取电压
void jc2804::request_power_voltage()
{
  send_read_request(0x4B, 0x0004, VOLTAGE_REQUEST);
}

// 读取？
void jc2804::request_bus_current()
{
  send_read_request(0x4B, 0x0005, CURRENT_REQUEST);
}

// 读取实时速度
void jc2804::request_real_time_speed()
{
  send_read_request(0x43, 0x0006, SPEED_REQUEST);
}

// 读取实时位置
void jc2804::request_real_time_position()
{
  send_read_request(0x43, 0x0008, POSITION_REQUEST);
}

// 读取驱动器温度
void jc2804::request_driver_temperature()
{
  send_read_request(0x4B, 0x000A, DRIVER_TEMP_REQUEST);
}

// 读取电机温度
void jc2804::request_motor_temperature()
{
  send_read_request(0x4B, 0x000B, MOTOR_TEMP_REQUEST);
}

// 读取错误日志
void jc2804::request_error_info()
{
  send_read_request(0x43, 0x000C, ERROR_INFO_REQUEST);
}

/* 数据解析（修改：基于请求类型）*/

// 解析数据
void jc2804::store_received_data(uint8_t* data)
{
  // 根据 _last_request_type 解析数据
  // 假设收到的响应数据格式符合文档描述
  switch (_last_request_type)
  {
    case VOLTAGE_REQUEST:
      if (data[0] == 0x4B)
      { // 确认是读取响应
        uint16_t raw        = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.voltage = raw * VOLTAGE_SCALE;
        _last_request_type  = NONE_REQUEST; // 清除请求类型
      }
      break;
    case CURRENT_REQUEST:
      if (data[0] == 0x4B)
      {
        uint16_t raw        = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.current = raw * CURRENT_SCALE;
        _last_request_type  = NONE_REQUEST;
      }
      break;
    case SPEED_REQUEST:
      if (data[0] == 0x43)
      {
        int32_t raw        = (static_cast<int32_t>(data[4]) << 24) | (static_cast<int32_t>(data[5]) << 16) | (static_cast<int32_t>(data[6]) << 8) | (static_cast<int32_t>(data[7]));
        latest_data.speed  = static_cast<float>(raw) * SPEED_SCALE;
        _last_request_type = NONE_REQUEST;
      }
      break;
    case POSITION_REQUEST:
      if (data[0] == 0x43)
      {
        int32_t raw          = (static_cast<int32_t>(data[4]) << 24) | (static_cast<int32_t>(data[5]) << 16) | (static_cast<int32_t>(data[6]) << 8) | (static_cast<int32_t>(data[7]));
        latest_data.position = static_cast<float>(raw) * POSITION_SCALE;
        _last_request_type   = NONE_REQUEST;
      }
      break;
    case DRIVER_TEMP_REQUEST:
      if (data[0] == 0x4B)
      {
        uint16_t raw            = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.driver_temp = raw * TEMPERATURE_SCALE;
        _last_request_type      = NONE_REQUEST;
      }
      break;
    case MOTOR_TEMP_REQUEST:
      if (data[0] == 0x4B)
      {
        uint16_t raw           = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        latest_data.motor_temp = raw * TEMPERATURE_SCALE;
        _last_request_type     = NONE_REQUEST;
      }
      break;
    case ERROR_INFO_REQUEST:
      if (data[0] == 0x43)
      {
        // 错误信息通常为4字节无符号整数
        uint32_t raw           = (static_cast<uint32_t>(data[4]) << 24) | (static_cast<uint32_t>(data[5]) << 16) | (static_cast<uint32_t>(data[6]) << 8) | (static_cast<uint32_t>(data[7]));
        latest_data.error_info = raw;
        _last_request_type     = NONE_REQUEST;
      }
      break;
    default:
      // 如果没有匹配的请求类型，或者不是预期的响应命令字，则清零
      _last_request_type = NONE_REQUEST;
      break;
  }
}


/* 响应验证（修改：简化逻辑，主要验证ID） */
bool jc2804::validate_response(uint8_t expected_cmd, can_rx_msg_t* rx_msg)
{
  uint32_t expected_id = 0x580 | _device_id; // 响应ID格式：0x580 + DeviceID
  if (rx_msg->header.Identifier != expected_id)
  {
    return false;
  }
  // 对于读取请求，响应的命令字应与发送的请求命令字相同 (0x4B or 0x43)
  // 对于写入请求，响应的命令字通常是 0x2A (ACK)
  // 我们在这里主要验证ID，具体命令字的处理交给 store_received_data
  // 简化验证逻辑，只要ID对得上，就认为是本设备的响应
  return true;
}


/* CAN 回调（修改：基于请求类型处理）*/

// CAN回调调用内容
void jc2804::on_can_message(can_rx_msg_t* rx_msg)
{
  // 验证消息ID是否属于本设备
  if (!validate_response(0, rx_msg))
  { // 不再关心expected_cmd
    return;
  }

  uint8_t received_cmd = rx_msg->data[0];

  // 检查是否是读取响应，并且我们之前确实发送了读取请求
  if (((received_cmd == 0x4B || received_cmd == 0x43) && _last_request_type != NONE_REQUEST))
  {
    // 尝试解析响应数据
    store_received_data(rx_msg->data);
  }

  // 注意：写入命令的响应 (0x2A) 通常不需要特殊处理来更新状态，
  // 因为它们只是确认命令已接收。
  // 如果需要确认写入的值，可能需要后续读取相关寄存器。
}


/* 获取最新数据（返回结构体）*/

// 只有这个可以从类中获取数据 发起请求后才可以读取到数据 异步
MotorData jc2804::get_latest_data_struct()
{
  MotorData data_copy = latest_data;
  return data_copy;
}