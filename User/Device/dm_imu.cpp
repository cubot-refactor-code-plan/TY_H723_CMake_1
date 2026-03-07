/**
 * @file dm_imu.cpp
 * @author Rh
 * @brief 使用bsp_can接口封装的DM IMU类实现
 * @version 0.1
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief 使用示例,需要在bsp_can
 *
 * @note 类的实例化，以及初始化
 *
 *  dm_imu imu_bmi088(&bsp_can1, 0x58, 0x59); // 全局实例化类
 *  imu_bmi088.init();                        // 需要在freertos内核开启之后去init
 *
 * @note 在CAN接收后处理任务中，添加对应的处理函数
 *
 *  imu_bmi088.on_can_message(&rx_msg); // 类似中断回调
 *
 * @note extern好之后 直接使用
 *
 *  imu_bmi088.change_to_active();                     // 主动发送数据
 *  imu_data user_imu_data = imu_bmi088.get_imu_data() // 读取数据
 *
 */


#include "dm_imu.hpp"
#include <stdio.h>
#include <string.h>


/* 全局对象的实例化 */

dm_imu imu_bmi088(&bsp_can1, 0x58, 0x59);


/**
 * @brief 枚举模式的实现
 *
 */
typedef enum reg_id_e
{
  REBOOT_IMU = 0,        // 重启IMU
  ACCEL_DATA,            // 加速度数据
  GYRO_DATA,             // 陀螺仪数据
  EULER_DATA,            // 欧拉角数据
  QUAT_DATA,             // 四元数数据
  SET_ZERO,              // 设置零点
  ACCEL_CALI,            // 加速度计校准
  GYRO_CALI,             // 陀螺仪校准
  MAG_CALI,              // 磁力计校准
  CHANGE_COM,            // 更改通信端口
  SET_DELAY,             // 设置延时
  CHANGE_ACTIVE,         // 更改激活状态
  SET_BAUD,              // 设置波特率
  SET_CAN_ID,            // 设置CAN ID
  SET_MST_ID,            // 设置主ID
  DATA_OUTPUT_SELECTION, // 数据输出选择
  SAVE_PARAM      = 254, // 保存参数
  RESTORE_SETTING = 255  // 恢复设置
} reg_id_e;


/**
 * @brief Construct a new dm imu::dm imu object
 *
 * @param can_bus 使用的bsp_can地址
 * @param device_id 设备id
 * @param master_id 主机id
 */
dm_imu::dm_imu(BspCan* can_bus, uint8_t device_id, uint8_t master_id) : _device_id(device_id), _master_id(master_id), _can_bus(can_bus)
{
  // 初始化内部数据结构
  memset(&_imu_data, 0, sizeof(_imu_data));
  _imu_data.can_id = device_id;
  _imu_data.mst_id = master_id;
}

dm_imu::~dm_imu()
{
  // 删除互斥锁
  if (_data_mutex_handle != NULL)
  {
    osMutexDelete(_data_mutex_handle);
    _data_mutex_handle = NULL;
  }
}

void dm_imu::init()
{
  snprintf(name, sizeof(name), "IMU_Data_Mutex");
  // 创建互斥锁
  const osMutexAttr_t mutex_attr = {
    .name      = name,
    .attr_bits = 0,
    .cb_mem    = NULL,
    .cb_size   = 0};
  _data_mutex_handle = osMutexNew(&mutex_attr);
}

void dm_imu::write_register(uint8_t reg_id, uint32_t data)
{
  if (_can_bus == nullptr)
    return;

  uint8_t buf[8] = {0xCC, reg_id, CMD_WRITE, 0xDD, 0, 0, 0, 0};
  memcpy(buf + 4, &data, 4);

  _can_bus->send(_device_id, buf, 8);
}

void dm_imu::read_register(uint8_t reg_id)
{
  if (_can_bus == nullptr)
    return;

  uint8_t buf[8] = {0xCC, reg_id, CMD_READ, 0xDD, 0, 0, 0, 0};

  _can_bus->send(_device_id, buf, 8);
}

void dm_imu::reboot()
{
  write_register(REBOOT_IMU, 0);
}

void dm_imu::accel_calibration()
{
  write_register(ACCEL_CALI, 0);
}

void dm_imu::gyro_calibration()
{
  write_register(GYRO_CALI, 0);
}

void dm_imu::change_com_port(imu_com_port_e port)
{
  write_register(CHANGE_COM, static_cast<uint8_t>(port));
}

void dm_imu::set_active_mode_delay(uint32_t delay)
{
  write_register(SET_DELAY, delay);
}

void dm_imu::change_to_active()
{
  write_register(CHANGE_ACTIVE, 1);
}

void dm_imu::change_to_request()
{
  write_register(CHANGE_ACTIVE, 0);
}

void dm_imu::set_baud(imu_baudrate_e baud)
{
  write_register(SET_BAUD, static_cast<uint8_t>(baud));
}

void dm_imu::set_can_id(uint8_t can_id)
{
  write_register(SET_CAN_ID, can_id);
}

void dm_imu::set_mst_id(uint8_t mst_id)
{
  write_register(SET_MST_ID, mst_id);
}

void dm_imu::save_parameters()
{
  write_register(SAVE_PARAM, 0);
}

void dm_imu::restore_settings()
{
  write_register(RESTORE_SETTING, 0);
}


void dm_imu::request_euler()
{
  read_register(EULER_DATA);
}

void dm_imu::request_quat()
{
  read_register(QUAT_DATA);
}

imu_data dm_imu::get_imu_data()
{
  imu_data data_copy;

  if (_data_mutex_handle != NULL)
  {
    osMutexAcquire(_data_mutex_handle, osWaitForever);
  }

  // 复制数据
  data_copy = _imu_data;

  // 退出临界区：释放互斥锁并恢复中断
  if (_data_mutex_handle != NULL)
  {
    osMutexRelease(_data_mutex_handle);
  }

  return data_copy;
}

void dm_imu::set_imu_data(const imu_data& data)
{
  // 进入临界区：关闭中断并获取互斥锁
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();

  if (_data_mutex_handle != NULL)
  {
    osMutexAcquire(_data_mutex_handle, osWaitForever);
  }

  // 更新数据
  _imu_data = data;

  // 退出临界区：释放互斥锁并恢复中断
  if (_data_mutex_handle != NULL)
  {
    osMutexRelease(_data_mutex_handle);
  }

  __set_PRIMASK(irq_state);
}

int dm_imu::float_to_uint(float x_float, float x_min, float x_max, int bits)
{
  /* Converts a float to an unsigned int, given range and number of bits */
  float span   = x_max - x_min;
  float offset = x_min;
  return (int)((x_float - offset) * ((float)((1 << bits) - 1)) / span);
}

float dm_imu::uint_to_float(int x_int, float x_min, float x_max, int bits)
{
  /* converts unsigned int to float, given range and number of bits */
  float span   = x_max - x_min;
  float offset = x_min;
  return ((float)x_int) * span / ((float)((1 << bits) - 1)) + offset;
}

void dm_imu::update_euler(uint8_t* pData)
{
  int16_t euler[3];

  euler[0] = static_cast<int16_t>((pData[3] << 8) | pData[2]);
  euler[1] = static_cast<int16_t>((pData[5] << 8) | pData[4]);
  euler[2] = static_cast<int16_t>((pData[7] << 8) | pData[6]);

  // 进入临界区：关闭中断并获取互斥锁
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();

  if (_data_mutex_handle != NULL)
  {
    osMutexAcquire(_data_mutex_handle, osWaitForever);
  }

  _imu_data.pitch = uint_to_float(euler[0], PITCH_CAN_MIN, PITCH_CAN_MAX, 16);
  _imu_data.yaw   = uint_to_float(euler[1], YAW_CAN_MIN, YAW_CAN_MAX, 16);
  _imu_data.roll  = uint_to_float(euler[2], ROLL_CAN_MIN, ROLL_CAN_MAX, 16);

  // 退出临界区：释放互斥锁并恢复中断
  if (_data_mutex_handle != NULL)
  {
    osMutexRelease(_data_mutex_handle);
  }

  __set_PRIMASK(irq_state);
}

void dm_imu::update_quaternion(uint8_t* pData)
{
  int w = pData[1] << 6 | ((pData[2] & 0xF8) >> 2);
  int x = (pData[2] & 0x03) << 12 | (pData[3] << 4) | ((pData[4] & 0xF0) >> 4);
  int y = (pData[4] & 0x0F) << 10 | (pData[5] << 2) | ((pData[6] & 0xC0) >> 6);
  int z = (pData[6] & 0x3F) << 8 | pData[7];

  // 进入临界区：关闭中断并获取互斥锁
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();

  if (_data_mutex_handle != NULL)
  {
    osMutexAcquire(_data_mutex_handle, osWaitForever);
  }

  _imu_data.q[0] = uint_to_float(w, Quaternion_MIN, Quaternion_MAX, 14);
  _imu_data.q[1] = uint_to_float(x, Quaternion_MIN, Quaternion_MAX, 14);
  _imu_data.q[2] = uint_to_float(y, Quaternion_MIN, Quaternion_MAX, 14);
  _imu_data.q[3] = uint_to_float(z, Quaternion_MIN, Quaternion_MAX, 14);

  // 退出临界区：释放互斥锁并恢复中断
  if (_data_mutex_handle != NULL)
  {
    osMutexRelease(_data_mutex_handle);
  }

  __set_PRIMASK(irq_state);
}

void dm_imu::on_can_message(can_rx_msg_t* rx_msg)
{
  if (rx_msg->data[0] == 0x03)
  {
    update_euler(rx_msg->data);
  }
  else if (rx_msg->data[0] == 0x04)
  {
    update_quaternion(rx_msg->data);
  }
}
