
#ifndef CARTOGRAPHER_MAPPING_GLOBAL_TRAJECTORY_BUILDER_INTERFACE_H_
#define CARTOGRAPHER_MAPPING_GLOBAL_TRAJECTORY_BUILDER_INTERFACE_H_

#include <functional>
#include <memory>
#include <string>

#include "cartographer/common/time.h"
#include "cartographer/mapping/submaps.h"
#include "cartographer/mapping/trajectory_builder.h"
#include "cartographer/sensor/point_cloud.h"
#include "cartographer/sensor/range_data.h"

namespace cartographer {
namespace mapping {

/*
虚基类,提供一个接口用于2d和3d的slam,不可拷贝/赋值

*/
// This interface is used for both 2D and 3D SLAM. Implementations wire up a
// global SLAM stack, i.e. local SLAM for initial pose estimates, scan matching
// to detect loop closure, and a sparse pose graph optimization to compute
// optimized pose estimates.
class GlobalTrajectoryBuilderInterface {
 public:
  using PoseEstimate = TrajectoryBuilder::PoseEstimate;//位姿估计

  GlobalTrajectoryBuilderInterface() {}
  virtual ~GlobalTrajectoryBuilderInterface() {}

  GlobalTrajectoryBuilderInterface(const GlobalTrajectoryBuilderInterface&) =
      delete;
  GlobalTrajectoryBuilderInterface& operator=(
      const GlobalTrajectoryBuilderInterface&) = delete;

  virtual const Submaps* submaps() const = 0;
  virtual const PoseEstimate& pose_estimate() const = 0;

  virtual void AddRangefinderData(common::Time time,
                                  const Eigen::Vector3f& origin,
                                  const sensor::PointCloud& ranges) = 0;
  virtual void AddImuData(common::Time time,
                          const Eigen::Vector3d& linear_acceleration,
                          const Eigen::Vector3d& angular_velocity) = 0;
  virtual void AddOdometerData(common::Time time,
                               const transform::Rigid3d& pose) = 0;
};

}  // namespace mapping
}  // namespace cartographer

#endif  // CARTOGRAPHER_MAPPING_GLOBAL_TRAJECTORY_BUILDER_INTERFACE_H_
