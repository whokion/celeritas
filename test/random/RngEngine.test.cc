//----------------------------------*-C++-*----------------------------------//
// Copyright 2021 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file RngEngine.test.cc
//---------------------------------------------------------------------------//
#include "random/RngParams.hh"
#include "DiagnosticRngEngine.hh"
#include "SequenceEngine.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include "celeritas_test.hh"
#include "RngEngine.test.hh"
#include "base/CollectionStateStore.hh"
#include "random/distributions/GenerateCanonical.hh"

using celeritas::CollectionStateStore;
using celeritas::generate_canonical;
using celeritas::RngParams;
using celeritas::RngStateData;
using namespace celeritas_test;

//---------------------------------------------------------------------------//
// SEQUENCE ENGINE
//---------------------------------------------------------------------------//

class SequenceEngineTest : public celeritas::Test
{
  public:
    void SetUp() override
    {
        const double inv_32 = std::ldexp(1.0, -32.0);
        const double inv_64 = std::ldexp(1.0, -64.0);
        /*!
         * Note: even the lowest *normalized* float value (1e-38) is below
         * 2**-64, so the "min" values for both double and float are
         * rounded up to 2**-64, which results in a value of 5e-20 for
         * double.
         *
         * Also note that the follwing two values are ommited because they're
         * accurate to only 1e-10 for doubles -- so our "promise" of
         * reproducibility isn't quite right.
         * \code
         * std::nextafter(float(inv_64), 1.0f),
         * std::nextafter(inv_32, 0.0),
         * \endcode
         */
        values_ = {
            0.0,
            inv_64,
            std::nextafter(inv_64, 1.0),
            12345678u * inv_64,
            0xffff0000u * inv_64,
            // Note: the following two values are
            std::nextafter(float(inv_32), 0.0f),
            inv_32,
            std::nextafter(inv_32, 1.0),
            std::nextafter(float(inv_32), 1.0f),
            std::nextafter(0.5f, 0.0f), // .5f - epsf
            0x7fffffffu * inv_32,       // not exactly representable by float
            std::nextafter(0.5, 0.0),   // .5 - eps
            0.5,
            std::nextafter(0.5, 1.0),   // .5 + eps
            std::nextafter(0.5f, 1.0f), // .5f + epsf
            std::nextafter(1.0f, 0.0f), // 1 - float eps
            std::nextafter(1.0, 0.0),   // 1 - double eps
        };
    }
    std::vector<double> values_;
};

TEST_F(SequenceEngineTest, manual)
{
    SequenceEngine::VecResult raw_vals{0x00000000u, 0x20210408u, 0xffffffffu};
    SequenceEngine            engine(raw_vals);
    EXPECT_EQ(0, engine.count());
    EXPECT_EQ(3, engine.max_count());

    // Call the engine operator for actual values
    SequenceEngine::VecResult actual(raw_vals.size());
    std::generate(actual.begin(), actual.end(), std::ref(engine));
    EXPECT_EQ(3, engine.count());
    EXPECT_VEC_EQ(raw_vals, actual);

    // Past the end
    EXPECT_THROW(engine(), celeritas::DebugError);
}

TEST_F(SequenceEngineTest, from_reals)
{
    auto engine = SequenceEngine::from_reals(celeritas::make_span(values_));
    EXPECT_EQ(values_.size() * 2, engine.max_count());
    SequenceEngine::VecResult actual(engine.max_count());
    std::generate(actual.begin(), actual.end(), std::ref(engine));

    const unsigned int expected[]
        = {0u,          0u,          1u,          0u,          1u,
           0u,          12345678u,   0u,          4294901760u, 0u,
           4294967040u, 0u,          0u,          1u,          0u,
           1u,          512u,        1u,          0u,          2147483520u,
           0u,          2147483647u, 4294966272u, 2147483647u, 0u,
           2147483648u, 2048u,       2147483648u, 0u,          2147483904u,
           0u,          4294967040u, 4294965248u, 4294967295u};
    EXPECT_VEC_EQ(expected, actual);

    // Past the end
    EXPECT_THROW(engine(), celeritas::DebugError);
}

TEST_F(SequenceEngineTest, double_canonical)
{
    auto engine = SequenceEngine::from_reals(celeritas::make_span(values_));
    for (double expected : values_)
    {
        double actual = generate_canonical<double>(engine);
        // Test within 2 ulp
        EXPECT_DOUBLE_EQ(expected, actual)
            << "for i=" << (engine.count() / 2 - 1);
        EXPECT_LT(actual, 1.0);
    }
}

TEST_F(SequenceEngineTest, float_canonical)
{
    auto engine = SequenceEngine::from_reals(celeritas::make_span(values_));
    for (double expected : values_)
    {
        float actual = generate_canonical<float>(engine);
        // Test within 2 ulp
        EXPECT_FLOAT_EQ(static_cast<float>(expected), actual);
        EXPECT_LT(actual, 1.0f);
    }
}

//---------------------------------------------------------------------------//
// DIAGNOSTIC ENGINE
//---------------------------------------------------------------------------//

TEST(DiagnosticEngineTest, from_reals)
{
    auto rng = DiagnosticRngEngine<std::mt19937>();
    EXPECT_EQ(0, rng.count());
    EXPECT_EQ(3499211612u, rng());
    EXPECT_EQ(1, rng.count());
    generate_canonical<double>(rng);
    EXPECT_EQ(3, rng.count());
    generate_canonical<float>(rng);
    EXPECT_EQ(4, rng.count());
    rng.reset_count();
    EXPECT_EQ(0, rng.count());
}

//---------------------------------------------------------------------------//
// CUDA RNG
//---------------------------------------------------------------------------//

class CudaRngEngineTest : public celeritas::Test
{
  public:
    using RngDeviceStore = CollectionStateStore<RngStateData, MemSpace::device>;

    void SetUp() override { params = std::make_shared<RngParams>(12345); }

    std::shared_ptr<RngParams> params;
};

TEST_F(CudaRngEngineTest, TEST_IF_CELERITAS_CUDA(device))
{
    // Create and initialize states
    RngDeviceStore rng_store(*params, 1024);

    // Generate on device
    std::vector<unsigned int> values = re_test_native(rng_store.ref());

    // Print a subset of the values
    std::vector<unsigned int> test_values;
    for (auto i : celeritas::range(rng_store.size()).step(127u))
    {
        test_values.push_back(values[i]);
    }

    // PRINT_EXPECTED(test_values);
    static const unsigned int expected_test_values[] = {165860337u,
                                                        3006138920u,
                                                        2161337536u,
                                                        390101068u,
                                                        2347834113u,
                                                        100129048u,
                                                        4122784086u,
                                                        473544901u,
                                                        2822849608u};
    EXPECT_VEC_EQ(test_values, expected_test_values);
}

//---------------------------------------------------------------------------//
// FLOAT TEST
//---------------------------------------------------------------------------//

template<typename T>
class CudaRngEngineFloatTest : public CudaRngEngineTest
{
};

void check_expected_float_samples(const std::vector<float>& v)
{
    ASSERT_LE(2, v.size());
    EXPECT_FLOAT_EQ(0.038617369, v[0]);
    EXPECT_FLOAT_EQ(0.411269426, v[1]);
}

void check_expected_float_samples(const std::vector<double>& v)
{
    ASSERT_LE(2, v.size());
    EXPECT_DOUBLE_EQ(0.283318433931184, v[0]);
    EXPECT_DOUBLE_EQ(0.653335242131673, v[1]);
}

using FloatTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(CudaRngEngineFloatTest, FloatTypes, );

#if CELERITAS_USE_CUDA
TYPED_TEST(CudaRngEngineFloatTest, device)
#else
TYPED_TEST(CudaRngEngineFloatTest, DISABLED_device)
#endif
{
    using RngDeviceStore = typename TestFixture::RngDeviceStore;
    using real_type      = TypeParam;

    // Create and initialize states
    RngDeviceStore rng_store(*this->params, 100);

    // Generate on device
    auto values = re_test_canonical<real_type>(rng_store.ref());

    // Test result
    EXPECT_EQ(rng_store.size(), values.size());
    for (real_type sample : values)
    {
        EXPECT_GE(sample, real_type(0));
        EXPECT_LT(sample, real_type(1));
    }

    check_expected_float_samples(values);
}
