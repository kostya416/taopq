// Copyright (c) 2016-2022 Daniel Frey and Dr. Colin Hirsch
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include <charconv>

#include <tao/pq/exception.hpp>
#include <tao/pq/result.hpp>

#include <tao/pq/internal/from_chars.hpp>
#include <tao/pq/internal/printf.hpp>
#include <tao/pq/internal/unreachable.hpp>

namespace tao::pq
{
   void result::check_has_result_set() const
   {
      if( m_columns == 0 ) {
         throw std::logic_error( "statement does not yield a result set" );
      }
   }

   void result::check_row( const std::size_t row ) const
   {
      check_has_result_set();
      if( !( row < m_rows ) ) {
         if( m_rows == 0 ) {
            throw std::out_of_range( internal::printf( "row %zu out of range, result is empty", row ) );
         }
         throw std::out_of_range( internal::printf( "row %zu out of range (0-%zu)", row, m_rows - 1 ) );
      }
   }

   result::result( PGresult* pgresult )
      : m_pgresult( pgresult, &PQclear ),
        m_columns( PQnfields( pgresult ) ),
        m_rows( PQntuples( pgresult ) )
   {
      switch( PQresultStatus( pgresult ) ) {
         case PGRES_COMMAND_OK:
         case PGRES_TUPLES_OK:
            return;

         case PGRES_EMPTY_QUERY:
            throw std::runtime_error( "unexpected empty query" );

         case PGRES_COPY_IN:
         case PGRES_COPY_OUT:
            TAO_PQ_UNREACHABLE;  // LCOV_EXCL_LINE

         default:
            internal::throw_sqlstate( pgresult );
      }
   }

   auto result::has_rows_affected() const noexcept -> bool
   {
      const char* str = PQcmdTuples( m_pgresult.get() );
      return str[ 0 ] != '\0';
   }

   auto result::rows_affected() const -> std::size_t
   {
      const char* str = PQcmdTuples( m_pgresult.get() );
      if( str[ 0 ] == '\0' ) {
         throw std::logic_error( "statement does not return affected rows" );
      }
      return internal::from_chars< std::size_t >( str );
   }

   auto result::name( const std::size_t column ) const -> std::string
   {
      if( column >= m_columns ) {
         throw std::out_of_range( internal::printf( "column %zu out of range (0-%zu)", column, m_columns - 1 ) );
      }
      return PQfname( m_pgresult.get(), static_cast< int >( column ) );
   }

   auto result::index( const internal::zsv in_name ) const -> std::size_t
   {
      const int column = PQfnumber( m_pgresult.get(), in_name );
      if( column < 0 ) {
         assert( column == -1 );
         check_has_result_set();
         throw std::out_of_range( "column not found: " + std::string( in_name ) );
      }
      return column;
   }

   auto result::empty() const -> bool
   {
      return size() == 0;
   }

   auto result::size() const -> std::size_t
   {
      check_has_result_set();
      return m_rows;
   }

   auto result::begin() const -> result::const_iterator
   {
      check_has_result_set();
      return const_iterator( row( *this, 0, 0, m_columns ) );
   }

   auto result::end() const -> result::const_iterator
   {
      return const_iterator( row( *this, size(), 0, m_columns ) );
   }

   auto result::is_null( const std::size_t row, const std::size_t column ) const -> bool
   {
      check_row( row );
      if( column >= m_columns ) {
         throw std::out_of_range( internal::printf( "column %zu out of range (0-%zu)", column, m_columns - 1 ) );
      }
      return PQgetisnull( m_pgresult.get(), static_cast< int >( row ), static_cast< int >( column ) ) != 0;
   }

   auto result::get( const std::size_t row, const std::size_t column ) const -> const char*
   {
      if( is_null( row, column ) ) {
         throw std::runtime_error( internal::printf( "unexpected NULL value in row %zu column %zu = %s", row, column, name( column ).c_str() ) );
      }
      return PQgetvalue( m_pgresult.get(), static_cast< int >( row ), static_cast< int >( column ) );
   }

   auto result::at( const std::size_t row ) const -> pq::row
   {
      check_row( row );
      return ( *this )[ row ];
   }

}  // namespace tao::pq
