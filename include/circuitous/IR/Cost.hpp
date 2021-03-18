/*
 * Copyright (c) 2021 Trail of Bits, Inc.
 */

#pragma once

#include <circuitous/IR/IR.h>

#include <unordered_map>
#include <map>
#include <sstream>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#pragma clang diagnostic pop

namespace circuitous {

struct RawNodesCounter_ : UniqueVisitor<RawNodesCounter_> {
  using parent = UniqueVisitor<RawNodesCounter_>;

  // We want it ordered, so we can do diff effectively
  // The key type is important! We use it later as
  // overload resolution in diff. Refactor!
  std::map<uint64_t, uint64_t> nodes;
  std::map<uint32_t, uint64_t> llvm_ops;

  void Process(Operation *op) {
    const auto &[it, _] = nodes.try_emplace(op->op_code, 0);
    ++it->second;
  }

  void VisitOperation(Operation *op) {
    Process(op);
    op->Traverse(*this);
  }

  void VisitOperation(LLVMOperation *op) {
    Process(op);
    const auto &[it, _] = llvm_ops.try_emplace(op->llvm_op_code, 0);
    ++it->second;
    op->Traverse(*this);
  }

  void Run(Operation *op) {
    Visit(op);
  }

  auto Export() { return *this; }

  template<typename CB>
  void Diff(RawNodesCounter_ &o, CB cb) {
    auto diff = [&cb](auto self_, auto other_) {
      auto self = self_.begin();
      auto other = other_.begin();
      while (self != self_.end() && other != other_.end()) {
        if (self->first == other->first){
          cb(self->first, self->second, other->second);
          ++self; ++other;
        } else if (self->first < other->first) {
          cb(self->first, self->second, 0ull);
          ++self;
        } else {
          cb(other->first, 0ull, other->second);
          ++other;
        }
      }
      while (self != self_.end()) {
        cb(self->first, self->second, 0ull);
        ++self;
      }
      while (other != other_.end()) {
        cb(other->first, 0ull, other->second);
        ++other;
      }
    };

    diff(nodes, o.nodes);
    diff(llvm_ops, o.llvm_ops);
  }
};

using RawNodesCounter = RawNodesCounter_;

template<typename Collector>
struct Printer {

  template<typename OS>
  static void Print(OS &ss, Collector &self) {
    ss << "Node counts:" << std::endl;
    for (auto &[op_code, count] : self.nodes) {
      ss << " " << to_string(op_code) << " " << count << std::endl;

      if (op_code == Operation::kLLVMOperation) {
        for (auto &[llvm_op, count] : self.llvm_ops) {
          ss << "\t " << llvm::Instruction::getOpcodeName(llvm_op) << " " << count << std::endl;
        }
      }
    }
    ss << std::endl;
  }

  static auto _to_string(uint64_t what) {
    return to_string(what);
  }

  static auto _to_string(uint32_t what) {
    return llvm::Instruction::getOpcodeName(what);
  }

  template<typename OS>
  static void Diff(OS &os, Collector &self, Collector &other) {
    auto red = [](auto what) -> std::string {
      return "\033[91m" + std::to_string(what) + "\033[0m";
    };
    auto green = [](auto what) -> std::string {
      return "\033[92m" + std::to_string(what) + "\033[0m";
    };

    auto log = [&](auto what, auto orig, auto updated) {
      if (orig == updated) {
        return;
      }
      int64_t changed_by = static_cast<int64_t>(updated) - static_cast<int64_t>(orig);
      os << " " << _to_string(what) << "( ";
      if (changed_by > 0) {
        os << red(changed_by);
      } else if (changed_by < 0) {
        os << green(changed_by);
      }
      os << " )" << std::endl;
    };
    self.Diff(other, log);
  }
};

template<typename Collector, typename P>
struct PrintStatistics_ : Collector, P {
  template<typename OS>
  void Run(Operation *op, OS &os) {
    this->Collector::Run(op);
    this->P::Print(os, *this);
  }
};

using StatsPrinter = PrintStatistics_<RawNodesCounter, Printer<RawNodesCounter>>;

// TODO(lukas): Make more configurable
static inline std::string GetStats(Operation *op) {
  std::stringstream ss;
  StatsPrinter().Run(op, ss);
  return ss.str();
}

} // namespace circuitous