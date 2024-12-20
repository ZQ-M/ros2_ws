// Copyright (c) 2022 ChenJun
// Licensed under the Apache-2.0 License.

#include <tf2/LinearMath/Quaternion.h>

#include <rclcpp/logging.hpp>
#include <rclcpp/qos.hpp>
#include <rclcpp/utilities.hpp>
#include <serial_driver/serial_driver.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

// C++ system
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rm_serial_driver/crc.hpp"
#include "rm_serial_driver/packet.hpp"
#include "rm_serial_driver/rm_serial_driver.hpp"
#include "vr_interfaces/msg/pos_cmd.hpp"

namespace rm_serial_driver
{
RMSerialDriver::RMSerialDriver(const rclcpp::NodeOptions & options)
: Node("rm_serial_driver", options),
  owned_ctx_{new IoContext(2)},
  serial_driver_{new drivers::serial_driver::SerialDriver(*owned_ctx_)}
{
  RCLCPP_INFO(get_logger(), "Start RMSerialDriver!");

  getParams();

  // TF broadcaster
  timestamp_offset_ = this->declare_parameter("timestamp_offset", 0.0);
  tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

  // Create Publisher
  arxvrr_pub_ = this->create_publisher<vr_interfaces::msg::PosCmd>("ARX_VR_R", 10);
  arxvrl_pub_ = this->create_publisher<vr_interfaces::msg::PosCmd>("ARX_VR_L", 10);

  // Detect parameter client
  detector_param_client_ = std::make_shared<rclcpp::AsyncParametersClient>(this, "armor_detector");

  // Tracker reset service client
  reset_tracker_client_ = this->create_client<std_srvs::srv::Trigger>("/tracker/reset");

  try {
    serial_driver_->init_port(device_name_, *device_config_);
    if (!serial_driver_->port()->is_open()) {
      serial_driver_->port()->open();
      receive_thread_ = std::thread(&RMSerialDriver::receiveData, this);
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(
      get_logger(), "Error creating serial port: %s - %s", device_name_.c_str(), ex.what());
    throw ex;
  }
}

RMSerialDriver::~RMSerialDriver()
{
  if (receive_thread_.joinable()) {
    receive_thread_.join();
  }

  if (serial_driver_->port()->is_open()) {
    serial_driver_->port()->close();
  }

  if (owned_ctx_) {
    owned_ctx_->waitForExit();
  }
}

void RMSerialDriver::receiveData()
{
  std::vector<uint8_t> header(2);       //设置帧头大小
  std::vector<uint8_t> data;            //储存数据
  data.reserve(sizeof(ReceivePacket));  //预分配足够的内存以存储一个完整的 ReceivePacket 数据包

  while (rclcpp::ok()) {
    try {
      serial_driver_->port()->receive(header);  //先接受帧头

      if (header[0] == 0x55 && header[1] == 0xAA) {  //检验帧头
        data.resize(sizeof(ReceivePacket) - 2);      //分配数据包大小
        serial_driver_->port()->receive(data);       //接受数据到向量data中

        data.insert(data.begin(), header.begin(), header.end());  //将帧头插入数据包形成完整数据帧
        ReceivePacket packet = fromVector(data);                  //通过内存分配的方式直接将数据塞入结构体的数据格中
        if (packet.end[0] == 0x0A && packet.end[1] == 0x0D && packet.end[2] == 0xEE) {//帧尾数据包检测
          // RCLCPP_INFO(
          //   get_logger(),
          //   "Packet Info: header1=%02X, header2=%02X, left_head=%02X, left_XYZ=[%d, %d, %d], "
          //   "left_RPY=[%d, %d, %d], left_gripper=%d, right_head=%02X, right_XYZ=[%d, %d, %d], "
          //   "right_RPY=[%d, %d, %d], right_gripper=%d, chassis_head=%02X, chassis_CHXYZ=[%d, %d, "
          //   "%d], "
          //   "chassis_PY=[%d, %d], height=%d, mode1=%d, mode2=%d, time_count=%d, End=[%02X, %02X, "
          //   "%02X]",
          //   packet.header1, packet.header2, packet.left_head, packet.left_XYZ[0],
          //   packet.left_XYZ[1], packet.left_XYZ[2], packet.left_RPY[0], packet.left_RPY[1],
          //   packet.left_RPY[2], packet.left_gripper, packet.right_head, packet.right_XYZ[0],
          //   packet.right_XYZ[1], packet.right_XYZ[2], packet.right_RPY[0], packet.right_RPY[1],
          //   packet.right_RPY[2], packet.right_gripper, packet.chassis_head, packet.chassis_CHXYZ[0],
          //   packet.chassis_CHXYZ[1], packet.chassis_CHXYZ[2], packet.chassis_PY[0],
          //   packet.chassis_PY[1], packet.height, packet.mode1, packet.mode2, packet.time_count,
          //   packet.end[0], packet.end[1], packet.end[2]);
          
          VR_Right.x= packet.right_XYZ[0];
          VR_Right.y= packet.right_XYZ[1];
          VR_Right.z= packet.right_XYZ[2];
          VR_Right.roll= packet.right_RPY[0];
          VR_Right.pitch= packet.right_RPY[1];
          VR_Right.yaw= packet.right_RPY[2];
          VR_Right.gripper= packet.right_gripper;

          VR_Left.x= packet.left_XYZ[0];
          VR_Left.y= packet.left_XYZ[1];
          VR_Left.z= packet.left_XYZ[2];
          VR_Left.roll= packet.left_RPY[0];
          VR_Left.pitch= packet.left_RPY[1];
          VR_Left.yaw= packet.left_RPY[2];
          VR_Left.gripper= packet.left_gripper;

          arxvrr_pub_->publish(VR_Right);
          arxvrl_pub_->publish(VR_Left);
          
        } else {RCLCPP_WARN_THROTTLE(
          get_logger(), *get_clock(), 20, "帧尾校验失败: %02X %02X %02X", packet.end[0], packet.end[1],packet.end[2]);}
      } else {
        RCLCPP_WARN_THROTTLE(
          get_logger(), *get_clock(), 20, "帧头校验失败: %02X %02X", header[0], header[1]);}
    } catch (const std::exception & ex) {
      RCLCPP_ERROR_THROTTLE(
        get_logger(), *get_clock(), 20, "Error while receiving data: %s", ex.what());
      reopenPort();
    }
  }
}

void RMSerialDriver::getParams()
{
  using FlowControl = drivers::serial_driver::FlowControl;
  using Parity = drivers::serial_driver::Parity;
  using StopBits = drivers::serial_driver::StopBits;

  uint32_t baud_rate{};
  auto fc = FlowControl::NONE;
  auto pt = Parity::NONE;
  auto sb = StopBits::ONE;

  try {
    device_name_ = declare_parameter<std::string>("device_name", "");
  } catch (rclcpp::ParameterTypeException & ex) {
    RCLCPP_ERROR(get_logger(), "The device name provided was invalid");
    throw ex;
  }

  try {
    baud_rate = declare_parameter<int>("baud_rate", 0);
  } catch (rclcpp::ParameterTypeException & ex) {
    RCLCPP_ERROR(get_logger(), "The baud_rate provided was invalid");
    throw ex;
  }

  try {
    const auto fc_string = declare_parameter<std::string>("flow_control", "");

    if (fc_string == "none") {
      fc = FlowControl::NONE;
    } else if (fc_string == "hardware") {
      fc = FlowControl::HARDWARE;
    } else if (fc_string == "software") {
      fc = FlowControl::SOFTWARE;
    } else {
      throw std::invalid_argument{
        "The flow_control parameter must be one of: none, software, or hardware."};
    }
  } catch (rclcpp::ParameterTypeException & ex) {
    RCLCPP_ERROR(get_logger(), "The flow_control provided was invalid");
    throw ex;
  }

  try {
    const auto pt_string = declare_parameter<std::string>("parity", "");

    if (pt_string == "none") {
      pt = Parity::NONE;
    } else if (pt_string == "odd") {
      pt = Parity::ODD;
    } else if (pt_string == "even") {
      pt = Parity::EVEN;
    } else {
      throw std::invalid_argument{"The parity parameter must be one of: none, odd, or even."};
    }
  } catch (rclcpp::ParameterTypeException & ex) {
    RCLCPP_ERROR(get_logger(), "The parity provided was invalid");
    throw ex;
  }

  try {
    const auto sb_string = declare_parameter<std::string>("stop_bits", "");

    if (sb_string == "1" || sb_string == "1.0") {
      sb = StopBits::ONE;
    } else if (sb_string == "1.5") {
      sb = StopBits::ONE_POINT_FIVE;
    } else if (sb_string == "2" || sb_string == "2.0") {
      sb = StopBits::TWO;
    } else {
      throw std::invalid_argument{"The stop_bits parameter must be one of: 1, 1.5, or 2."};
    }
  } catch (rclcpp::ParameterTypeException & ex) {
    RCLCPP_ERROR(get_logger(), "The stop_bits provided was invalid");
    throw ex;
  }

  device_config_ =
    std::make_unique<drivers::serial_driver::SerialPortConfig>(baud_rate, fc, pt, sb);
}

void RMSerialDriver::reopenPort()
{
  RCLCPP_WARN(get_logger(), "Attempting to reopen port");
  try {
    if (serial_driver_->port()->is_open()) {
      serial_driver_->port()->close();
    }
    serial_driver_->port()->open();
    RCLCPP_INFO(get_logger(), "Successfully reopened port");
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Error while reopening port: %s", ex.what());
    if (rclcpp::ok()) {
      rclcpp::sleep_for(std::chrono::seconds(1));
      reopenPort();
    }
  }
}

void RMSerialDriver::setParam(const rclcpp::Parameter & param)
{
  if (!detector_param_client_->service_is_ready()) {
    RCLCPP_WARN(get_logger(), "Service not ready, skipping parameter set");
    return;
  }

  if (
    !set_param_future_.valid() ||
    set_param_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    RCLCPP_INFO(get_logger(), "Setting detect_color to %ld...", param.as_int());
    set_param_future_ = detector_param_client_->set_parameters(
      {param}, [this, param](const ResultFuturePtr & results) {
        for (const auto & result : results.get()) {
          if (!result.successful) {
            RCLCPP_ERROR(get_logger(), "Failed to set parameter: %s", result.reason.c_str());
            return;
          }
        }
        RCLCPP_INFO(get_logger(), "Successfully set detect_color to %ld!", param.as_int());
        initial_set_param_ = true;
      });
  }
}

void RMSerialDriver::resetTracker()
{
  if (!reset_tracker_client_->service_is_ready()) {
    RCLCPP_WARN(get_logger(), "Service not ready, skipping tracker reset");
    return;
  }

  auto request = std::make_shared<std_srvs::srv::Trigger::Request>();
  reset_tracker_client_->async_send_request(request);
  RCLCPP_INFO(get_logger(), "Reset tracker!");
}

}  // namespace rm_serial_driver

#include "rclcpp_components/register_node_macro.hpp"

// Register the component with class_loader.
// This acts as a sort of entry point, allowing the component to be discoverable when its library
// is being loaded into a running process.
RCLCPP_COMPONENTS_REGISTER_NODE(rm_serial_driver::RMSerialDriver)
