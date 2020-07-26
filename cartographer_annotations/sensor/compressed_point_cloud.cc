
#include "cartographer/sensor/compressed_point_cloud.h"

#include <limits>

#include "cartographer/common/math.h"
#include "cartographer/mapping_3d/hybrid_grid.h"

namespace cartographer {
namespace sensor {

namespace {

// Points are encoded on a fixed grid with a grid spacing of 'kPrecision' with
// integers. Points are organized in blocks, where each point is encoded
// relative to the block's origin in an int32 with 'kBitsPerCoordinate' bits per
// coordinate.
constexpr float kPrecision = 0.001f;  // in meters.
constexpr int kBitsPerCoordinate = 10;
constexpr int kCoordinateMask = (1 << kBitsPerCoordinate) - 1;
constexpr int kMaxBitsPerDirection = 23;

}  // namespace

CompressedPointCloud::ConstIterator::ConstIterator(
    const CompressedPointCloud* compressed_point_cloud)
    : compressed_point_cloud_(compressed_point_cloud),
      remaining_points_(compressed_point_cloud->num_points_),
      remaining_points_in_current_block_(0),
      input_(compressed_point_cloud->point_data_.begin()) {
  if (remaining_points_ > 0) {
    ReadNextPoint();
  }
}

CompressedPointCloud::ConstIterator
CompressedPointCloud::ConstIterator::EndIterator(
    const CompressedPointCloud* compressed_point_cloud) {
  ConstIterator end_iterator(compressed_point_cloud);
  end_iterator.remaining_points_ = 0;
  return end_iterator;
}

Eigen::Vector3f CompressedPointCloud::ConstIterator::operator*() const {
  CHECK_GT(remaining_points_, 0);
  return current_point_;
}

CompressedPointCloud::ConstIterator& CompressedPointCloud::ConstIterator::
operator++() {
  --remaining_points_;
  if (remaining_points_ > 0) {
    ReadNextPoint();
  }
  return *this;
}

bool CompressedPointCloud::ConstIterator::operator!=(
    const ConstIterator& it) const {
  CHECK(compressed_point_cloud_ == it.compressed_point_cloud_);
  return remaining_points_ != it.remaining_points_;
}

void CompressedPointCloud::ConstIterator::ReadNextPoint() {
  if (remaining_points_in_current_block_ == 0) {
    remaining_points_in_current_block_ = *input_++;
    for (int i = 0; i < 3; ++i) {
      current_block_coordinates_[i] = *input_++ << kBitsPerCoordinate;
    }
  }
  --remaining_points_in_current_block_;
  const int point = *input_++;
  constexpr int kMask = (1 << kBitsPerCoordinate) - 1;
  current_point_[0] =
      (current_block_coordinates_[0] + (point & kMask)) * kPrecision;
  current_point_[1] = (current_block_coordinates_[1] +
                       ((point >> kBitsPerCoordinate) & kMask)) *
                      kPrecision;
  current_point_[2] =
      (current_block_coordinates_[2] + (point >> (2 * kBitsPerCoordinate))) *
      kPrecision;
}

/*
最重要的构造函数
压缩点云
*/
CompressedPointCloud::CompressedPointCloud(const PointCloud& point_cloud)
//point_cloud是一个3f的vector,压缩到point_data_中储存
    : num_points_(point_cloud.size()) {
  // Distribute points into blocks.
  struct RasterPoint {
    Eigen::Array3i point; //  Array3i (int d1, int d2, int d3)
    int index;
  };
  using Blocks = mapping_3d::HybridGridBase<std::vector<RasterPoint>>;
  Blocks blocks(kPrecision);
  int num_blocks = 0;
  CHECK_LE(point_cloud.size(), std::numeric_limits<int>::max());
  for (int point_index = 0; point_index < static_cast<int>(point_cloud.size());
       ++point_index) {
    const Eigen::Vector3f& point = point_cloud[point_index];//获取某个point{x,y,z}
    CHECK_LT(point.cwiseAbs().maxCoeff() / kPrecision,
             1 << kMaxBitsPerDirection)
        << "Point out of bounds: " << point;
    Eigen::Array3i raster_point;
    Eigen::Array3i block_coordinate;
    for (int i = 0; i < 3; ++i) {
      raster_point[i] = common::RoundToInt(point[i] / kPrecision);
      block_coordinate[i] = raster_point[i] >> kBitsPerCoordinate;
      raster_point[i] &= kCoordinateMask;
    }
    auto* const block = blocks.mutable_value(block_coordinate);
    num_blocks += block->empty();
    block->push_back({raster_point, point_index});
  } //end for

  // Encode blocks.
  point_data_.reserve(4 * num_blocks + point_cloud.size());
  for (Blocks::Iterator it(blocks); !it.Done(); it.Next(), --num_blocks) {
    const auto& raster_points = it.GetValue();
    CHECK_LE(raster_points.size(), std::numeric_limits<int32>::max());
    point_data_.push_back(raster_points.size());
    const Eigen::Array3i block_coordinate = it.GetCellIndex();
    point_data_.push_back(block_coordinate.x());
    point_data_.push_back(block_coordinate.y());
    point_data_.push_back(block_coordinate.z());
    for (const RasterPoint& raster_point : raster_points) {
      point_data_.push_back((((raster_point.point.z() << kBitsPerCoordinate) +
                              raster_point.point.y())
                             << kBitsPerCoordinate) +
                            raster_point.point.x());
    }
  }
  CHECK_EQ(num_blocks, 0);
}

/*私有的构造函数,外部不能调用*/
CompressedPointCloud::CompressedPointCloud(const std::vector<int32>& point_data,
                                           size_t num_points)
    : point_data_(point_data), num_points_(num_points) {}

bool CompressedPointCloud::empty() const { return num_points_ == 0; }

size_t CompressedPointCloud::size() const { return num_points_; }

CompressedPointCloud::ConstIterator CompressedPointCloud::begin() const {//迭代器首,
  return ConstIterator(this);
}

CompressedPointCloud::ConstIterator CompressedPointCloud::end() const { //迭代器尾,
  return ConstIterator::EndIterator(this);
}

PointCloud CompressedPointCloud::Decompress() const {
  PointCloud decompressed;                    //Vector3f组成的vector
  for (const Eigen::Vector3f& point : *this) { //此处调用的是迭代器函数,begin(),end()
    decompressed.push_back(point);
  }
  return decompressed;
}

proto::CompressedPointCloud CompressedPointCloud::ToProto() const {
  proto::CompressedPointCloud result;
  result.set_num_points(num_points_);       //序列化点云的个数
  for (const int32 data : point_data_) {
    result.add_point_data(data);            //依次序添加数据 
  }
  return result;
}

}  // namespace sensor
}  // namespace cartographer
