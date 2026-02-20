#ifndef PIMSIM_SERDE_H
#define PIMSIM_SERDE_H

#include "llvm/ADT/ArrayRef.h"
#include <type_traits>

namespace pimsim {
using Byte = std::uint8_t;

template <typename T, typename Enable = void> struct Serde;

template <typename T>
struct Serde<
    T, std::enable_if_t<std::conjunction_v<
           std::is_trivially_copyable<T>, std::negation<std::is_pointer<T>>>>> {
  [[nodiscard]] static llvm::ArrayRef<Byte>
  deserializeImpl(T &obj, llvm::ArrayRef<Byte> data) {
    assert(data.size() >= sizeof(T) && "Insufficient data for deserialization");
    llvm::ArrayRef<Byte> objData = data.slice(0, sizeof(T));
    memcpy(&obj, objData.data(), sizeof(T));
    return data.drop_front(sizeof(T));
  }

  static void serializeImpl(const T &obj, llvm::SmallVectorImpl<Byte> &out) {
    const Byte *objBytes = reinterpret_cast<const Byte *>(&obj);
    out.append(objBytes, objBytes + sizeof(T));
  }
};

template <typename T> struct Serde<T *> {
  [[nodiscard]] static llvm::ArrayRef<Byte>
  deserializeImpl(T *&obj, llvm::ArrayRef<Byte> data) {
    size_t length;
    data = Serde<size_t>::deserializeImpl(length, data);
    assert(data.size() >= length * sizeof(T) &&
           "Insufficient data for deserialization");
    if (obj == nullptr)
      obj = new T[length];
    for (size_t i = 0; i < length; ++i) {
      data = Serde<T>::deserializeImpl(obj[i], data);
    }
    return data;
  }

  static void serializeImpl(const T *obj, llvm::SmallVectorImpl<Byte> &out,
                            size_t length) {
    Serde<size_t>::serializeImpl(length, out);
    out.reserve(out.size() + length * sizeof(T));
    for (size_t i = 0; i < length; ++i) {
      Serde<T>::serializeImpl(obj[i], out);
    }
  }
};

template <typename T> struct VectorSerde {
  template <template <typename> class VecType>
  static llvm::ArrayRef<Byte> deserializeImpl(VecType<T> &vec,
                                              llvm::ArrayRef<Byte> data) {
    // parse size

    size_t vecSize;
    data = Serde<size_t>::deserializeImpl(vecSize, data);
    if (!vec.empty()) {
      assert(vec.size() == vecSize &&
             "Vector must be empty or match the deserialized size");
      for (size_t i = 0; i < vecSize; ++i) {
        data = Serde<T>::deserializeImpl(vec[i], data);
      }
      return data;
    }

    vec.reserve(vecSize);
    for (size_t i = 0; i < vecSize; ++i) {
      data = Serde<T>::deserializeImpl(vec.emplace_back(), data);
    }
    return data;
  }

  static void serializeImpl(llvm::ArrayRef<T> vec,
                            llvm::SmallVectorImpl<Byte> &out) {
    // serializeImpl size
    Serde<size_t>::serializeImpl(vec.size(), out);
    out.reserve(out.size() + vec.size() * sizeof(T));
    for (const T &elem : vec) {
      Serde<T>::serializeImpl(elem, out);
    }
  }
};

template <typename T>
struct Serde<llvm::SmallVectorImpl<T>> : public VectorSerde<T> {};

template <typename T, unsigned N>
struct Serde<llvm::SmallVector<T, N>> : public VectorSerde<T> {
  static llvm::ArrayRef<Byte> deserializeImpl(llvm::SmallVector<T, N> &vec,
                                              llvm::ArrayRef<Byte> data) {
    return VectorSerde<T>::template deserializeImpl<llvm::SmallVectorImpl>(
        vec, data);
  }

  static void serializeImpl(const llvm::SmallVector<T, N> &vec,
                            llvm::SmallVectorImpl<Byte> &out) {
    VectorSerde<llvm::SmallVectorImpl<T>>::serializeImpl(vec, out);
  }
};

template <typename T> struct Serde<std::vector<T>> : public VectorSerde<T> {};

template <typename T>
llvm::ArrayRef<Byte> deserialize(T &obj, llvm::ArrayRef<Byte> data) {
  return Serde<T>::deserializeImpl(obj, data);
}

template <typename T, typename... Args>
void serialize(const T &obj, llvm::SmallVectorImpl<Byte> &out, Args &&...args) {
  Serde<T>::serializeImpl(obj, out, std::forward<Args>(args)...);
}

} // namespace pimsim

#endif // PIMSIM_SERDE_H
