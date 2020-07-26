#include "cartographer/io/xyz_writing_points_processor.h"

#include <iomanip>

#include "cartographer/common/make_unique.h"
#include "glog/logging.h"

namespace cartographer {
namespace io {

namespace {

/*
将x,y,x以字符串形式写入FileWriter
*/
void WriteXyzPoint(const Eigen::Vector3f& point,
                   FileWriter* const file_writer) {
  std::ostringstream stream;
  stream << std::setprecision(6);
  stream << point.x() << " " << point.y() << " " << point.z() << "\n";
  const string out = stream.str();
  CHECK(file_writer->Write(out.data(), out.size()));
}

}  // namespace

/*
构造函数初始化
*/
XyzWriterPointsProcessor::XyzWriterPointsProcessor(
    std::unique_ptr<FileWriter> file_writer, PointsProcessor* const next)
    : next_(next), file_writer_(std::move(file_writer)) {}

/*
根据filename返回指向文件名的file_writer_factory
*/
std::unique_ptr<XyzWriterPointsProcessor>
XyzWriterPointsProcessor::FromDictionary
(
    FileWriterFactory file_writer_factory,
    common::LuaParameterDictionary* const dictionary,
    PointsProcessor* const next) 
{
  return common::make_unique<XyzWriterPointsProcessor>(
      file_writer_factory(dictionary->GetString("filename")), next);
}

/*刷新缓冲*/
PointsProcessor::FlushResult XyzWriterPointsProcessor::Flush() {
  CHECK(file_writer_->Close()) << "Closing XYZ file failed.";
  switch (next_->Flush()) {
    case FlushResult::kFinished:
      return FlushResult::kFinished;

    case FlushResult::kRestartStream:
      LOG(FATAL) << "XYZ generation must be configured to occur after any "
                    "stages that require multiple passes.";
  }
  LOG(FATAL);
}

/*
主逻辑,以{x,y,z}写入文件
*/
void XyzWriterPointsProcessor::Process(std::unique_ptr<PointsBatch> batch) {
  for (const Eigen::Vector3f& point : batch->points) {
    WriteXyzPoint(point, file_writer_.get());
  }
  next_->Process(std::move(batch));
}

}  // namespace io
}  // namespace cartographer
