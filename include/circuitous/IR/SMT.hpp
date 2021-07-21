/*
 * Copyright (c) 2021 Trail of Bits, Inc.
 */

#include <functional>
#include <memory>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <glog/logging.h>
#include <llvm/IR/InstrTypes.h>
#include <z3++.h>
#pragma clang diagnostic pop

#include <circuitous/IR/IR.h>
#include <circuitous/IR/Storage.hpp>
#include <circuitous/IR/Circuit.hpp>

namespace circ
{

  template< typename Derived >
  struct BaseSMTVisitor : Visitor< Derived >
  {
    using Base = Visitor< Derived >;
    using Base::Dispatch;
    using Base::Visit;

    z3::context ctx;

    z3::expr to_bv(z3::expr expr)
    {
      return z3::ite(expr, ctx.bv_val(1, 1), ctx.bv_val(0, 1));
    }

    z3::expr true_bv() { return ctx.bv_val(1, 1); }

    z3::expr uninterpreted(Operation *op, const std::string &name, z3::sort result_sort)
    {
      z3::expr_vector args(ctx);
      z3::sort_vector args_sorts(ctx);
      for (const auto &arg : op->operands) {
        args.push_back(Dispatch(arg));
        args_sorts.push_back(args.back().get_sort());
      }

      auto decl = z3::function(name.c_str(), args_sorts, result_sort);
      return decl(args);
    }

    z3::expr uninterpreted(Operation *op, const std::string &name)
    {
      return uninterpreted(op, name, ctx.bv_sort(op->size));
    }

    z3::expr constant(Operation *op, const std::string &name)
    {
      return ctx.bv_const(name.c_str(), op->size);
    }

    z3::expr constant(Operation *op)
    {
      return constant(op, op->Name());
    }

    z3::expr lhs(Operation *op) { return Dispatch(op->operands[0]); };
    z3::expr rhs(Operation *op) { return Dispatch(op->operands[1]); };

    z3::expr Visit(Operation *op)
    {
      LOG(FATAL) << "Unhandled operation: " << op->Name();
    }
  };

  template< typename Derived >
  struct IRToSMTConstantsVisitor : BaseSMTVisitor< Derived >
  {
    using Base = BaseSMTVisitor< Derived >;
    using Base::Dispatch;
    using Base::Visit;
    using Base::constant;
    using Base::ctx;

    z3::expr Visit(InputInstructionBits *op) { return constant(op, "InputBits"); }
    z3::expr Visit(InputRegister *op) { return constant(op); }
    z3::expr Visit(OutputRegister *op) { return constant(op); }

    z3::expr Visit(Advice *op)
    {
      auto name = "Advice." + std::to_string(reinterpret_cast<uint64_t>(op));
      return ctx.bv_const(name.c_str(), op->size);
    }

    z3::expr Visit(PopulationCount *op)     { return constant(op, "Population"); }
    z3::expr Visit(CountLeadingZeroes *op)  { return constant(op, "LeadingZeros"); }
    z3::expr Visit(CountTrailingZeroes *op) { return constant(op, "TrailingZeros"); }

    z3::expr Visit(InputTimestamp *op)  { return constant(op); }
    z3::expr Visit(OutputTimestamp *op) { return constant(op); }
    z3::expr Visit(InputErrorFlag *op)  { return constant(op); }
    z3::expr Visit(OutputErrorFlag *op) { return constant(op); }

    z3::expr Visit(Undefined *op) { return constant(op); }

    z3::expr Visit(Constant *op)
    {
      auto bits = std::make_unique<bool[]>(op->size);

      std::size_t idx = 0;
      for (auto bit : op->bits)
        bits[idx++] = bit != '0';

      return ctx.bv_val(op->size, bits.get());
    }
  };

  template< typename Derived >
  struct IRToSMTOpsVisitor : IRToSMTConstantsVisitor< Derived >
  {
    using Base = IRToSMTConstantsVisitor< Derived >;
    using Base::Dispatch;
    using Base::Visit;
    using Base::to_bv;
    using Base::lhs;
    using Base::rhs;

    z3::expr Visit(Add *op) { return lhs(op) + rhs(op); }
    z3::expr Visit(Sub *op) { return lhs(op) - rhs(op); }
    z3::expr Visit(Mul *op) { return lhs(op) * rhs(op); }

    z3::expr Visit(UDiv *op) { return z3::udiv(lhs(op), rhs(op)); }
    z3::expr Visit(SDiv *op) { return lhs(op) / rhs(op); }

    z3::expr Visit(CAnd *op) { return lhs(op) & rhs(op); }
    z3::expr Visit(COr *op)  { return lhs(op) | rhs(op); }
    z3::expr Visit(CXor *op) { return lhs(op) ^ rhs(op); }

    z3::expr Visit(Shl *op)  { return z3::shl(lhs(op), rhs(op)); }
    z3::expr Visit(LShr *op) { return z3::lshr(lhs(op), rhs(op)); }
    z3::expr Visit(AShr *op) { return z3::ashr(lhs(op), rhs(op)); }

    z3::expr Visit(Trunc *op) { return lhs(op).extract(op->size - 1, 0); }

    z3::expr Visit(ZExt *op)
    {
      auto diff = op->size - op->operands[0]->size;
      return z3::zext(lhs(op), diff);
    }

    z3::expr Visit(SExt *op)
    {
      auto diff = op->size - op->operands[0]->size;
      return z3::sext(lhs(op), diff);
    }

    z3::expr Visit(Icmp_ult *op) { return to_bv(z3::ult(lhs(op), rhs(op))); }
    z3::expr Visit(Icmp_slt *op) { return to_bv(z3::slt(lhs(op), rhs(op))); }
    z3::expr Visit(Icmp_ugt *op) { return to_bv(z3::ugt(lhs(op), rhs(op))); }
    z3::expr Visit(Icmp_eq *op)  { return to_bv(lhs(op) == rhs(op)); }
    z3::expr Visit(Icmp_ne *op)  { return to_bv(lhs(op) != rhs(op)); }
    z3::expr Visit(Icmp_uge *op) { return to_bv(z3::uge(lhs(op), rhs(op))); }
    z3::expr Visit(Icmp_ule *op) { return to_bv(z3::ule(lhs(op), rhs(op))); }
    z3::expr Visit(Icmp_sgt *op) { return to_bv(lhs(op) > rhs(op)); }
    z3::expr Visit(Icmp_sge *op) { return to_bv(lhs(op) >= rhs(op)); }
    z3::expr Visit(Icmp_sle *op) { return to_bv(z3::sle(lhs(op), rhs(op))); }

    z3::expr Visit(Extract *op)
    {
      auto val = Dispatch(op->operands[0]);
      auto hi = op->high_bit_exc - 1;
      auto lo = op->low_bit_inc;
      return val.extract(hi, lo);
    }
  };

  struct IRToSMTVisitor : IRToSMTOpsVisitor<IRToSMTVisitor>
  {
    using Base = IRToSMTOpsVisitor<IRToSMTVisitor>;
    using Base::Dispatch;
    using Base::Visit;

    z3::expr Visit(Not *op) { return uninterpreted(op, "not"); }

    z3::expr Visit(Concat *op) { return uninterpreted(op, "Concat"); }
    z3::expr Visit(Select *op) { return uninterpreted(op, "Select"); }
    z3::expr Visit(Parity *op) { return uninterpreted(op, "Parity"); }

    z3::expr Visit(BSelect *op) { return uninterpreted(op, "BSelect"); }

    z3::expr Visit(RegConstraint *op)       { return uninterpreted(op, "RegisterConstraint"); }
    z3::expr Visit(PreservedConstraint *op) { return uninterpreted(op, "PreservedConstraint"); }
    z3::expr Visit(CopyConstraint *op)      { return uninterpreted(op, "CopyConstraint"); }
    z3::expr Visit(AdviceConstraint *op)    { return uninterpreted(op, "AdviceConstraint"); }
    z3::expr Visit(OnlyOneCondition *op)    { return uninterpreted(op, "OnlyOne"); }
    z3::expr Visit(DecodeCondition *op)     { return uninterpreted(op, "Decode"); }
    z3::expr Visit(VerifyInstruction *op)   { return uninterpreted(op, "Verify"); }

    z3::expr Visit(Circuit *op)
    {
      return uninterpreted(op, "Circuit", ctx.bool_sort());
    }
  };

  struct IRToBitBlastableSMTVisitor : IRToSMTOpsVisitor< IRToBitBlastableSMTVisitor >
  {
    using Base = IRToSMTOpsVisitor<  IRToBitBlastableSMTVisitor >;
    using Base::Dispatch;
    using Base::Visit;

    z3::expr accumulate(const auto &operands, const auto &fn)
    {
      auto dispatched = [&, fn] (const auto &lhs, auto rhs) { return fn(lhs, Dispatch(rhs)); };
      auto init = Dispatch(operands[0]);
      return std::accumulate(std::next(operands.begin()), operands.end(), init, dispatched);
    }

    // z3::expr Visit(Not *op) { return uninterpreted(op, "not"); }

    z3::expr Visit(Concat *op)
    {
      z3::expr_vector vec(ctx);
      for (auto o : op->operands) {
        vec.push_back(Dispatch(o));
      }
      return z3::concat(vec);
    }

    z3::expr Visit(Select *op)
    {
      auto index = Dispatch(op->operands[0]);
      auto value = Dispatch(op->operands[1]);

      auto bw = index.get_sort().bv_size();
      auto current = ctx.bv_val(0, bw);

      auto undef = ctx.bv_val(0, value.get_sort().bv_size());

      z3::expr result = z3::ite(current == index, value, undef);
      for (auto i = 2U; i < op->operands.size(); ++i) {
        value = Dispatch(op->operands[i]);
        current = ctx.bv_val(i, bw);
        result = z3::ite(current == index, value, result);
      }

      return result;
    }

    z3::expr Visit(BSelect *op)
    {
      auto cond = Dispatch(op->operands[0]);
      auto first = Dispatch(op->operands[1]);
      auto second = Dispatch(op->operands[2]);
      return z3::ite(cond == true_bv(), first, second);
    }

    z3::expr Visit(Parity *op)
    {
      auto operand = Dispatch(op->operands[0]);
      auto operand_size = operand.get_sort().bv_size();
      auto sum = operand.extract(0, 0);
      for (auto i = 1U; i < operand_size; ++i) {
        sum = sum ^ operand.extract(i, i);
      }
      return sum;
    }

    z3::expr Visit(RegConstraint *op)       { return to_bv(lhs(op) == rhs(op)); }
    z3::expr Visit(PreservedConstraint *op) { return to_bv(lhs(op) == rhs(op)); }
    z3::expr Visit(CopyConstraint *op)      { return to_bv(lhs(op) == rhs(op)); }
    z3::expr Visit(AdviceConstraint *op)    { return to_bv(lhs(op) == rhs(op)); }
    z3::expr Visit(DecodeCondition *op)     { return to_bv(lhs(op) == rhs(op)); }

    z3::expr Visit(OnlyOneCondition *op)
    {
      return accumulate(op->operands, std::bit_xor());
    }

    z3::expr Visit(VerifyInstruction *op)
    {
      return accumulate(op->operands, std::bit_and());
    }

    z3::expr Visit(Circuit *op)
    {
      auto expr = Dispatch(op->operands[0]);
      return expr != ctx.num_val(0, expr.get_sort());
    }
  };

} // namespace circ