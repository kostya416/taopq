// Copyright (c) 2016-2021 Daniel Frey and Dr. Colin Hirsch
// Please see LICENSE for license or visit https://github.com/taocpp/taopq/

#ifndef TAO_PQ_ROW_HPP
#define TAO_PQ_ROW_HPP

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <tao/pq/field.hpp>
#include <tao/pq/internal/demangle.hpp>
#include <tao/pq/internal/dependent_false.hpp>
#include <tao/pq/internal/printf.hpp>
#include <tao/pq/internal/unreachable.hpp>
#include <tao/pq/internal/zsv.hpp>
#include <tao/pq/result_traits.hpp>

namespace tao::pq
{
   class result;

   class row
   {
   protected:
      friend class result;

      const result* m_result = nullptr;
      std::size_t m_row = 0;
      std::size_t m_offset = 0;
      std::size_t m_columns = 0;

      row() = default;

      row( const result& in_result, const std::size_t in_row, const std::size_t in_offset, const std::size_t in_columns ) noexcept
         : m_result( &in_result ),
           m_row( in_row ),
           m_offset( in_offset ),
           m_columns( in_columns )
      {}

      void ensure_column( const std::size_t column ) const;

   public:
      [[nodiscard]] auto slice( const std::size_t offset, const std::size_t in_columns ) const -> row;

      [[nodiscard]] auto columns() const noexcept -> std::size_t
      {
         return m_columns;
      }

      [[nodiscard]] auto name( const std::size_t column ) const -> std::string;
      [[nodiscard]] auto index( const internal::zsv in_name ) const -> std::size_t;

      [[nodiscard]] auto is_null( const std::size_t column ) const -> bool;
      [[nodiscard]] auto get( const std::size_t column ) const -> const char*;

      template< typename T >
      [[nodiscard]] auto get( const std::size_t column ) const -> T
      {
         if constexpr( result_traits_size< T > == 0 ) {
            static_assert( internal::dependent_false< T >, "tao::pq::result_traits<T>::size yields zero" );
            TAO_PQ_UNREACHABLE;  // LCOV_EXCL_LINE
         }
         else if constexpr( result_traits_size< T > == 1 ) {
            if constexpr( result_traits_has_null< T > ) {
               if( is_null( column ) ) {
                  return result_traits< T >::null();
               }
            }
            else {
               ensure_column( column );
            }
            return result_traits< T >::from( get( column ) );
         }
         else {
            return result_traits< T >::from( slice( column, result_traits_size< T > ) );
         }
      }

      template< typename T >
      [[nodiscard]] auto optional( const std::size_t column ) const
      {
         return get< std::optional< T > >( column );
      }

      template< typename T >
      [[nodiscard]] auto as() const -> T
      {
         if( result_traits_size< T > != m_columns ) {
            throw std::out_of_range( internal::printf( "datatype (%s) requires %zu columns, but row/slice has %zu columns", internal::demangle< T >().c_str(), result_traits_size< T >, m_columns ) );
         }
         return get< T >( 0 );
      }

      template< typename T >
      [[nodiscard]] auto optional() const
      {
         return as< std::optional< T > >();
      }

      template< typename T, typename U >
      [[nodiscard]] auto pair() const
      {
         return as< std::pair< T, U > >();
      }

      template< typename... Ts >
      [[nodiscard]] auto tuple() const
      {
         return as< std::tuple< Ts... > >();
      }

      [[nodiscard]] auto at( const std::size_t column ) const -> field;

      [[nodiscard]] auto operator[]( const std::size_t column ) const noexcept -> field
      {
         return field( *this, m_offset + column );
      }

      [[nodiscard]] auto at( const internal::zsv in_name ) const -> field
      {
         // row::index does the necessary checks, so we forward to operator[]
         return ( *this )[ row::index( in_name ) ];
      }

      [[nodiscard]] auto operator[]( const internal::zsv in_name ) const -> field
      {
         return ( *this )[ row::index( in_name ) ];
      }
   };

   template< typename T >
   auto field::as() const
      -> std::enable_if_t< result_traits_size< T > == 1, T >
   {
      return m_row.get< T >( m_column );
   }

}  // namespace tao::pq

#endif
