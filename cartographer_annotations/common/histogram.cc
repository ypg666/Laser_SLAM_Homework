

#include "cartographer/common/histogram.h"

#include <algorithm>
#include <numeric>
#include <string>

#include "cartographer/common/port.h"
#include "glog/logging.h"

namespace cartographer {
namespace common {

//添加元素
void Histogram::Add(const float value) { values_.push_back(value); }

//以bin大小是buckets转成 string,输出 直方图信息.
string Histogram::ToString(const int buckets) const {
  CHECK_GE(buckets, 1);      //篮子个数必须>=1
  if (values_.empty()) {
    return "Count: 0";
  }
  const float min = *std::min_element(values_.begin(), values_.end());
  const float max = *std::max_element(values_.begin(), values_.end());
  const float mean =
      std::accumulate(values_.begin(), values_.end(), 0.f) / values_.size();
  string result = "Count: " + std::to_string(values_.size()) +
                  "  Min: " + std::to_string(min) +
                  "  Max: " + std::to_string(max) +
                  "  Mean: " + std::to_string(mean);
  if (min == max) { //最大等于最小直接返回.
    return result;
  }
  CHECK_LT(min, max);
  float lower_bound = min; //bin区间下界
  int total_count = 0;
  for (int i = 0; i != buckets; ++i) { //根据bin个数
    const float upper_bound =  //bin区间上界
        (i + 1 == buckets)
            ? max
            : (max * (i + 1) / buckets + min * (buckets - i - 1) / buckets);
    int count = 0;
    for (const float value : values_) { //对每一个vector<float>的值
      if (lower_bound <= value &&
          (i + 1 == buckets ? value <= upper_bound : value < upper_bound)) {
        ++count;
      }
    }
    total_count += count;
    result += "\n[" + std::to_string(lower_bound) + ", " +
              std::to_string(upper_bound) + ((i + 1 == buckets) ? "]" : ")");
    constexpr int kMaxBarChars = 20;
    const int bar =
        (count * kMaxBarChars + values_.size() / 2) / values_.size();
    result += "\t";
    for (int i = 0; i != kMaxBarChars; ++i) {
      result += (i < (kMaxBarChars - bar)) ? " " : "#";
    }
    result += "\tCount: " + std::to_string(count) + " (" +
              std::to_string(count * 1e2f / values_.size()) + "%)";
    result += "\tTotal: " + std::to_string(total_count) + " (" +
              std::to_string(total_count * 1e2f / values_.size()) + "%)";
    lower_bound = upper_bound;
  }
  return result;
}

}  // namespace common
}  // namespace cartographer
