#include "pimsim/TypeSupport.h"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "API.h"
#include "doctest/doctest.h"

// [M, N] x [N, 1]
// y = A x
//
// small gemv: M = 16, N < 1024
void smallGemv(pimsim_memory_device_t *dev, pimsim_row_group_t *rg, size_t M,
               size_t N, pimsim::f16 *result) {
  for (size_t col = 0; col < N * 2;
       col += PIMSIM_DEFAULT_PIM_COMP_COL_SIZE * 2) {
    pimsim_issue_compute(dev, rg, col * sizeof(pimsim::f16));
  }
  pimsim_issue_read_result(dev, rg->channel_addr, result);
}

std::vector<std::vector<pimsim::f16>>
convertToF16Matrix(const std::vector<std::vector<float>> &matrix) {
  std::vector<std::vector<pimsim::f16>> f16Matrix(
      matrix.size(), std::vector<pimsim::f16>(matrix[0].size()));
  for (size_t i = 0; i < matrix.size(); ++i) {
    for (size_t j = 0; j < matrix[i].size(); ++j) {
      f16Matrix[i][j] = pimsim::f16::fromFloat(matrix[i][j]);
    }
  }
  return f16Matrix;
}

std::vector<pimsim::f16> convertToF16Vector(const std::vector<float> &vec) {
  std::vector<pimsim::f16> f16Vec(vec.size());
  for (size_t i = 0; i < vec.size(); ++i) {
    f16Vec[i] = pimsim::f16::fromFloat(vec[i]);
  }
  return f16Vec;
}

static std::vector<std::vector<pimsim::f16>> test16x16{
    {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f,
     13.0f, 14.0f, 15.0f, 16.0f},
    {16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f,
     5.0f, 4.0f, 3.0f, 2.0f, 1.0f},
    {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f, 9.5f, 10.5f, 11.5f, 12.5f,
     13.5f, 14.5f, 15.5f, 16.5f},
    {16.5f, 15.5f, 14.5f, 13.5f, 12.5f, 11.5f, 10.5f, 9.5f, 8.5f, 7.5f, 6.5f,
     5.5f, 4.5f, 3.5f, 2.5f, 1.5f},
    {2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f,
     14.0f, 15.0f, 16.0f},
    {16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f,
     5.0f, 4.0f, 3.0f, 2.0f, 1.0f},
    {2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f, 9.5f, 10.5f, 11.5f, 12.5f, 13.5f,
     14.5f, 15.5f, 16.5f, 16.5f},
    {16.5f, 15.5f, 14.5f, 13.5f, 12.5f, 11.5f, 10.5f, 9.5f, 8.5f, 7.5f, 6.5f,
     5.5f, 4.5f, 3.5f, 2.5f, 1.5f},
    {3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f,
     14.0f, 15.0f, 16.0f, 17.0f, 18.0f},
    {18.0f, 17.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f,
     7.0f, 6.0f, 5.0f, 4.0f, 3.0f},
    {3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f, 9.5f, 10.5f, 11.5f, 12.5f, 13.5f,
     14.5f, 15.5f, 16.5f, 17.5f, 18.5f},
    {18.5f, 17.5f, 16.5f, 15.5f, 14.5f, 13.5f, 12.5f, 11.5f, 10.5f, 9.5f, 8.5f,
     7.5f, 6.5f, 5.5f, 4.5f, 3.5f},
    {4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f,
     15.0f, 16.0f, 17.0f, 18.0f, 19.0f},
    {19.0f, 18.0f, 17.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f,
     8.0f, 7.0f, 6.0f, 5.0f, 4.0f},
    {4.5f, 5.5f, 6.5f, 7.5f, 8.5f, 9.5f, 10.5f, 11.5f, 12.5f, 13.5f, 14.5f,
     15.5f, 16.5f, 17.5f, 18.5f, 19.5f},
    {19.5f, 18.5f, 17.5f, 16.5f, 15.5f, 14.5f, 13.5f, 12.5f, 11.5f, 10.5f, 9.5f,
     8.5f, 7.5f, 6.5f, 5.5f, 4.5f},
};

static std::vector<pimsim::f16> testVec16{
    1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
    9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};

static std::vector<pimsim::f16> testResult16;

static void computeTestResult16() {
  testResult16.resize(16);
  for (size_t i = 0; i < 16; ++i) {
    pimsim::f16 sum = 0.0f;
    for (size_t j = 0; j < 16; ++j) {
      auto a = test16x16[i][j];
      auto b = testVec16[j];
      auto product = a * b;
      sum += product;
    }
    testResult16[i] = sum;
  }
}

static bool verifyResult(const std::vector<pimsim::f16> &result,
                         const std::vector<pimsim::f16> &expected) {
  if (result.size() != expected.size()) {
    llvm::errs() << "Result size " << result.size()
                 << " does not match expected size " << expected.size() << '\n';
    return false;
  }
  bool failed = false;
  for (size_t i = 0; i < result.size(); ++i) {
    if (!result[i].equal(expected[i], 6)) { // Allow 6 ULP difference
      llvm::errs() << "result[" << i << "]: " << result[i] << " expected[" << i
                   << "]: " << expected[i] << '\n';
      failed = true;
    }
  }
  return !failed;
}

TEST_CASE("Small Gemv") {
  computeTestResult16();
  pimsim_memory_device_t **device_ptr =
      (pimsim_memory_device_t **)malloc(sizeof(pimsim_memory_device_t *));
  auto status = pimsim_create_memory_device("type=newton config=HBM2_8Gb_x128",
                                            device_ptr, true);
  assert(pimsim_is_success(status) && "Failed to create memory device");

  pimsim_config_t config;
  status = pimsim_get_config(*device_ptr, 0, &config);
  assert(pimsim_is_success(status) && "Failed to get memory device config");
  assert(config.banks * config.bankgroups * config.ranks == 16 &&
         "Config must have 16 rows for this test");
  testVec16.resize(
      config.columns /
      sizeof(pimsim::f16)); // Ensure test vector size matches config

  pimsim_memory_device_t *device = *device_ptr;

  SUBCASE("Gemv with 16x16 matrix") {
    size_t M = 16;
    size_t N = 16;

    pimsim_row_group_t rowGroup;
    auto status = pimsim_allocate_row_groups(device, 1, &rowGroup,
                                             PIMSIM_PREFER_ON_ONE_MODULE,
                                             PIMSIM_PREFER_ON_ONE_CHANNEL);
    assert(pimsim_is_success(status) && "Failed to allocate row group");

    // initialize row group with matrix data
    for (size_t row = 0; row < M; ++row) {
      pimsim_issue_write(device, rowGroup.row_addrs[row], test16x16[row].data(),
                         N * sizeof(pimsim::f16));
    }

    pimsim_issue_gwrite(device, rowGroup.channel_addr, testVec16.data());

    std::vector<pimsim::f16> result(N);
    smallGemv(device, &rowGroup, M, N, result.data());

    REQUIRE(verifyResult(result, testResult16));
    pimsim_free_row_groups(device, 1, &rowGroup);
  }

  pimsim_destroy_memory_device(*device_ptr);
  free(device_ptr);
}

pimsim::f16 getRandomValue() {
  return static_cast<float>(rand() % 100) / 100 * 10.0f;
}

std::vector<std::vector<pimsim::f16>> createRandomF16Matrix(size_t N,
                                                            size_t M) {
  std::vector<std::vector<pimsim::f16>> matrix(N, std::vector<pimsim::f16>(M));

  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < M; ++j) {
      float randomValue = getRandomValue();
      matrix[i][j] = pimsim::f16::fromFloat(randomValue);
    }
  }

  return matrix;
}

std::vector<pimsim::f16> createRandomF16Vector(size_t N) {
  std::vector<pimsim::f16> vector(N);

  for (size_t i = 0; i < N; ++i) {
    float randomValue = getRandomValue();
    vector[i] = pimsim::f16::fromFloat(randomValue);
  }

  return vector;
}

std::vector<pimsim::f16>
computeGemv(const std::vector<std::vector<pimsim::f16>> &A,
            const std::vector<pimsim::f16> &x) {
  size_t N = A.size();
  size_t M = A[0].size();
  std::vector<pimsim::f16> y(N, pimsim::f16(0.0f));

  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < M; ++j) {
      y[i] += A[i][j] * x[j];
    }
  }

  return y;
}

TEST_CASE("Big Gemv") {
  pimsim_memory_device_t **device_ptr =
      (pimsim_memory_device_t **)malloc(sizeof(pimsim_memory_device_t *));

  auto status = pimsim_create_memory_device("type=newton config=HBM2_8Gb_x128",
                                            device_ptr, true);
  assert(pimsim_is_success(status));

  pimsim_memory_device_t *device = *device_ptr;

  pimsim_config_t config;
  status = pimsim_get_config(device, 0, &config);
  assert(pimsim_is_success(status));

  assert(config.columns == 128);
  assert(config.banks * config.bankgroups * config.ranks == 16);

  SUBCASE("GEMV 16 x 64 Matrix") {
    size_t N = 16, M = 64;
    auto testMatrix = createRandomF16Matrix(N, M);
    auto testVector = createRandomF16Vector(M);
    auto expectedResult = computeGemv(testMatrix, testVector);

    // one full row group
    pimsim_row_group_t rg;
    status =
        pimsim_allocate_row_groups(device, 1, &rg, PIMSIM_PREFER_ON_ONE_MODULE,
                                   PIMSIM_PREFER_ON_ONE_CHANNEL);
    assert(pimsim_is_success(status));

    // initialize
    for (size_t row = 0; row < N; ++row) {
      size_t rowAddress = rg.row_addrs[row];

      pimsim_issue_write(device, rowAddress, testMatrix[row].data(),
                         M * sizeof(pimsim::f16));
    }

    pimsim_issue_gwrite(device, rg.channel_addr, testVector.data());

    for (size_t col = 0; col < M * 2;
         col += PIMSIM_DEFAULT_PIM_COMP_COL_SIZE * 2) {
      pimsim_issue_compute(device, &rg, col);
    }

    std::vector<pimsim::f16> result(N);
    pimsim_issue_read_result(device, rg.channel_addr, result.data());

    REQUIRE(verifyResult(result, expectedResult));

    pimsim_free_row_groups(device, 1, &rg);
  }

  SUBCASE("GEMV 64 x 64 Matrix") {
    size_t N = 64, M = 64;
    auto testMatrix = createRandomF16Matrix(N, M);
    auto testVector = createRandomF16Vector(M);
    auto expectedResult = computeGemv(testMatrix, testVector);

    // four full row group
    std::vector<pimsim_row_group_t> rgs(4);
    status = pimsim_allocate_row_groups(device, 4, rgs.data(),
                                        PIMSIM_PREFER_ON_ONE_MODULE,
                                        PIMSIM_PREFER_BALANCED_CHANNELS);
    assert(pimsim_is_success(status));

    // initialize
    size_t tileN = rgs[0].row_cnt;

    for (size_t nTI = 0; nTI < N; nTI += tileN) {
      auto rgIndex = nTI / tileN;
      const auto &rg = rgs[rgIndex];

      pimsim_issue_gwrite(device, rg.channel_addr, testVector.data());
      for (size_t row = 0; row < tileN; ++row) {
        size_t rowAddr = rg.row_addrs[row];
        status =
            pimsim_issue_write(device, rowAddr, testMatrix[nTI + row].data(),
                               M * sizeof(pimsim::f16));
        assert(pimsim_is_success(status));
      }
    }

    // comp
    std::vector<pimsim::f16> result(N);
    for (size_t rgIndex = 0; rgIndex < N / tileN; ++rgIndex) {
      const auto &rg = rgs[rgIndex];

      size_t colOffset = rgIndex * tileN;
      for (size_t col = 0; col < M * 2;
           col += PIMSIM_DEFAULT_PIM_COMP_COL_SIZE * 2) {
        status = pimsim_issue_compute(device, &rg, col);
        assert(pimsim_is_success(status));
      }

      status = pimsim_issue_read_result(device, rg.channel_addr,
                                        result.data() + colOffset);
    }

    REQUIRE(verifyResult(result, expectedResult));

    pimsim_free_row_groups(device, 4, rgs.data());
  }

  pimsim_destroy_memory_device(*device_ptr);
  free(device_ptr);
}
