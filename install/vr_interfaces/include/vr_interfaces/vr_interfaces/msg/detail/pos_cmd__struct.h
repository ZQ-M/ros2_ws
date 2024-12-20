// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from vr_interfaces:msg/PosCmd.idl
// generated code does not contain a copyright notice

#ifndef VR_INTERFACES__MSG__DETAIL__POS_CMD__STRUCT_H_
#define VR_INTERFACES__MSG__DETAIL__POS_CMD__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Constants defined in the message

/// Struct defined in msg/PosCmd in the package vr_interfaces.
typedef struct vr_interfaces__msg__PosCmd
{
  double x;
  double y;
  double z;
  double roll;
  double pitch;
  double yaw;
  double gripper;
  double quater_x;
  double quater_y;
  double quater_z;
  double quater_w;
  double chx;
  double chy;
  double chz;
  double vel_l;
  double vel_r;
  double height;
  double head_pit;
  double head_yaw;
  double temp_float_data[6];
  int32_t temp_int_data[6];
  int32_t mode1;
  int32_t mode2;
  int32_t time_count;
} vr_interfaces__msg__PosCmd;

// Struct for a sequence of vr_interfaces__msg__PosCmd.
typedef struct vr_interfaces__msg__PosCmd__Sequence
{
  vr_interfaces__msg__PosCmd * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} vr_interfaces__msg__PosCmd__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // VR_INTERFACES__MSG__DETAIL__POS_CMD__STRUCT_H_
