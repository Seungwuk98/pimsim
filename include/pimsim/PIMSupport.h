#ifndef PIMSIM_PIM_SUPPORT_H
#define PIMSIM_PIM_SUPPORT_H

#include "pimsim/Memory.h"

namespace pimsim {

template <typename ConcreteChannel, typename ParentChannel,
          template <typename> typename... Traits>
class PIMChannel : public ParentChannel, public Traits<ConcreteChannel>... {
public:
  using ParentChannel::ParentChannel;

  virtual void comp() = 0;
  virtual llvm::SmallVector<f16> readResult() const = 0;

  void readRes(const dramsim3::Address &addr) {
    llvm::SmallVector<Byte> tempBuffer;
    llvm::SmallVector<f16> result = readResult();
    size_t dataSize = result.size() * sizeof(f16);
    tempBuffer.resize(dataSize);
    std::memcpy(tempBuffer.data(), result.data(), dataSize);

    auto written = this->getParentMemory()->write(
        tempBuffer, addr.channel, addr.rank, addr.bankgroup, addr.bank,
        addr.row, addr.column);
    assert(written == dataSize &&
           "Did not write all result data back to memory");
  }
};

template <typename ConcretePIM, template <typename> typename Trait>
struct TraitBase {
protected:
  ConcretePIM *derived() { return static_cast<ConcretePIM *>(this); }
  const ConcretePIM *derived() const {
    return static_cast<const ConcretePIM *>(this);
  }
};

template <typename ConcretePIM>
class BufferedChannelTrait
    : public TraitBase<ConcretePIM, BufferedChannelTrait> {
public:
  virtual void gwrite(const dramsim3::Address &addr) {
    if (globalBuffer.buffer.size() == 0) {
      initializeGlobalBuffer();
    }

    size_t dataSize = this->derived()
                          ->getParentMemory()
                          ->getConfig()
                          .columns; // assuming full row activation
    auto readData = this->derived()->getParentMemory()->read(
        globalBuffer.buffer, addr.channel, addr.rank, addr.bankgroup, addr.bank,
        addr.row, addr.column);
    assert(readData == dataSize && "Did not read all data for global buffer");
  }

  struct GlobalBuffer {
    llvm::SmallVector<Byte> buffer;
  };

  llvm::ArrayRef<Byte> getGlobalBuffer() const { return globalBuffer.buffer; }

protected:
  void initializeGlobalBuffer() {
    auto columnSize = this->derived()->getParentMemory()->getConfig().columns;
    globalBuffer.buffer.resize(columnSize, 0);
  }

  GlobalBuffer globalBuffer;
};

template <typename ConcretePIM> class HeaderedChannelTrait {
public:
  virtual void pimHeader(size_t bits) { pimHeaderBits = bits; }

  size_t getPimHeaderBits() const { return pimHeaderBits; }

protected:
  unsigned headerLength() const { return std::popcount(pimHeaderBits); }

  size_t pimHeaderBits = 0;
};

} // namespace pimsim

#endif // PIMSIM_PIM_SUPPORT_H
