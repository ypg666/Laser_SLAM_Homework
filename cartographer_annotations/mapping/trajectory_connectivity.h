
#ifndef CARTOGRAPHER_MAPPING_TRAJECTORY_CONNECTIVITY_H_
#define CARTOGRAPHER_MAPPING_TRAJECTORY_CONNECTIVITY_H_

#include <map>
#include <unordered_map>

#include "cartographer/common/mutex.h"
#include "cartographer/mapping/proto/trajectory_connectivity.pb.h"
#include "cartographer/mapping/submaps.h"

namespace cartographer {
namespace mapping {

/*

TrajectoryConnectivity用于解决不同轨迹线的连通性问题.

多条轨迹构成一颗森林，而相互联通的轨迹应该合并。

不可拷贝/赋值
包含3个数据成员
1,互斥锁lock_
2,forest_  不同轨迹线组成的森林
3,connection_map_ 连通图
成员函数
Add():添加一条轨迹线
Connect():将2条轨迹线联通
TransitivelyConnected():判断是否处于同一个连通域
ConnectionCount():返回直接联通的数量

the transitive connectivity:传递连通性

ConnectedComponents():由联通分量id组成的已联通分类组
*/
// A class that tracks the connectivity structure between trajectories.
//
// Connectivity includes both the count ("How many times have I _directly_
// connected trajectories i and j?") and the transitive connectivity.
//
// This class is thread-safe.
class TrajectoryConnectivity {
 public:
  TrajectoryConnectivity();

  TrajectoryConnectivity(const TrajectoryConnectivity&) = delete;
  TrajectoryConnectivity& operator=(const TrajectoryConnectivity&) = delete;

  // Add a trajectory which is initially connected to nothing.
  //添加一条轨迹线，默认不连接到任何轨迹线
  void Add(int trajectory_id) EXCLUDES(lock_);

 /*

 Connect two trajectories. If either trajectory is untracked, it will be  tracked.  This function is invariant to the order of its arguments. 
 Repeated calls to Connect increment the connectivity count.
将轨迹a和轨迹b联通
  */
  void Connect(int trajectory_id_a, int trajectory_id_b) EXCLUDES(lock_);

  // Determines if two trajectories have been (transitively) connected. If
  // either trajectory is not being tracked, returns false. This function is
  // invariant to the order of its arguments.
  //:判断是否处于同一个连通域
  bool TransitivelyConnected(int trajectory_id_a, int trajectory_id_b)
      EXCLUDES(lock_);

  // Return the number of _direct_ connections between 'trajectory_id_a' and
  // 'trajectory_id_b'. If either trajectory is not being tracked, returns 0.
  // This function is invariant to the order of its arguments.
  //返回直接联通的数量
  int ConnectionCount(int trajectory_id_a, int trajectory_id_b) EXCLUDES(lock_);

  // The trajectory IDs, grouped by connectivity.
  std::vector<std::vector<int>> ConnectedComponents() EXCLUDES(lock_);

 private:
  // Find the representative and compresses the path to it.
  int FindSet(int trajectory_id) REQUIRES(lock_);
  void Union(int trajectory_id_a, int trajectory_id_b) REQUIRES(lock_);

  common::Mutex lock_; //互斥锁

  // Tracks transitive connectivity using a disjoint set forest, i.e. each
  // entry points towards the representative for the given trajectory.
  // 不同轨迹线组成的森林，即连通域问题。
  std::map<int, int> forest_ GUARDED_BY(lock_); 

  
  // Tracks the number of direct connections between a pair of trajectories.
  //直接链接的轨迹线
  std::map<std::pair<int, int>, int> connection_map_ GUARDED_BY(lock_);
};


// Returns a proto encoding connected components.编码已联通成分到proto文件
proto::TrajectoryConnectivity ToProto(
    std::vector<std::vector<int>> connected_components);

// Returns the connected component containing 'trajectory_index'. 
//返回连接到联通id的所有联通分量。
proto::TrajectoryConnectivity::ConnectedComponent FindConnectedComponent(
    const cartographer::mapping::proto::TrajectoryConnectivity&
        trajectory_connectivity,
    int trajectory_id);

}  // namespace mapping
}  // namespace cartographer

#endif  // CARTOGRAPHER_MAPPING_TRAJECTORY_CONNECTIVITY_H_
