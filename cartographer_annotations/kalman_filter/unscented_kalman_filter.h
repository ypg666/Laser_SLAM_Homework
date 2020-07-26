
#ifndef CARTOGRAPHER_KALMAN_FILTER_UNSCENTED_KALMAN_FILTER_H_
#define CARTOGRAPHER_KALMAN_FILTER_UNSCENTED_KALMAN_FILTER_H_

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "Eigen/Cholesky"
#include "Eigen/Core"
#include "Eigen/Eigenvalues"
#include "cartographer/kalman_filter/gaussian_distribution.h"
#include "glog/logging.h"
/*
无损卡尔曼滤波,去芳香卡尔曼滤波https://en.wikipedia.org/wiki/Kalman_filter
*/
namespace cartographer {
namespace kalman_filter {

//平方,常量表达式
template <typename FloatType>
constexpr FloatType sqr(FloatType a) {
  return a * a;
}

// N*1 || 1*N -> 外积,N*N
template <typename FloatType, int N>
Eigen::Matrix<FloatType, N, N> OuterProduct( 
    const Eigen::Matrix<FloatType, N, 1>& v) {
  return v * v.transpose();
}

// Checks if 'A' is a symmetric matrix.检查A是否是对称矩阵,A减去A的转置~=0
template <typename FloatType, int N>
void CheckSymmetric(const Eigen::Matrix<FloatType, N, N>& A) {
  // This should be pretty much Eigen::Matrix<>::Zero() if the matrix is
  // symmetric.

  //The NaN values are used to identify undefined or non-representable values for floating-point elements, 
  //such as the square root of negative numbers or the result of 0/0.

  const FloatType norm = (A - A.transpose()).norm();
  CHECK(!std::isnan(norm) && std::abs(norm) < 1e-5)
      << "Symmetry check failed with norm: '" << norm << "' from matrix:\n"
      << A;
}

/*
https://zh.wikipedia.org/wiki/%E6%AD%A3%E5%AE%9A%E7%9F%A9%E9%98%B5

几个术语: Av=λv.  λ 为一标量，称为 v 对应的特征值。也称 v 为特征值 λ 对应的特征向量。
eigendecomposition,特征分解,谱分解,是将矩阵分解为由其特征值和特征向量表示的矩阵之积的方法。
需要注意只有对可对角化矩阵才可以施以特征分解。

eigenvalues 特征值,
eigenvectors 特征向量 


返回对称半正定矩阵的平方根B,M=B*B
*/
// Returns the matrix square root of a symmetric positive semidefinite matrix.
template <typename FloatType, int N>
Eigen::Matrix<FloatType, N, N> MatrixSqrt(
    const Eigen::Matrix<FloatType, N, N>& A) {
  CheckSymmetric(A);//必须是对称矩阵

  Eigen::SelfAdjointEigenSolver<Eigen::Matrix<FloatType, N, N>>
      adjoint_eigen_solver((A + A.transpose()) / 2.);
      //A==A的转置，构造特征分解对象。

  const auto& eigenvalues = adjoint_eigen_solver.eigenvalues();//特征值
  CHECK_GT(eigenvalues.minCoeff(), -1e-5)
      << "MatrixSqrt failed with negative eigenvalues: "
      << eigenvalues.transpose();
//Cholesky分解：把一个对称正定的矩阵表示成一个下三角矩阵L和其转置的乘积的分解。
  return adjoint_eigen_solver.eigenvectors() *
         adjoint_eigen_solver.eigenvalues()
             .cwiseMax(Eigen::Matrix<FloatType, N, 1>::Zero())
             .cwiseSqrt()
             .asDiagonal() *
         adjoint_eigen_solver.eigenvectors().transpose();
}

/*
无损卡尔曼滤波
对于雷达来说，人们感兴趣的是其能够跟踪目标。但目标的位置、速度、加速度的测量值往往在任何时候都有噪声。
卡尔曼滤波利用目标的动态信息，设法去掉噪声的影响，得到一个关于目标位置的好的估计。
这个估计可以是对当前目标位置的估计（滤波），也可以是对于将来位置的估计（预测），
也可以是对过去位置的估计（插值或平滑）。

卡尔曼滤波是一种递归的估计，即只要获知上一时刻状态的估计值以及当前状态的观测值就可以计算出当前状态的估计值，
因此不需要记录观测或者估计的历史信息。

卡尔曼滤波器的操作包括两个阶段：预测与更新。在预测阶段，滤波器使用上一状态的估计，做出对当前状态的估计。
在更新阶段，滤波器利用对当前状态的观测值优化在预测阶段获得的预测值，以获得一个更精确的新估计值。


UnscentedKalmanFilter类是根据《Probabilistic Robotics》实现的卡尔曼滤波类，
并且扩展到了处理非线性噪声和传感器。
数据成员:
1),add_delta_
2),compute_delta_

成员函数：
1),Predict():预测
*/

/*
Implementation of a Kalman filter. We follow the nomenclature (过程)from
Thrun, S. et al., Probabilistic Robotics, 2006.
Extended to handle non-additive noise/sensors inspired by Kraft, E., A
Quaternion-based Unscented Kalman Filter for Orientation Tracking.

*/
template <typename FloatType, int N>
class UnscentedKalmanFilter {
 public:
  using StateType = Eigen::Matrix<FloatType, N, 1>;           //状态矩阵N*1
  using StateCovarianceType = Eigen::Matrix<FloatType, N, N>; //协方差矩阵N*N

/*
构造函数,
参数1,N*1矩阵,
参数2,stl函数对象 add_delta(默认),
参数3,stl函数对象 compute_delta(默认),
*/
  explicit UnscentedKalmanFilter(
      const GaussianDistribution<FloatType, N>& initial_belief,                 //参数1

      std::function<StateType(const StateType& state, const StateType& delta)>  //参数2
          add_delta = [](const StateType& state,
                         const StateType& delta) { return state + delta; },

      std::function<StateType(const StateType& origin, const StateType& target)> //参数3
          compute_delta =
              [](const StateType& origin, const StateType& target) {
                return target - origin;
              })
      : belief_(initial_belief),
        add_delta_(add_delta),
        compute_delta_(compute_delta) {}

/*
预测step。使用卡尔曼滤波对当前状态进行预测。

参数1是std函数对象g, 作用是状态转移方程
参数2是高斯分布的“ε”,控制和模型噪声的线性组合。

*/
  // Does the control/prediction step for the filter. The control must be
  // implicitly added by the function g which also does the state transition.
  // 'epsilon' is the additive combination of control and model noise.
  void Predict(std::function<StateType(const StateType&)> g,
               const GaussianDistribution<FloatType, N>& epsilon) {
    CheckSymmetric(epsilon.GetCovariance());

    // Get the state mean and matrix root of its covariance.
    const StateType& mu = belief_.GetMean();
    const StateCovarianceType sqrt_sigma = MatrixSqrt(belief_.GetCovariance());//N*N

    std::vector<StateType> Y;//需要计算的状态矩阵，N*1矩阵。
    Y.reserve(2 * N + 1);//公式：p65
    Y.emplace_back(g(mu));//状态转移方程,公式p65:3.68,p70:3
/*
按照公式p65,3.66计算：
1),mu是u,
2),add_delta_()得到Xi,
3),g(Xi)得到Yi,
4),ComputeMean()得到 u',
5),更新Σ得到new_sigma
6),返回新的高斯分布。均值是new_mu,方差是new_sigma。二者再加上epsilon。
*/
    const FloatType kSqrtNPlusLambda = std::sqrt(N + kLambda);
    for (int i = 0; i < N; ++i) {
      // Order does not matter here as all have the same weights in the
      // summation later on anyways.
      Y.emplace_back(g(add_delta_(mu, kSqrtNPlusLambda * sqrt_sigma.col(i))));
      Y.emplace_back(g(add_delta_(mu, -kSqrtNPlusLambda * sqrt_sigma.col(i))));
    }
    const StateType new_mu = ComputeMean(Y);

    StateCovarianceType new_sigma =
        kCovWeight0 * OuterProduct<FloatType, N>(compute_delta_(new_mu, Y[0]));
    for (int i = 0; i < N; ++i) {
      new_sigma += kCovWeightI * OuterProduct<FloatType, N>(
                                     compute_delta_(new_mu, Y[2 * i + 1]));
      new_sigma += kCovWeightI * OuterProduct<FloatType, N>(
                                     compute_delta_(new_mu, Y[2 * i + 2]));
    }
    CheckSymmetric(new_sigma);

    belief_ = GaussianDistribution<FloatType, N>(new_mu, new_sigma) + epsilon;//更新状态
  }

/*
观察step.
参数1:stl函数对象h
参数2:高斯分布的测量噪声delta,均值为0

按照《Probabilistic Robotics》p70,的公式(7)-(14)计算。

  template <int K>:k是欲观测的变量数。
1),看书
2),...
3),...
4),...
5),...
6),...
*/
  // The observation step of the Kalman filter. 'h' transfers the state
  // into an observation that should be zero, i.e., the sensor readings should
  // be included in this function already. 'delta' is the measurement noise and
  // must have zero mean.
  template <int K>
  void Observe(
      std::function<Eigen::Matrix<FloatType, K, 1>(const StateType&)> h,
      const GaussianDistribution<FloatType, K>& delta) {
    CheckSymmetric(delta.GetCovariance());
    // We expect zero mean delta.
    CHECK_NEAR(delta.GetMean().norm(), 0., 1e-9);

    // Get the state mean and matrix root of its covariance.
    const StateType& mu = belief_.GetMean();
    const StateCovarianceType sqrt_sigma = MatrixSqrt(belief_.GetCovariance());

    // As in Kraft's paper, we compute W containing the zero-mean sigma points,
    // since this is all we need.
    std::vector<StateType> W;
    W.reserve(2 * N + 1);
    W.emplace_back(StateType::Zero());

    std::vector<Eigen::Matrix<FloatType, K, 1>> Z;
    Z.reserve(2 * N + 1);
    Z.emplace_back(h(mu));

    Eigen::Matrix<FloatType, K, 1> z_hat = kMeanWeight0 * Z[0];
    const FloatType kSqrtNPlusLambda = std::sqrt(N + kLambda);
    for (int i = 0; i < N; ++i) {
      // Order does not matter here as all have the same weights in the
      // summation later on anyways.
      W.emplace_back(kSqrtNPlusLambda * sqrt_sigma.col(i));
      Z.emplace_back(h(add_delta_(mu, W.back())));

      W.emplace_back(-kSqrtNPlusLambda * sqrt_sigma.col(i));
      Z.emplace_back(h(add_delta_(mu, W.back())));

      z_hat += kMeanWeightI * Z[2 * i + 1];
      z_hat += kMeanWeightI * Z[2 * i + 2];
    }

    Eigen::Matrix<FloatType, K, K> S =
        kCovWeight0 * OuterProduct<FloatType, K>(Z[0] - z_hat);
    for (int i = 0; i < N; ++i) {
      S += kCovWeightI * OuterProduct<FloatType, K>(Z[2 * i + 1] - z_hat);
      S += kCovWeightI * OuterProduct<FloatType, K>(Z[2 * i + 2] - z_hat);
    }
    CheckSymmetric(S);
    S += delta.GetCovariance();

    Eigen::Matrix<FloatType, N, K> sigma_bar_xz =
        kCovWeight0 * W[0] * (Z[0] - z_hat).transpose();
    for (int i = 0; i < N; ++i) {
      sigma_bar_xz +=
          kCovWeightI * W[2 * i + 1] * (Z[2 * i + 1] - z_hat).transpose();
      sigma_bar_xz +=
          kCovWeightI * W[2 * i + 2] * (Z[2 * i + 2] - z_hat).transpose();
    }

    const Eigen::Matrix<FloatType, N, K> kalman_gain =
        sigma_bar_xz * S.inverse();
    const StateCovarianceType new_sigma =
        belief_.GetCovariance() - kalman_gain * S * kalman_gain.transpose();
    CheckSymmetric(new_sigma);

    belief_ = GaussianDistribution<FloatType, N>(
        add_delta_(mu, kalman_gain * -z_hat), new_sigma);
  }

  const GaussianDistribution<FloatType, N>& GetBelief() const {
    return belief_;
  }

 private:
  //计算带权重的偏差
  StateType ComputeWeightedError(const StateType& mean_estimate,
                                 const std::vector<StateType>& states) {
    StateType weighted_error =
        kMeanWeight0 * compute_delta_(mean_estimate, states[0]);
    for (int i = 1; i != 2 * N + 1; ++i) {
      weighted_error += kMeanWeightI * compute_delta_(mean_estimate, states[i]);
    }
    return weighted_error;
  }

  // Algorithm for computing the mean of non-additive states taken from Kraft's
  // Section 3.4, adapted to our implementation.
  //计算均值
  StateType ComputeMean(const std::vector<StateType>& states) {
    CHECK_EQ(states.size(), 2 * N + 1);
    StateType current_estimate = states[0];
    StateType weighted_error = ComputeWeightedError(current_estimate, states);
    int iterations = 0;
    while (weighted_error.norm() > 1e-9) {
      double step_size = 1.;
      while (true) {
        const StateType next_estimate =
            add_delta_(current_estimate, step_size * weighted_error);
        const StateType next_error =
            ComputeWeightedError(next_estimate, states);
        if (next_error.norm() < weighted_error.norm()) {
          current_estimate = next_estimate;
          weighted_error = next_error;
          break;
        }
        step_size *= 0.5;
        CHECK_GT(step_size, 1e-3) << "Step size too small, line search failed.";
      }
      ++iterations;
      CHECK_LT(iterations, 20) << "Too many iterations.";
    }
    return current_estimate;
  }

  // According to Wikipedia these are the normal values. Thrun does not
  // mention those.
  constexpr static FloatType kAlpha = 1e-3;
  constexpr static FloatType kKappa = 0.;
  constexpr static FloatType kBeta = 2.;
  constexpr static FloatType kLambda = sqr(kAlpha) * (N + kKappa) - N;
  constexpr static FloatType kMeanWeight0 = kLambda / (N + kLambda);
  constexpr static FloatType kCovWeight0 =
      kLambda / (N + kLambda) + (1. - sqr(kAlpha) + kBeta);
  constexpr static FloatType kMeanWeightI = 1. / (2. * (N + kLambda));
  constexpr static FloatType kCovWeightI = kMeanWeightI;

//1),N*1矩阵,对N个变量的估计
  GaussianDistribution<FloatType, N> belief_; 
//2),add_delta_，加法操作
  const std::function<StateType(const StateType& state, const StateType& delta)>
      add_delta_;
//3),compute_delta_，计算偏差操作
  const std::function<StateType(const StateType& origin,
                                const StateType& target)>
      compute_delta_;
};

//外部声明。
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kAlpha;
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kKappa;
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kBeta;
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kLambda;
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kMeanWeight0;
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kCovWeight0;
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kMeanWeightI;
template <typename FloatType, int N>
constexpr FloatType UnscentedKalmanFilter<FloatType, N>::kCovWeightI;

}  // namespace kalman_filter
}  // namespace cartographer

#endif  // CARTOGRAPHER_KALMAN_FILTER_UNSCENTED_KALMAN_FILTER_H_
