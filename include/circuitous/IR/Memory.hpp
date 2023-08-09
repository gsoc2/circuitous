/*
 * Copyright (c) 2021 Trail of Bits, Inc.
 */

#pragma once

#include <circuitous/Util/TypeList.hpp>

#include <circuitous/Support/Check.hpp>

#include <array>
#include <optional>

namespace circ::irops::memory
{
    struct Layout
    {
        static inline constexpr uint32_t e_num = 8;

        // Index is position in vector, and values are [ size, value of given chunk ].
        template< typename T >
        using attrs_t = std::array< T, e_num >;

        attrs_t< uint32_t > defs;

        Layout( uint32_t ptr_size )
            : defs( { 1u, 1u, 6u, 4u, 4u, ptr_size, ptr_size, 64u  } )
        {}

        auto size() const
        {
            uint32_t out = 0;
            for ( auto s : defs )
                out += s;
            return out;
        }

        bool operator==( const Layout & ) const = default;
    };

    static inline uint32_t size( uint32_t ptr_size ) { return Layout( ptr_size ).size(); }


    template< typename T >
    static T make( auto ptr_size_ )
    {
      auto ptr_size = static_cast< uint32_t >( ptr_size_ );
      check( ptr_size == 32 || ptr_size == 64 );
      return T( ptr_size );
    }

    template< typename V >
    struct Parsed : Layout
    {
        // NOTE(lukas): Cannot use `std::array`, because for example `z3::expr` is
        //              not default constructible.
        std::vector< V > vals;

        using Layout::Layout;
        Parsed( uint32_t size, std::vector< V > vals )
            : Layout( size ), vals( std::move(vals ) )
        {}

        // TODO(lukas): Move to debug
        void validate() const { check( vals.size() == e_num ); }

        auto used()      const { validate(); return vals[ 0 ]; }
        auto mode()      const { validate(); return vals[ 1 ]; }
        auto reserved()  const { validate(); return vals[ 2 ]; }
        auto id()        const { validate(); return vals[ 3 ]; }
        auto size()      const { validate(); return vals[ 4 ]; }
        auto addr()      const { validate(); return vals[ 5 ]; }
        auto value()     const { validate(); return vals[ 6 ]; }
        auto timestamp() const { validate(); return vals[ 7 ]; }

        template< uint32_t I >
        auto as_tuple() const
        {
            auto current = std::make_tuple( vals [ I ] );
            if constexpr ( I == 0 )
                return current;
            else
                return std::tuple_cat( std::move( current ), as_tuple< I - 1 >() );
        }

        // TODO(lukas): Implement custom `std::get`
        auto as_tuple() const
        {
            check( e_num == vals.size() );
            return as_tuple< e_num >();
        }

        template< typename F >
        void apply( F &&f ) const
        {
            check( e_num == vals.size() );
            for ( std::size_t i = 0; i < e_num; ++i )
                f( this->defs[ i ], vals[ i ] );
        }

        bool operator==( const Parsed< V > & ) const = default;

        std::string to_string( auto &&fmt ) const
        {
            std::stringstream ss;
            ss << "Parsed< V >:\n";
            for ( std::size_t i = 0; i < vals.size(); ++i )
                ss << "\t" << i << ": " << fmt( vals[ i ] ) << "\n";
            return ss.str();
        }
    };


    template< typename V, typename Inserter >
    static void construct( const Parsed< V > &parsed, Inserter &insert )
    {
        auto current = 0u;
        auto exec = [ & ]( const auto &def, const auto &val )
        {
            insert( val, current, def );
            current += def;
        };
        parsed.apply( exec );
    }

    template< typename V, typename Extractor >
    static Parsed< V > parse( V call, Extractor extract, auto size )
    {
        auto current = 0u;
        auto do_extract = [&](auto e_size) -> V
        {
            auto out = extract( call, current, e_size );
            current += e_size;
            return out;
        };

        auto out = make< Parsed< V > >( size );
        for ( std::size_t i = 0; i < out.e_num; ++i )
          out.vals.push_back( do_extract( out.defs[ i ] ) );
        return out;
    }
} // namespace circ::irops::memory
