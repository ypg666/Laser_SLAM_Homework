
#ifndef CARTOGRAPHER_MAPPING_COLLATED_TRAJECTORY_BUILDER_H_
#define CARTOGRAPHER_MAPPING_COLLATED_TRAJECTORY_BUILDER_H_

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "cartographer/common/port.h"
#include "cartographer/common/rate_timer.h"
#include "cartographer/mapping/global_trajectory_builder_interface.h"
#include "cartographer/mapping/submaps.h"
#include "cartographer/mapping/trajectory_builder.h"
#include "cartographer/sensor/collator.h"
#include "cartographer/sensor/data.h"

namespace cartographer {
namespace mapping {
/*

CollatedTrajectoryBuilder类继承自TrajectoryBuilder,
作用：使用Collator处理从传感器收集而来的数据.
并传递给GlobalTrajectoryBuilderInterface。

数据成员包括

sensor::Collator* const sensor_collator_; 传感器收集类实例
const int trajectory_id_;                 轨迹id
std::unique_ptr<GlobalTrajectoryBuilderInterface> wrapped_trajectory_builder_;全局的建图接口
 
std::chrono::steady_clock::time_point last_logging_time_; 上一次传递数据时间
std::map<string, common::RateTimer<>> rate_timers_;频率

成员函数,重写了父类的接口
submaps()用于建立子图
pose_estimate() 位姿估计
AddSensorData() 添加传感器数据
HandleCollatedSensorData() 处理收集得来的传感器数据


Handles collating sensor data using a sensor::Collator,
then passing it on toa mapping::GlobalTrajectoryBuilderInterface which is common for 2D and 3D.

*/
class CollatedTrajectoryBuilder : public TrajectoryBuilder {
 public:
  CollatedTrajectoryBuilder(
      sensor::Collator* sensor_collator, int trajectory_id,
      const std::unordered_set<string>& expected_sensor_ids,
      std::unique_ptr<GlobalTrajectoryBuilderInterface>
          wrapped_trajectory_builder);
  ~CollatedTrajectoryBuilder() override;

  CollatedTrajectoryBuilder(const CollatedTrajectoryBuilder&) = delete;
  CollatedTrajectoryBuilder& operator=(const CollatedTrajectoryBuilder&) =
      delete;

  const Submaps* submaps() const override;
  const PoseEstimate& pose_estimate() const override;

  void AddSensorData(const string& sensor_id,
                     std::unique_ptr<sensor::Data> data) override;

 private:
  void HandleCollatedSensorData(const string& sensor_id,
                                std::unique_ptr<sensor::Data> data);

  sensor::Collator* const sensor_collator_;
  const int trajectory_id_;
  std::unique_ptr<GlobalTrajectoryBuilderInterface> wrapped_trajectory_builder_;

  // Time at which we last logged the rates of incoming sensor data.
  std::chrono::steady_clock::time_point last_logging_time_;
  std::map<string, common::RateTimer<>> rate_timers_;
};

}  // namespace mapping
}  // namespace cartographer

#endif  // CARTOGRAPHER_MAPPING_COLLATED_TRAJECTORY_BUILDER_H_
