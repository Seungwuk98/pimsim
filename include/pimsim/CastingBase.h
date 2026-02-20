#ifndef PIMSIM_CASTING_BASE_H
#define PIMSIM_CASTING_BASE_H

#include "llvm/Support/raw_ostream.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace pimsim {

template <typename T> size_t typeIDGenerator() {
  static_assert(!std::is_pointer<T>::value, "T must not be a pointer type");
  static_assert(!std::is_reference<T>::value, "T must not be a reference type");
  static_assert(!std::is_const<T>::value, "T must not be a const type");
  static_assert(!std::is_volatile<T>::value, "T must not be a volatile type");
  static_assert(std::is_class<T>::value, "T must be a class type");
  static uint8_t typeID;
  return reinterpret_cast<size_t>(&typeID);
}

template <typename T> struct TypeIDGetter {
  static size_t get() { return typeIDGenerator<T>(); }
};

template <typename T> size_t TypeID = typeIDGenerator<T>();

template <typename ConcreteType> class CastingBase {
protected:
  CastingBase(size_t typeID) : typeID(typeID) {}

public:
  size_t getTypeID() const { return typeID; }

private:
  size_t typeID;
};

template <typename Derived, typename Base> struct ClassOf {
  static bool classof(const Base *base) {
    return base->getTypeID() == TypeID<Derived>;
  }
};

} // namespace pimsim

#endif // PIMSIM_CASTING_BASE_H
