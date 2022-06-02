/*
 * Copyright (c) 2022 Trail of Bits, Inc.
 */

#pragma once

#include <circuitous/Util/Logging.hpp>
#include <circuitous/IR/Memory.hpp>

CIRCUITOUS_RELAX_WARNINGS
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/ADT/APInt.h>
CIRCUITOUS_UNRELAX_WARNINGS

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace circ
{
    struct Circuit;
} // namespace circ

namespace circ::run
{
    // NOTE(lukas): We could template all the classes, but compile time.
    using raw_value_type = llvm::APInt;
    using value_type = std::optional< llvm::APInt >;

    struct Memory
    {
        using memory_map_t = std::unordered_map< uint64_t, raw_value_type >;

        uint32_t hint_size;
        memory_map_t memory;

        Memory(Circuit *circuit);
        Memory(const Memory &) = default;
        Memory(Memory &&) = default;

        Memory &operator=(const Memory &) = default;
        Memory &operator=(Memory &&) = default;

        memory_map_t take_memory() { return std::move(memory); }

        bool defined(uint64_t addr, std::size_t size) const;

        value_type load(uint64_t addr, std::size_t size_) const;
        void store(uint64_t addr, raw_value_type val);

        using Parsed = irops::memory::Parsed< llvm::APInt >;

        Parsed deconstruct(const llvm::APInt &value);
        llvm::APInt construct(const Parsed &parsed);
    };

} // namespace circ::run