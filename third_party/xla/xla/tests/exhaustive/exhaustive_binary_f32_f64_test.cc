/* Copyright 2019 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <tuple>
#include <type_traits>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "xla/client/xla_builder.h"
#include "xla/literal.h"
#include "xla/tests/exhaustive/exhaustive_op_test_utils.h"
#include "xla/tests/test_macros.h"
#include "tsl/platform/test.h"

#ifdef __FAST_MATH__
#error("Can't be compiled with fast math on");
#endif

namespace xla {
namespace exhaustive_op_test {
namespace {

// Exhaustive test for binary operations for float and double.
//
// Test parameter is a tuple of (FpValues, FpValues) describing the possible
// values for each operand. The inputs for the test are the Cartesian product
// of the possible values for the two operands.
template <PrimitiveType T>
class Exhaustive32BitOrMoreBinaryTest
    : public ExhaustiveBinaryTest<T>,
      public ::testing::WithParamInterface<std::tuple<FpValues, FpValues>> {
 protected:
  using typename ExhaustiveBinaryTest<T>::NativeT;
  using ExhaustiveBinaryTest<T>::ConvertAndReplaceKnownIncorrectValueWith;

 private:
  int64_t GetInputSize() override {
    FpValues values_0;
    FpValues values_1;
    std::tie(values_0, values_1) = GetParam();
    return values_0.GetTotalNumValues() * values_1.GetTotalNumValues();
  }

  void FillInput(std::array<Literal, 2>* input_literals) override {
    int64_t input_size = GetInputSize();
    FpValues values_0;
    FpValues values_1;
    std::tie(values_0, values_1) = GetParam();
    if (VLOG_IS_ON(2)) {
      LOG(INFO) << this->SuiteName() << this->TestName() << " Values:";
      LOG(INFO) << "\tleft values=" << values_0.ToString();
      LOG(INFO) << "\tright values=" << values_1.ToString();
      LOG(INFO) << "\ttotal values to test=" << input_size;
    }
    CHECK(input_size == (*input_literals)[0].element_count() &&
          input_size == (*input_literals)[1].element_count());

    absl::Span<NativeT> input_arr_0 = (*input_literals)[0].data<NativeT>();
    absl::Span<NativeT> input_arr_1 = (*input_literals)[1].data<NativeT>();

    uint64_t i = 0;
    for (auto src0 : values_0) {
      for (auto src1 : values_1) {
        input_arr_0[i] = ConvertAndReplaceKnownIncorrectValueWith(src0, 1);
        input_arr_1[i] = ConvertAndReplaceKnownIncorrectValueWith(src1, 1);
        ++i;
      }
    }
    CHECK_EQ(i, input_size);
  }
};

using ExhaustiveF32BinaryTest = Exhaustive32BitOrMoreBinaryTest<F32>;

#define BINARY_TEST_FLOAT_32(test_name, ...)     \
  XLA_TEST_P(ExhaustiveF32BinaryTest, test_name) \
  __VA_ARGS__

BINARY_TEST_FLOAT_32(Add, {
  auto host_add = [](float x, float y) { return x + y; };
  Run(AddEmptyBroadcastDimension(Add), host_add);
})

BINARY_TEST_FLOAT_32(Sub, {
  auto host_sub = [](float x, float y) { return x - y; };
  Run(AddEmptyBroadcastDimension(Sub), host_sub);
})

// TODO(bixia): Need to investigate the failure on CPU and file bugs.
BINARY_TEST_FLOAT_32(DISABLED_ON_CPU(Mul), {
  auto host_mul = [](float x, float y) { return x * y; };
  Run(AddEmptyBroadcastDimension(Mul), host_mul);
})

// TODO(bixia): Need to investigate the failure on CPU and file bugs.
BINARY_TEST_FLOAT_32(DISABLED_ON_CPU(Div), {
  auto host_div = [](float x, float y) { return x / y; };
  Run(AddEmptyBroadcastDimension(Div), host_div);
})

BINARY_TEST_FLOAT_32(Max, {
  Run(AddEmptyBroadcastDimension(Max), ReferenceMax<float>);
})

BINARY_TEST_FLOAT_32(Min, {
  Run(AddEmptyBroadcastDimension(Min), ReferenceMin<float>);
})

INSTANTIATE_TEST_SUITE_P(
    SpecialValues, ExhaustiveF32BinaryTest,
    ::testing::Combine(
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<float>()),
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<float>())));

INSTANTIATE_TEST_SUITE_P(
    SpecialAndNormalValues, ExhaustiveF32BinaryTest,
    ::testing::Combine(
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<float>()),
        ::testing::Values(GetNormals<float>(2000))));

INSTANTIATE_TEST_SUITE_P(
    NormalAndSpecialValues, ExhaustiveF32BinaryTest,
    ::testing::Combine(
        ::testing::Values(GetNormals<float>(2000)),
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<float>())));

INSTANTIATE_TEST_SUITE_P(
    NormalAndNormalValues, ExhaustiveF32BinaryTest,
    ::testing::Combine(::testing::Values(GetNormals<float>(2000)),
                       ::testing::Values(GetNormals<float>(2000))));

// Tests a total of 40000 ^ 2 inputs, with 2000 ^ 2 inputs in each sub-test.
// Comparing with the unary tests, the binary tests use a smaller set of inputs
// for each sub-test to avoid timeout because the implementation of ExpectNear
// more than 2x slower for binary test.
INSTANTIATE_TEST_SUITE_P(
    LargeAndSmallMagnitudeNormalValues, ExhaustiveF32BinaryTest,
    ::testing::Combine(
        ::testing::ValuesIn(GetFpValuesForMagnitudeExtremeNormals<float>(40000,
                                                                         2000)),
        ::testing::ValuesIn(
            GetFpValuesForMagnitudeExtremeNormals<float>(40000, 2000))));

#if !defined(XLA_BACKEND_DOES_NOT_SUPPORT_FLOAT64)
using ExhaustiveF64BinaryTest = Exhaustive32BitOrMoreBinaryTest<F64>;
#define BINARY_TEST_FLOAT_64(test_name, ...)     \
  XLA_TEST_P(ExhaustiveF64BinaryTest, test_name) \
  __VA_ARGS__
#else
#define BINARY_TEST_FLOAT_64(test_name, ...)
#endif

BINARY_TEST_FLOAT_64(Add, {
  auto host_add = [](double x, double y) { return x + y; };
  Run(AddEmptyBroadcastDimension(Add), host_add);
})

BINARY_TEST_FLOAT_64(Sub, {
  auto host_sub = [](double x, double y) { return x - y; };
  Run(AddEmptyBroadcastDimension(Sub), host_sub);
})

// TODO(bixia): Need to investigate the failure on CPU and file bugs.
BINARY_TEST_FLOAT_64(DISABLED_ON_CPU(Mul), {
  auto host_mul = [](double x, double y) { return x * y; };
  Run(AddEmptyBroadcastDimension(Mul), host_mul);
})

// TODO(bixia): Need to investigate the failure on CPU and file bugs.
BINARY_TEST_FLOAT_64(DISABLED_ON_CPU(Div), {
  auto host_div = [](double x, double y) { return x / y; };
  Run(AddEmptyBroadcastDimension(Div), host_div);
})

BINARY_TEST_FLOAT_64(Max, {
  Run(AddEmptyBroadcastDimension(Max), ReferenceMax<double>);
})

BINARY_TEST_FLOAT_64(Min, {
  Run(AddEmptyBroadcastDimension(Min), ReferenceMin<double>);
})

#if !defined(XLA_BACKEND_DOES_NOT_SUPPORT_FLOAT64)
INSTANTIATE_TEST_SUITE_P(
    SpecialValues, ExhaustiveF64BinaryTest,
    ::testing::Combine(
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<double>()),
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<double>())));

INSTANTIATE_TEST_SUITE_P(
    SpecialAndNormalValues, ExhaustiveF64BinaryTest,
    ::testing::Combine(
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<double>()),
        ::testing::Values(GetNormals<double>(1000))));

INSTANTIATE_TEST_SUITE_P(
    NormalAndSpecialValues, ExhaustiveF64BinaryTest,
    ::testing::Combine(
        ::testing::Values(GetNormals<double>(1000)),
        ::testing::ValuesIn(CreateFpValuesForBoundaryTest<double>())));

INSTANTIATE_TEST_SUITE_P(
    NormalAndNormalValues, ExhaustiveF64BinaryTest,
    ::testing::Combine(::testing::Values(GetNormals<double>(1000)),
                       ::testing::Values(GetNormals<double>(1000))));

// Tests a total of 40000 ^ 2 inputs, with 1000 ^ 2 inputs in each sub-test.
// Similar to ExhaustiveF64BinaryTest, we use a smaller set of inputs for each
// for each sub-test comparing with the unary test to avoid timeout.
INSTANTIATE_TEST_SUITE_P(
    LargeAndSmallMagnitudeNormalValues, ExhaustiveF64BinaryTest,
    ::testing::Combine(
        ::testing::ValuesIn(
            GetFpValuesForMagnitudeExtremeNormals<double>(40000, 2000)),
        ::testing::ValuesIn(
            GetFpValuesForMagnitudeExtremeNormals<double>(40000, 2000))));
#endif  // !defined(XLA_BACKEND_DOES_NOT_SUPPORT_FLOAT64)

#define BINARY_TEST_FLOAT_BOTH(test_name, ...) \
  BINARY_TEST_FLOAT_32(test_name, __VA_ARGS__) \
  BINARY_TEST_FLOAT_64(test_name, __VA_ARGS__)

// Can be thought of as an absolute error of
// `<= |std::numeric_limits::<float>::min()|`.
template <typename NativeRefT>
double AbsComplexCpuAbsErr(NativeRefT real, NativeRefT imag) {
  // absolute value (distance) short circuits if the first component is
  // subnormal.
  if (!std::isnan(real) && IsSubnormal(real)) {
    return std::abs(real);
  }
  return 0.0;
}

template <typename NativeRefT>
bool AbsComplexSkip(NativeRefT real, NativeRefT imag) {
  // TODO(timshen): see b/162664705.
  if (std::isnan(real) || std::isnan(imag)) {
    return true;
  }
  return false;
}

// It is more convenient to implement Abs(complex) as a binary op than a unary
// op, as the operations we currently support all have the same data type for
// the source operands and the results.
// TODO(bixia): May want to move this test to unary test if we will be able to
// implement Abs(complex) as unary conveniently.
BINARY_TEST_FLOAT_BOTH(AbsComplex, {
  ErrorSpecGen error_spec_gen = +[](NativeRefT, NativeRefT) {
    return ErrorSpec::Builder().strict_signed_zeros().build();
  };

  if (IsCpu(platform_)) {
    if constexpr (std::is_same_v<NativeT, float> ||
                  std::is_same_v<NativeT, double>) {
      error_spec_gen = +[](NativeRefT real, NativeRefT imag) {
        return ErrorSpec::Builder()
            .abs_err(AbsComplexCpuAbsErr(real, imag))
            .distance_err(2)
            .skip_comparison(AbsComplexSkip(real, imag))
            .build();
      };
    }
  }

  if (IsGpu(platform_)) {
    if constexpr (std::is_same_v<NativeT, float>) {
      error_spec_gen = +[](NativeRefT real, NativeRefT imag) {
        return ErrorSpec::Builder()
            .distance_err(3)
            .skip_comparison(AbsComplexSkip(real, imag))
            .build();
      };
    } else if constexpr (std::is_same_v<NativeT, double>) {
      error_spec_gen = +[](NativeRefT real, NativeRefT imag) {
        return ErrorSpec::Builder()
            .distance_err(2)
            .skip_comparison(AbsComplexSkip(real, imag))
            .build();
      };
    }
  }

  EnableDebugLoggingForScope([this, error_spec_gen]() {
    Run([](XlaOp x, XlaOp y) { return Abs(Complex(x, y)); },
        [](NativeRefT x, NativeRefT y) {
          return std::abs(std::complex<NativeRefT>(x, y));
        },
        error_spec_gen);
  });
})

}  // namespace
}  // namespace exhaustive_op_test
}  // namespace xla
