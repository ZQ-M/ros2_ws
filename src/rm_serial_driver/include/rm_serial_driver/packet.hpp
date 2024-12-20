// Copyright (c) 2022 ChenJun
// Licensed under the Apache-2.0 License.

#ifndef RM_SERIAL_DRIVER__PACKET_HPP_
#define RM_SERIAL_DRIVER__PACKET_HPP_

#include <algorithm>
#include <cstdint>
#include <vector>

namespace rm_serial_driver
{
struct ReceivePacket
{
  uint8_t header1 = 0x55;
  uint8_t header2 = 0xAA;
  uint8_t left_head = 0xF1; //左手帧头
  int left_XYZ[3];          //左手坐标
  int left_RPY[3];          //左手欧拉角
  int left_gripper;         //左手夹爪
  uint8_t right_head = 0xF2; //右手帧头
  int right_XYZ[3];          //右手坐标
  int right_RPY[3];          //右手欧拉角
  int right_gripper;         //右手夹爪
  uint8_t chassis_head = 0xF3; //底盘帧头
  int chassis_CHXYZ[3];     //底盘手柄坐标
  int chassis_PY[2];        //底盘头部欧拉角
  int height;               //底盘高度
  uint8_t mode1;            //底盘模式1
  uint8_t mode2;            //底盘模式2
  int time_count;           //数据包计数
  uint8_t end[3] = {0X0A,0x0D,0XEE}; //帧尾
} __attribute__((packed));

struct SendPacket
{
  uint8_t header = 0xA5;
  bool tracking : 1;
  uint8_t id : 3;          // 0-outpost 6-guard 7-base
  uint8_t armors_num : 3;  // 2-balance 3-outpost 4-normal
  uint8_t reserved : 1;
  float x;
  float y;
  float z;
  float yaw;
  float vx;
  float vy;
  float vz;
  float v_yaw;
  float r1;
  float r2;
  float dz;
  uint16_t checksum = 0;
} __attribute__((packed));

inline ReceivePacket fromVector(const std::vector<uint8_t> & data)
{
  ReceivePacket packet;
  std::copy(data.begin(), data.end(), reinterpret_cast<uint8_t *>(&packet));
  return packet;
}

inline std::vector<uint8_t> toVector(const SendPacket & data)
{
  std::vector<uint8_t> packet(sizeof(SendPacket));
  std::copy(
    reinterpret_cast<const uint8_t *>(&data),
    reinterpret_cast<const uint8_t *>(&data) + sizeof(SendPacket), packet.begin());
  return packet;
}

}  // namespace rm_serial_driver

#endif  // RM_SERIAL_DRIVER__PACKET_HPP_
