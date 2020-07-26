
#include "cartographer/transform/transform.h"

#include <random>

#include "cartographer/transform/rigid_transform.h"
#include "cartographer/transform/rigid_transform_test_helpers.h"
#include "gtest/gtest.h"

namespace cartographer {
namespace transform {
namespace {

TEST(TransformTest, GetAngle) {
  std::mt19937 rng(42);//随机数种子
  std::uniform_real_distribution<float> angle_distribution(0.f, M_PI);
     //[0- pi],概率均匀分布
  std::uniform_real_distribution<float> position_distribution(-1.f, 1.f);
     //[-1 ,1]

  for (int i = 0; i != 100; ++i) {
    const float angle = angle_distribution(rng);
    const float x = position_distribution(rng);
    const float y = position_distribution(rng);
    const float z = position_distribution(rng);
    const Eigen::Vector3f axis = Eigen::Vector3f(x, y, z).normalized();
    //, for vectors, the l2 norm of *this, and for matrices the Frobenius norm. 
    
    //给定角度绕axis旋转，得到Rigid3f，再求Rigid3d的angle。2者理论上应该相同
    EXPECT_NEAR(angle,
                GetAngle(Rigid3f::Rotation(AngleAxisVectorToRotationQuaternion(
                    Eigen::Vector3f(angle * axis)))),
                1e-6f);
  }
}

TEST(TransformTest, GetYaw) {
  const auto rotation =
      Rigid3d::Rotation(Eigen::AngleAxisd(1.2345, Eigen::Vector3d::UnitZ()));
  EXPECT_NEAR(1.2345, GetYaw(rotation), 1e-9);
  EXPECT_NEAR(-1.2345, GetYaw(rotation.inverse()), 1e-9);//四元数的逆就是共轭。
}

TEST(TransformTest, GetYawAxisOrdering) {
  //乘法顺序不能颠倒。zyx
  const auto rotation =
      Rigid3d::Rotation(Eigen::AngleAxisd(1.2345, Eigen::Vector3d::UnitZ()) *
                        Eigen::AngleAxisd(0.4321, Eigen::Vector3d::UnitY()) *
                        Eigen::AngleAxisd(0.6789, Eigen::Vector3d::UnitX()));
  EXPECT_NEAR(1.2345, GetYaw(rotation), 1e-9);
}

TEST(TransformTest, Embed3D) {
  const Rigid2d rigid2d({1., 2.}, 0.);
  const Rigid3d rigid3d(
      Rigid3d::Translation(Eigen::Vector3d(1., 2., 3.)) *
      Rigid3d::Rotation(Eigen::Quaterniond(1., 2., 3., 4.).normalized()));
  const Rigid3d expected =
      rigid3d * Rigid3d::Translation(Eigen::Vector3d(1., 2., 0.));
  EXPECT_THAT(expected, IsNearly(rigid3d * Embed3D(rigid2d), 1e-9));
}

}  // namespace
}  // namespace transform
}  // namespace cartographer
