#ifndef PIMSIM_TYPE_SUPPORT_H
#define PIMSIM_TYPE_SUPPORT_H

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>
namespace pimsim {
class Context;

using Byte = std::uint8_t;

template <typename T> struct TypeSupport;

using u8 = uint8_t;
using i8 = int8_t;
using u16 = uint16_t;
using i16 = int16_t;
using u32 = uint32_t;
using i32 = int32_t;
using u64 = uint64_t;
using i64 = int64_t;

struct __Float16 {
  uint16_t data;

  __Float16() : data(0) {}
  __Float16(const __Float16 &other) = default;
  __Float16(uint16_t bits) : data(bits) {}
  __Float16(float value) : data(fromFloat(value).data) {}
  __Float16 &operator=(uint16_t bits) {
    data = bits;
    return *this;
  }
  __Float16 &operator=(const __Float16 &other) {
    data = other.data;
    return *this;
  }
  __Float16 &operator=(float value) {
    *this = fromFloat(value);
    return *this;
  }

  operator uint16_t() const { return data; }

  static __Float16 fromBits(uint16_t bits) {
    __Float16 f;
    f.data = bits;
    return f;
  }

  static __Float16 fromFloat(float value) {
    llvm::APFloat apValue(value);
    bool lose;
    apValue.convert(llvm::APFloat::IEEEhalf(),
                    llvm::APFloat::rmNearestTiesToEven, &lose);
    return fromBits(
        static_cast<uint16_t>(apValue.bitcastToAPInt().getZExtValue()));
  }

  __Float16 operator+(const __Float16 &other) const {
    llvm::APFloat a(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(data)));
    llvm::APFloat b(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(other.data)));
    a.add(b, llvm::APFloat::rmNearestTiesToEven);
    bool lose;
    a.convert(llvm::APFloat::IEEEhalf(), llvm::APFloat::rmNearestTiesToEven,
              &lose);
    return fromBits(static_cast<uint16_t>(a.bitcastToAPInt().getZExtValue()));
  }

  __Float16 operator*(const __Float16 &other) const {
    llvm::APFloat a(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(data)));
    llvm::APFloat b(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(other.data)));
    a.multiply(b, llvm::APFloat::rmNearestTiesToEven);
    bool lose;
    a.convert(llvm::APFloat::IEEEhalf(), llvm::APFloat::rmNearestTiesToEven,
              &lose);
    return fromBits(static_cast<uint16_t>(a.bitcastToAPInt().getZExtValue()));
  }

  __Float16 operator-(const __Float16 &other) const {
    llvm::APFloat a(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(data)));
    llvm::APFloat b(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(other.data)));
    a.subtract(b, llvm::APFloat::rmNearestTiesToEven);
    bool lose;
    a.convert(llvm::APFloat::IEEEhalf(), llvm::APFloat::rmNearestTiesToEven,
              &lose);
    return fromBits(static_cast<uint16_t>(a.bitcastToAPInt().getZExtValue()));
  }

  __Float16 operator/(const __Float16 &other) const {
    llvm::APFloat a(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(data)));
    llvm::APFloat b(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(other.data)));
    a.divide(b, llvm::APFloat::rmNearestTiesToEven);
    bool lose;
    a.convert(llvm::APFloat::IEEEhalf(), llvm::APFloat::rmNearestTiesToEven,
              &lose);
    return fromBits(static_cast<uint16_t>(a.bitcastToAPInt().getZExtValue()));
  }

  __Float16 operator-() const {
    llvm::APFloat a(llvm::APFloat::IEEEhalf(),
                    llvm::APInt(16, static_cast<uint16_t>(data)));
    a.changeSign();
    bool lose;
    a.convert(llvm::APFloat::IEEEhalf(), llvm::APFloat::rmNearestTiesToEven,
              &lose);
    return fromBits(static_cast<uint16_t>(a.bitcastToAPInt().getZExtValue()));
  }

  __Float16 operator+() const { return *this; }

  __Float16 operator+=(const __Float16 &other) { return *this = *this + other; }
  __Float16 operator-=(const __Float16 &other) { return *this = *this - other; }
  __Float16 operator*=(const __Float16 &other) { return *this = *this * other; }
  __Float16 operator/=(const __Float16 &other) { return *this = *this / other; }

  bool operator==(const __Float16 &other) const { return data == other.data; }
  bool operator!=(const __Float16 &other) const { return data != other.data; }

  bool equal(const __Float16 &other, size_t maxULP) const;
};
using f16 = __Float16;

struct __BFloat16 {
  uint16_t data;
};
using bf16 = __BFloat16;

using f32 = float;
using f64 = double;

template <typename T, typename Enable = void>
struct IsFloat : std::false_type {};

template <typename T>
struct IsFloat<T, std::enable_if_t<std::is_floating_point_v<T>>>
    : std::true_type {
  using bitRep = typename std::conditional_t<
      sizeof(T) == 4, uint32_t,
      typename std::conditional_t<sizeof(T) == 8, uint64_t, void>>;
};

template <typename T>
struct IsFloat<T, std::enable_if_t<std::is_same_v<T, f16>>> : std::true_type {
  using bitRep = uint16_t;
};

template <typename T>
struct IsFloat<T, std::enable_if_t<std::is_same_v<T, bf16>>> : std::true_type {
  using bitRep = uint16_t;
};

template <typename T> struct IntVerifier {
  static bool verify(llvm::ArrayRef<Byte> data, llvm::StringRef expected,
                     llvm::raw_ostream &err) {
    T value = 0;
    assert(data.size() == sizeof(T) && "Data size must match type size");
    memcpy(&value, data.data(), sizeof(T));

    T expectedValue = 0;
    if (expected.getAsInteger(0, expectedValue)) {
      err << "Failed to parse expected value: " << expected << "\n";
      return false; // Failed to parse expected value
    }
    return value == expectedValue;
  }
};

template <typename T> struct IntPrinter {
  static void print(llvm::raw_ostream &os, llvm::ArrayRef<Byte> data) {
    T value = 0;
    assert(data.size() == sizeof(T) && "Data size must match type size");
    memcpy(&value, data.data(), sizeof(T));
    os << value;
  }
};

template <>
struct TypeSupport<uint8_t> : IntVerifier<uint8_t>, IntPrinter<uint8_t> {
  static constexpr const char *name() { return "u8"; }
  static constexpr size_t dataWidth() { return 3; }
};

template <>
struct TypeSupport<int8_t> : IntVerifier<int8_t>, IntPrinter<int8_t> {
  static constexpr const char *name() { return "i8"; }
  static constexpr size_t dataWidth() { return 4; }
};

template <>
struct TypeSupport<uint16_t> : IntVerifier<uint16_t>, IntPrinter<uint16_t> {
  static constexpr const char *name() { return "u16"; }
  static constexpr size_t dataWidth() { return 5; }
};
template <>
struct TypeSupport<int16_t> : IntVerifier<int16_t>, IntPrinter<int16_t> {
  static constexpr const char *name() { return "i16"; }
  static constexpr size_t dataWidth() { return 6; }
};

template <>
struct TypeSupport<uint32_t> : IntVerifier<uint32_t>, IntPrinter<uint32_t> {
  static constexpr const char *name() { return "u32"; }
  static constexpr size_t dataWidth() { return 10; }
};
template <>
struct TypeSupport<int32_t> : IntVerifier<int32_t>, IntPrinter<int32_t> {
  static constexpr const char *name() { return "i32"; }
  static constexpr size_t dataWidth() { return 11; }
};

template <>
struct TypeSupport<uint64_t> : IntVerifier<uint64_t>, IntPrinter<uint64_t> {
  static constexpr const char *name() { return "u64"; }
  static constexpr size_t dataWidth() { return 20; }
};

template <>
struct TypeSupport<int64_t> : IntVerifier<int64_t>, IntPrinter<int64_t> {
  static constexpr const char *name() { return "i64"; }
  static constexpr size_t dataWidth() { return 21; }
};

template <typename T> struct FloatPrinter {
  static void print(llvm::raw_ostream &os, llvm::ArrayRef<Byte> data) {
    T value;
    memcpy(&value, data.data(), sizeof(T));
    llvm::APFloat apValue(
        *TypeSupport<T>::getSemantics(),
        llvm::APInt(
            sizeof(T) * 8,
            *reinterpret_cast<const typename IsFloat<T>::bitRep *>(&value)));
    llvm::SmallVector<char, 16> floatStr;
    apValue.toString(floatStr, TypeSupport<T>::dataWidth());
    os << llvm::StringRef(floatStr.data(), floatStr.size());
  }
};

#define MAX_ULP_DIFF 4
template <typename T> struct FloatVerifier {
  static size_t ulpDiff(llvm::APFloat a, llvm::APFloat b) {
    if (a.isNaN() || b.isNaN()) {
      return std::numeric_limits<size_t>::max(); // NaNs are infinitely
                                                 // different
    }
    if (a.isInfinity() || b.isInfinity()) {
      return (a.isInfinity() && b.isInfinity() &&
              a.isNegative() == b.isNegative())
                 ? 0
                 : std::numeric_limits<
                       size_t>::max(); // Infinities of the same sign are equal,
                                       // otherwise infinitely different
    }

    if (a.isNegative() != b.isNegative()) {
      llvm::APFloat zeroA(*TypeSupport<T>::getSemantics(),
                          llvm::APInt(sizeof(T) * 8, 0));
      return ulpDiff(a, zeroA) + ulpDiff(zeroA, b) + 1;
    }

    uint64_t aInt = a.bitcastToAPInt().getZExtValue();
    uint64_t bInt = b.bitcastToAPInt().getZExtValue();

    return aInt > bInt ? aInt - bInt : bInt - aInt;
  }

  static bool verify(llvm::ArrayRef<Byte> data, llvm::StringRef expected,
                     llvm::raw_ostream &err) {
    T value;
    memcpy(&value, data.data(), sizeof(T));
    llvm::APFloat apValue(
        *TypeSupport<T>::getSemantics(),
        llvm::APInt(sizeof(T) * 8,
                    *reinterpret_cast<typename IsFloat<T>::bitRep *>(&value)));

    llvm::APFloat expectedValue(*TypeSupport<T>::getSemantics());
    auto status = expectedValue.convertFromString(
        expected, llvm::APFloat::rmNearestTiesToEven);
    if (auto errorMsg = status.takeError()) {
      err << "Failed to parse expected value: " << expected
          << ". Error: " << llvm::toString(std::move(errorMsg)) << "\n";
      return false; // Failed to parse expected value
    }

    auto ulp = ulpDiff(apValue, expectedValue);
    return ulp < MAX_ULP_DIFF;
  }
};

inline bool __Float16::equal(const __Float16 &other,
                             size_t maxULP = MAX_ULP_DIFF) const {
  llvm::APFloat a(llvm::APFloat::IEEEhalf(),
                  llvm::APInt(16, static_cast<uint16_t>(data)));
  llvm::APFloat b(llvm::APFloat::IEEEhalf(),
                  llvm::APInt(16, static_cast<uint16_t>(other.data)));
  return FloatVerifier<f16>::ulpDiff(a, b) <= maxULP;
}

template <>
struct TypeSupport<float> : FloatPrinter<float>, FloatVerifier<float> {
  static constexpr const char *name() { return "f32"; }
  static constexpr size_t dataWidth() { return 14; }
  using bitRep = uint32_t;
  static const llvm::fltSemantics *getSemantics() {
    return &llvm::APFloat::IEEEsingle();
  }
};
template <> struct TypeSupport<f16> : FloatPrinter<f16>, FloatVerifier<f16> {
  static constexpr const char *name() { return "f16"; }
  static constexpr size_t dataWidth() { return 7; }
  using bitRep = uint16_t;
  static const llvm::fltSemantics *getSemantics() {
    return &llvm::APFloat::IEEEhalf();
  }
};
template <> struct TypeSupport<bf16> : FloatPrinter<bf16>, FloatVerifier<bf16> {
  static constexpr const char *name() { return "bf16"; }
  static constexpr size_t dataWidth() { return 7; }
  using bitRep = uint16_t;
  static const llvm::fltSemantics *getSemantics() {
    return &llvm::APFloat::BFloat();
  }
};

template <>
struct TypeSupport<double> : FloatPrinter<double>, FloatVerifier<double> {
  static constexpr const char *name() { return "f64"; }
  static constexpr size_t dataWidth() { return 25; }
  using bitRep = uint64_t;
  static const llvm::fltSemantics *getSemantics() {
    return &llvm::APFloat::IEEEdouble();
  }
};

std::tuple<
    size_t,
    llvm::function_ref<void(llvm::raw_ostream &os, llvm::ArrayRef<Byte>)>,
    llvm::function_ref<bool(llvm::ArrayRef<Byte>, llvm::StringRef,
                            llvm::raw_ostream &)>,
    bool>
parseType(llvm::StringRef typeStr, Context *ctx);

const llvm::fltSemantics &parseFloatSemantics(llvm::StringRef typeStr);

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const f16 &value) {
  TypeSupport<f16>::print(
      os, llvm::ArrayRef<Byte>(reinterpret_cast<const Byte *>(&value.data),
                               sizeof(value.data)));
  return os;
}

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const bf16 &value) {
  TypeSupport<bf16>::print(
      os, llvm::ArrayRef<Byte>(reinterpret_cast<const Byte *>(&value.data),
                               sizeof(value.data)));
  return os;
}

} // namespace pimsim

#endif // PIMSIM_TYPE_SUPPORT_H
