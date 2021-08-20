// Copyright (c) 2021 Daniel Frey and Dr. Colin Hirsch
// Please see LICENSE for license or visit https://github.com/taocpp/taopq/

#ifndef TAO_PQ_RESULT_TRAITS_ARRAY_HPP
#define TAO_PQ_RESULT_TRAITS_ARRAY_HPP

#include <array>
#include <cstring>
#include <list>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <tao/pq/result_traits.hpp>

namespace tao::pq
{
   namespace internal
   {
      template< typename... >
      inline constexpr bool has_null = false;

      template< typename T >
      inline constexpr bool has_null< T, decltype( T::null() ) > = true;

      template< typename >
      inline constexpr bool is_array_result = false;

      // TODO:
      // template< typename T, std::size_t N >
      // inline constexpr bool is_array_result< std::array< T, N > > = true;

      template< typename... Ts >
      inline constexpr bool is_array_result< std::list< Ts... > > = true;

      template< typename... Ts >
      inline constexpr bool is_array_result< std::set< Ts... > > = true;

      template< typename... Ts >
      inline constexpr bool is_array_result< std::unordered_set< Ts... > > = true;

      template< typename... Ts >
      inline constexpr bool is_array_result< std::vector< Ts... > > = true;

      template< typename T >
      void parse_elements( T& container, const char*& value );

      template< typename T >
      void parse_element( T& container, const char*& value )
      {
         using value_type = typename T::value_type;
         if constexpr( is_array_result< value_type > ) {
            value_type element;
            parse_elements( element, value );
            container.push_back( std::move( element ) );
         }
         else {
            if( *value == '"' ) {
               ++value;
               std::string input;
               while( const auto* pos = std::strpbrk( value, "\\\"" ) ) {
                  if( *pos == '\\' ) {
                     input.append( value, pos++ );
                     input += *pos++;
                     value = pos;
                  }
                  else {
                     input.append( value, pos++ );
                     value = pos;
                     break;
                  }
               }
               container.push_back( result_traits< value_type >::from( input.c_str() ) );
            }
            else if( *value != '}' ) {
               if( const auto* end = std::strpbrk( value, ",;}" ) ) {
                  std::string input( value, end );
                  if( input == "NULL" ) {
                     if constexpr( has_null< result_traits< value_type >, value_type > ) {
                        container.push_back( result_traits< value_type >::null() );
                     }
                     else {
                        throw std::runtime_error( "invalid array input, unexpected NULL value" );
                     }
                  }
                  else {
                     container.push_back( result_traits< value_type >::from( input.c_str() ) );
                  }
                  value = end;
               }
               else {
                  throw std::runtime_error( "invalid array input, unterminated unquoted string" );
               }
            }
         }
      }

      template< typename T >
      void parse_elements( T& container, const char*& value )
      {
         if( *value++ != '{' ) {
            throw std::runtime_error( "invalid array input, expected '{'" );
         }
         while( true ) {
            parse_element( container, value );
            switch( *value++ ) {
               case ',':
               case ';':
                  continue;
               case '}':
                  return;
               default:
                  throw std::runtime_error( "invalid array input, expected ',', ';', or '}'" );
            }
         }
      }

   }  // namespace internal

   template< typename T >
   struct result_traits< T, std::enable_if_t< internal::is_array_result< T > > >
   {
      static auto from( const char* value ) -> T
      {
         T nrv;
         internal::parse_elements( nrv, value );
         if( *value != '\0' ) {
            throw std::runtime_error( "invalid array input, unexpected additional data" );
         }
         return nrv;
      }
   };

}  // namespace tao::pq

#endif