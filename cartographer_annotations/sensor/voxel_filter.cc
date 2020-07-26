
#include "cartographer/sensor/voxel_filter.h"

#include <cmath>

#include "cartographer/common/math.h"

namespace cartographer {
namespace sensor {

namespace {

/*
对点云数据，根据指定范围max_range进行l2距离过滤，
形参：   points，
返回值： points
*/
PointCloud FilterByMaxRange(const PointCloud& point_cloud,
                            const float max_range) {
  PointCloud result;
  for (const Eigen::Vector3f& point : point_cloud) {
    if (point.norm() <= max_range) { // l2范数,向量各个元素平方和的1/2次方
      result.push_back(point);
    }
  }
  return result;
}

/*
自适应滤波函数。
形参1：滤波器option，
形参2：待滤波的points
返回值：points

voxels_：表征一系列的Cell，每个Cell有一个size，piont以size为间距分布，实现稀疏表示。
Cell的数量和即为稀疏表达的point的数量和。

滤波实现：
1，调整edge的大小，以使每个Cell占据的points个数在min_num_points左右,以实现稀疏表达。
2，按照l2范数测度。
3，

*/
PointCloud AdaptivelyVoxelFiltered(
    const proto::AdaptiveVoxelFilterOptions& options,
    const PointCloud& point_cloud) {
  if (point_cloud.size() <= options.min_num_points()) {
    // 'point_cloud' is already sparse enough.
    return point_cloud;
  }

  //max_length即为体素edge的max_size。下面以max_size为间距，稀疏提取point。
  PointCloud result = VoxelFiltered(point_cloud, options.max_length());

  if (result.size() >= options.min_num_points()) {
    //即使以max_size为间距也超过min_num.则直接返回。
    // Filtering with 'max_length' resulted in a sufficiently dense point cloud.
    return result;
  }
  // Search for a 'low_length' that is known to result in a sufficiently
  // dense point cloud. We give up and use the full 'point_cloud' if reducing
  // the edge length by a factor of 1e-2 is not enough.

/*
以max_size为间距没有超过min_num.则继续减小edge大小，使得以low_length为间距。

low_length是每个Cell的长度。
每个Cell“占据”的points不能超过min_num.
二分查找法：
for()循环每次将edge减半
对每一个low_length:
while()循环控制edge不能小于10%
满足要求则记录，否则将high折半。

结果：
每个Cell最少“占据”min_num个points。
*/
  for (float high_length = options.max_length();
       high_length > 1e-2f * options.max_length(); high_length /= 2.f) {
    float low_length = high_length / 2.f;
    result = VoxelFiltered(point_cloud, low_length);//edge大小为low_length

    if (result.size() >= options.min_num_points()) {
      // Binary search to find the right amount of filtering. 'low_length' gave
      // a sufficiently dense 'result', 'high_length' did not. We stop when the
      // edge length is at most 10% off.
      while ((high_length - low_length) / low_length > 1e-1f) {
        const float mid_length = (low_length + high_length) / 2.f;
        const PointCloud candidate = VoxelFiltered(point_cloud, mid_length);
        if (candidate.size() >= options.min_num_points()) {
          low_length = mid_length;
          result = candidate;
        } else {
          high_length = mid_length;
        }
      }
      return result;
    }
  }
  return result;
}

}  // namespace

/*
形参1：pionts
形参2：size，体素大小
返回值：稀疏点云，points
*/
PointCloud VoxelFiltered(const PointCloud& point_cloud, const float size) {
  VoxelFilter voxel_filter(size);//创建滤波器
  voxel_filter.InsertPointCloud(point_cloud);
  return voxel_filter.point_cloud();
}

VoxelFilter::VoxelFilter(const float size) : voxels_(size) {}

//插入点云
void VoxelFilter::InsertPointCloud(const PointCloud& point_cloud) {
  for (const Eigen::Vector3f& point : point_cloud) {
/*对每一个点云point,如果落在Cell中，则插入piont，并且后续不在接收落在此Cell的point。

[point1] ,[point2],[point3],[point4]....
[point11] ,[point12],[point13],[point14]....
*/
    auto* const value = voxels_.mutable_value(voxels_.GetCellIndex(point));
    if (*value == 0) {
      point_cloud_.push_back(point);
      *value = 1;
    }
  }
}

//表征Cell边界的point
const PointCloud& VoxelFilter::point_cloud() const { return point_cloud_; }


/*
sparse_pose_graph.lua/trajectory_builder_2d.lua
adaptive_voxel_filter = {
      max_length = 0.9,
      min_num_points = 100,
      max_range = 50.,
    },

2d:adaptive_voxel_filter = {
    max_length = 0.5,
    min_num_points = 200,
    max_range = 50.,
  },

根据.lua文件选项，配置options滤波器参数。

*/
proto::AdaptiveVoxelFilterOptions CreateAdaptiveVoxelFilterOptions(
    common::LuaParameterDictionary* const parameter_dictionary) {
  proto::AdaptiveVoxelFilterOptions options;
  options.set_max_length(parameter_dictionary->GetDouble("max_length"));
  options.set_min_num_points(
      parameter_dictionary->GetNonNegativeInt("min_num_points"));
  options.set_max_range(parameter_dictionary->GetDouble("max_range"));
  return options;
}

//构造函数
AdaptiveVoxelFilter::AdaptiveVoxelFilter(
    const proto::AdaptiveVoxelFilterOptions& options)
    : options_(options) {}


//将point进行自适应滤波，返回point。
PointCloud AdaptiveVoxelFilter::Filter(const PointCloud& point_cloud) const {
  return AdaptivelyVoxelFiltered(
      options_, FilterByMaxRange(point_cloud, options_.max_range()));
}

}  // namespace sensor
}  // namespace cartographer


/*
  3d：
  high_resolution_adaptive_voxel_filter = {
    max_length = 2.,
    min_num_points = 150,
    max_range = 15.,
  },

  low_resolution_adaptive_voxel_filter = {
    max_length = 4.,
    min_num_points = 200,
    max_range = MAX_3D_RANGE,
  }, 

*/