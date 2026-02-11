#include "JC2804.hpp"
#include "cmsis_os.h"
#include "string.h"

/**
 * @todo 对于电机返回的报文的处理方式需要改进
 *       有一些报文没有收到，需要根据仓库修改代码
 */

// 在JC2804.cpp或其他实现文件中添加以下定义
const float JC2804::VOLTAGE_SCALE     = 0.1f;
const float JC2804::CURRENT_SCALE     = 0.01f;
const float JC2804::SPEED_SCALE       = 0.01f;
const float JC2804::POSITION_SCALE    = 0.01f;
const float JC2804::TEMPERATURE_SCALE = 0.1f;

JC2804 motor_yaw(bsp_can1,2);
JC2804 motor_pitch(bsp_can1,1);

JC2804::JC2804(bsp_can& can_driver, int offset_id) : _device_id(offset_id), _can_driver(can_driver)
{
  // 默认设备ID为1
}

// 读取电源电压 (单位: V)
float JC2804::read_power_voltage()
{
  uint8_t data[8] = {0x4B, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8];

  if (send_command(0x4B, data, 8, response, 8))
  {
    uint16_t voltage_raw = (response[4] << 8) | response[5];
    return static_cast<float>(voltage_raw) * VOLTAGE_SCALE;
  }

  return 0.0f;
}

// 读取母线电流 (单位: A)
float JC2804::read_bus_current()
{
  uint8_t data[8] = {0x4B, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8];

  if (send_command(0x4B, data, 8, response, 8))
  {
    uint16_t current_raw = (response[4] << 8) | response[5];
    return static_cast<float>(current_raw) * CURRENT_SCALE;
  }

  return 0.0f;
}

float JC2804::read_real_time_speed()
{
  uint8_t data[8] = {0x43, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8];

  if (send_command(0x43, data, 8, response, 8))
  {
    int32_t speed_raw = (static_cast<int32_t>(response[4]) << 24) | (static_cast<int32_t>(response[5]) << 16) | (static_cast<int32_t>(response[6]) << 8) | static_cast<int32_t>(response[7]);
    return static_cast<float>(speed_raw) * SPEED_SCALE;
  }

  return 0.0f;
}

float JC2804::read_real_time_position()
{
  uint8_t data[8] = {0x43, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8];

  if (send_command(0x43, data, 8, response, 8))
  {
    int32_t position_raw = (static_cast<int32_t>(response[4]) << 24) | (static_cast<int32_t>(response[5]) << 16) | (static_cast<int32_t>(response[6]) << 8) | static_cast<int32_t>(response[7]);
    return static_cast<float>(position_raw) * POSITION_SCALE;
  }

  return 0.0f;
}

float JC2804::read_driver_temperature()
{
  uint8_t data[8] = {0x4B, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8];

  if (send_command(0x4B, data, 8, response, 8))
  {
    uint16_t temp_raw = (response[4] << 8) | response[5];
    return static_cast<float>(temp_raw) * TEMPERATURE_SCALE;
  }

  return 0.0f;
}

float JC2804::read_motor_temperature()
{
  uint8_t data[8] = {0x4B, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8];

  if (send_command(0x4B, data, 8, response, 8))
  {
    uint16_t temp_raw = (response[4] << 8) | response[5];
    return static_cast<float>(temp_raw) * TEMPERATURE_SCALE;
  }

  return 0.0f;
}

uint32_t JC2804::read_error_info()
{
  uint8_t data[8] = {0x43, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8];

  if (send_command(0x43, data, 8, response, 8))
  {
    uint32_t error_raw = (response[4] << 24) | (response[5] << 16) | (response[6] << 8) | response[7];
    return error_raw;
  }

  return 0;
}

void JC2804::set_torque(float torque)
{
  uint32_t torque_raw = static_cast<uint32_t>(torque * 100.0f); // 放大100倍
  uint8_t  data[8]    = {0x2B, 0x00, 0x20, 0x00,
                         (uint8_t)((torque_raw >> 8) & 0xFF),
                         (uint8_t)(torque_raw & 0xFF),
                         0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::set_speed(float speed)
{
  uint32_t speed_raw = static_cast<int32_t>(speed * 100.0f); // 放大100倍
  uint8_t  data[8]   = {0x23, 0x00, 0x21, 0x00,
                        (uint8_t)((speed_raw >> 24) & 0xFF),
                        (uint8_t)((speed_raw >> 16) & 0xFF),
                        (uint8_t)((speed_raw >> 8) & 0xFF),
                        (uint8_t)(speed_raw & 0xFF)};

  send_command(0x23, data, 8, nullptr, 0);
}

void JC2804::set_absolute_position(float position)
{
  uint32_t position_raw = static_cast<uint32_t>(position * 100.0f); // 放大100倍
  uint8_t  data[8]      = {0x23, 0x00, 0x23, 0x00,
                           (uint8_t)((position_raw >> 24) & 0xFF),
                           (uint8_t)((position_raw >> 16) & 0xFF),
                           (uint8_t)((position_raw >> 8) & 0xFF),
                           (uint8_t)(position_raw & 0xFF)};

  send_command(0x23, data, 8, nullptr, 0);
}

void JC2804::set_relative_position(float position)
{
  uint32_t position_raw = static_cast<uint32_t>(position * 100.0f); // 放大100倍
  uint8_t  data[8]      = {0x23, 0x00, 0x25, 0x00,
                           (uint8_t)((position_raw >> 24) & 0xFF),
                           (uint8_t)((position_raw >> 16) & 0xFF),
                           (uint8_t)((position_raw >> 8) & 0xFF),
                           (uint8_t)(position_raw & 0xFF)};

  send_command(0x23, data, 8, nullptr, 0);
}

void JC2804::set_low_speed(float speed)
{
  uint32_t speed_raw = static_cast<uint32_t>(speed * 100.0f); // 放大100倍
  uint8_t  data[8]   = {0x2B, 0x00, 0x27, 0x00,
                        (uint8_t)((speed_raw >> 8) & 0xFF),
                        (uint8_t)(speed_raw & 0xFF),
                        0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::pv_command(int32_t position, float speed)
{
  uint32_t position_raw = static_cast<uint32_t>(position);
  uint32_t speed_raw    = static_cast<uint32_t>(speed * 100.0f); // 放大100倍

  uint8_t data[8] = {0x24,
                     (uint8_t)((position_raw >> 24) & 0xFF),
                     (uint8_t)((position_raw >> 16) & 0xFF),
                     (uint8_t)((position_raw >> 8) & 0xFF),
                     (uint8_t)(position_raw & 0xFF),
                     (uint8_t)((speed_raw >> 8) & 0xFF),
                     (uint8_t)(speed_raw & 0xFF),
                     0x00};

  send_command(0x24, data, 8, nullptr, 0);
}

void JC2804::pvt_command(int32_t position, float speed, float torque_percent)
{
  uint32_t position_raw       = static_cast<uint32_t>(position);
  uint32_t speed_raw          = static_cast<uint32_t>(speed * 100.0f); // 放大100倍
  uint8_t  torque_percent_raw = static_cast<uint8_t>(torque_percent);  // 力矩百分比，0-100

  uint8_t data[8] = {0x25,
                     (uint8_t)((position_raw >> 24) & 0xFF),
                     (uint8_t)((position_raw >> 16) & 0xFF),
                     (uint8_t)((position_raw >> 8) & 0xFF),
                     (uint8_t)(position_raw & 0xFF),
                     (uint8_t)((speed_raw >> 8) & 0xFF),
                     (uint8_t)(speed_raw & 0xFF),
                     torque_percent_raw};

  send_command(0x25, data, 8, nullptr, 0);
}

void JC2804::set_control_mode(uint8_t mode)
{
  uint8_t data[8] = {0x2B, 0x00, 0x60, 0x00,
                     0x00, mode, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::idle()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA0, 0x00,
                     0x00, 0x01, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::enter_closed_loop()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA2, 0x00,
                     0x00, 0x01, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::erase()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA3, 0x00,
                     0x00, 0x01, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::save()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA4, 0x00,
                     0x00, 0x01, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::restart()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA5, 0x00,
                     0x00, 0x01, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::set_origin()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA6, 0x00,
                     0x00, 0x01, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

void JC2804::set_temporary_origin()
{
  uint8_t data[8] = {0x2B, 0x00, 0xA7, 0x00,
                     0x00, 0x01, 0x00, 0x00};

  send_command(0x2B, data, 8, nullptr, 0);
}

// 收发逻辑在一起，有阻塞
bool JC2804::send_command(uint8_t command, uint8_t* data, uint8_t len, uint8_t* response, uint8_t response_len)
{
  // 构建CAN消息
  uint32_t std_id = 0x600 | _device_id;

  // 发送命令
  if (_can_driver.send(std_id, data, len) != HAL_OK)
  {
    return false;
  }

  // 等待响应
  can_rx_msg_t rx_msg;
  osStatus_t   status = _can_driver.receive(&rx_msg, COMM_TIMEOUT);

  if (status == osOK)
  {
    // 检查响应ID是否正确
    uint32_t expected_id = 0x580 | _device_id;
    if (rx_msg.header.Identifier == expected_id)
    {
      // 检查命令字是否匹配
      if (response && rx_msg.data[0] == command)
      {
        // 复制响应数据
        if (response_len > 0)
        {
          memcpy(response, rx_msg.data, response_len);
        }
        return true;
      }
    }
  }

  return false;
}