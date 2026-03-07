/**
 * @file DJImotor.cpp
 * @author ChoseB
 * @brief 大疆电机3508,6020,2006的驱动代码
 * @version 0.1
 * @date 2026-03-05
 * 
 * @copyright Copyright (c) 2026
 * 
 */

/**
 * @brief 使用示例
 * 
 */
#include "DJImotor.hpp"
#include "bsp_can.hpp"
#include "stm32h7xx_hal_fdcan.h"
#include <cstdint>

can_tx_msg_t CanTxBuffer0x200forCan1 = {
  .header = {
    .Identifier = 0x200,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x200forCan2 = {
  .header = {
    .Identifier = 0x200,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x1FFforCan1 = {
  .header = {
    .Identifier = 0x1FF,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x1FFforCan2 ={
  .header = {
    .Identifier = 0x1FF,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x2FFforCan1 = {
  .header = {
    .Identifier = 0x2FF,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x2FFforCan2 = {
  .header = {
    .Identifier = 0x2FF,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x1FEforCan1 = {
  .header = {
    .Identifier = 0x1FE,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x1FEforCan2 = {
  .header = {
    .Identifier = 0x1FE,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x2FEforCan1 = {
  .header = {
    .Identifier = 0x2FE,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};
can_tx_msg_t CanTxBuffer0x2FEforCan2 = {
  .header = {
    .Identifier = 0x2FE,
    .DataLength = FDCAN_DLC_BYTES_8,
  }
};

RM_Motor::RM_Motor(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio) : CanItem(can)
{
  motorParam.motor_id = id;
  motorParam.ecd_offset = ecd_offset;
  motorParam.ecd_range = 8191; 
  motorParam.reduction_ratio = gearRatio;
}

Motor3508::Motor3508(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio) : RM_Motor(can, id, ecd_offset, gearRatio) {
  motorParam.motor_id = id;
  motorParam.ecd_offset = ecd_offset;
  motorParam.ecd_range = 8191; 
  motorParam.reduction_ratio = gearRatio;
  motorParam.current_limit = 16000; 
  txBuffer = nullptr;
  if ( id >= 1 && id <= 4 && bsp_can == &bsp_can1 ){
    txBuffer = &CanTxBuffer0x200forCan1;
  }
  if ( id >= 1 && id <= 4 && bsp_can == &bsp_can2 ){
    txBuffer = &CanTxBuffer0x200forCan2;
  }
  if ( id >= 5 && id <= 8 && bsp_can == &bsp_can1 ){
    txBuffer = &CanTxBuffer0x1FFforCan1;
  }
  if ( id >= 5 && id <= 8 && bsp_can == &bsp_can2 ){
    txBuffer = &CanTxBuffer0x1FFforCan2;
  }
  this->canId = 0x200 + id; // 3508的CAN ID为0x201-0x208
}

Motor6020::Motor6020(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio, Motor6020ControlMode mode) : RM_Motor(can, id, ecd_offset, gearRatio) , controlMode(mode){
  motorParam.motor_id = id;
  motorParam.ecd_offset = ecd_offset;
  motorParam.ecd_range = 8191; 
  motorParam.reduction_ratio = gearRatio;
  motorParam.current_limit = 29000; 
  txBuffer = nullptr;
  if ( mode == Voltage ){
    if ( id >= 1 && id <= 4 && bsp_can == &bsp_can1 ){
      txBuffer = &CanTxBuffer0x1FFforCan1;
    }
    if ( id >= 1 && id <= 4 && bsp_can == &bsp_can2 ){
      txBuffer = &CanTxBuffer0x1FFforCan2;
    }
    if ( id >= 5 && id <= 8 && bsp_can == &bsp_can1 ){
      txBuffer = &CanTxBuffer0x2FFforCan1;
    }
    if ( id >= 5 && id <= 8 && bsp_can == &bsp_can2 ){
      txBuffer = &CanTxBuffer0x2FFforCan2;
    }
  }
  if ( mode == Current ){
    if ( id >= 1 && id <= 4 && bsp_can == &bsp_can1 ){
      txBuffer = &CanTxBuffer0x1FEforCan1;
    }
    if ( id >= 1 && id <= 4 && bsp_can == &bsp_can2 ){
      txBuffer = &CanTxBuffer0x1FEforCan2;
    }
    if ( id >= 5 && id <= 8 && bsp_can == &bsp_can1 ){
      txBuffer = &CanTxBuffer0x2FEforCan1;
    }
    if ( id >= 5 && id <= 8 && bsp_can == &bsp_can2 ){
      txBuffer = &CanTxBuffer0x2FEforCan2;
    }
  }
  this->canId = 0x204 + id;
}

Motor2006::Motor2006(BspCan* can, uint8_t id, uint16_t ecd_offset, float gearRatio) : RM_Motor(can, id, ecd_offset, gearRatio) {
  motorParam.motor_id = id;
  motorParam.ecd_offset = ecd_offset;
  motorParam.ecd_range = 8191; 
  motorParam.reduction_ratio = gearRatio;
  motorParam.current_limit = 9900; 
  txBuffer = nullptr;
  if ( id >= 1 && id <= 4 && bsp_can == &bsp_can1 ){
    txBuffer = &CanTxBuffer0x200forCan1;
  }
  if ( id >= 1 && id <= 4 && bsp_can == &bsp_can2 ){
    txBuffer = &CanTxBuffer0x200forCan2;
  }
  if ( id >= 5 && id <= 8 && bsp_can == &bsp_can1 ){
    txBuffer = &CanTxBuffer0x1FFforCan1;
  }
  if ( id >= 5 && id <= 8 && bsp_can == &bsp_can2 ){
    txBuffer = &CanTxBuffer0x1FFforCan2;
  }
  this->canId = 0x200 + id; // 2006的CAN ID为0x201-0x208
}

static void updateData(RawData_t rawData, can_rx_msg_t rxMsg){
  rawData.raw_ecd = (rxMsg.data[0] << 8) | rxMsg.data[1];
  rawData.speed_rpm = (rxMsg.data[2] << 8) | rxMsg.data[3];
  rawData.torque_current = (rxMsg.data[4] << 8) | rxMsg.data[5];
  rawData.temperature = rxMsg.data[6];
}

static void EcdToAngle(RawData_t rawData, TreatedData_t treatedData, MotorParam_t motorParam){
  treatedData.last_ecd = treatedData.ecd;
  treatedData.last_angle = treatedData.last_angle;

  treatedData.ecd = rawData.raw_ecd - motorParam.ecd_offset;
  if (treatedData.ecd < 0) {
    treatedData.ecd += motorParam.ecd_range;
  }

  treatedData.round_cnt += (treatedData.ecd - treatedData.last_ecd) > motorParam.ecd_range / 2 ? 1 : 0;
  treatedData.round_cnt -= (treatedData.ecd - treatedData.last_ecd) < -motorParam.ecd_range / 2 ? 1 : 0;
  treatedData.total_ecd = treatedData.round_cnt * motorParam.ecd_range + treatedData.ecd;
  treatedData.total_angle = treatedData.total_ecd / (float)motorParam.ecd_range * 360.0;
  treatedData.axis_total_angle = treatedData.total_ecd / (float)(motorParam.ecd_range * motorParam.reduction_ratio) * 360.0;
  treatedData.angle = fmodf(treatedData.axis_total_angle, 360.0f);
  treatedData.angle = treatedData.angle < 0 ? treatedData.angle + motorParam.ecd_range : treatedData.angle;

}

void RM_Motor::callback(uint8_t* data) 
{
  updateData(rawData, this->rxMsg);
  EcdToAngle(rawData, treatedData, motorParam);
}

void RM_Motor::send(uint8_t* data, uint8_t len)
{
  // 禁用RM电机单独发送数据
}

void RM_Motor::filldata(int32_t output){
  treatedData.motor_output = output;
  if (output > motorParam.current_limit) {
    treatedData.motor_output = motorParam.current_limit;
  } 
  else if (output < -motorParam.current_limit) {
    treatedData.motor_output = -motorParam.current_limit;
  }

  if (txBuffer != nullptr){
    if ( motorParam.motor_id <= 4 ){
      txBuffer->data[2*(motorParam.motor_id-0)-2] = (uint8_t)(treatedData.motor_output >> 8);
      txBuffer->data[2*(motorParam.motor_id-0)-1] = (uint8_t)(treatedData.motor_output);
    }
    else{
      txBuffer->data[2*(motorParam.motor_id-4)-2] = (uint8_t)(treatedData.motor_output >> 8);
      txBuffer->data[2*(motorParam.motor_id-4)-1] = (uint8_t)(treatedData.motor_output);
    }
  }
}