// Copyright (c) 2021 Daniel Frey and Dr. Colin Hirsch
// Please see LICENSE for license or visit https://github.com/taocpp/taopq/

#include <tao/pq/table_reader.hpp>

#include <libpq-fe.h>

#include <cstring>

#include <tao/pq/connection.hpp>
#include <tao/pq/internal/transaction.hpp>
#include <tao/pq/result.hpp>
#include <tao/pq/transaction.hpp>

namespace tao::pq
{
   namespace internal
   {
      class table_reader_transaction final
         : public subtransaction_base< parameter_text_traits >  // note: the traits are never used
      {
      public:
         explicit table_reader_transaction( const std::shared_ptr< connection >& connection )
            : subtransaction_base< parameter_text_traits >( connection )
         {}

         ~table_reader_transaction() override
         {}

         table_reader_transaction( const table_reader_transaction& ) = delete;
         table_reader_transaction( table_reader_transaction&& ) = delete;
         void operator=( const table_reader_transaction& ) = delete;
         void operator=( table_reader_transaction&& ) = delete;

      private:
         void v_commit() override
         {}

         void v_rollback() override
         {}
      };

   }  // namespace internal

   table_reader::table_reader( const std::shared_ptr< internal::transaction >& transaction, const std::string& statement )
      : m_previous( transaction ),
        m_transaction( std::make_shared< internal::table_reader_transaction >( transaction->m_connection ) ),
        m_buffer( nullptr, &PQfreemem )
   {
      result( PQexecParams( m_transaction->underlying_raw_ptr(), statement.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 0 ), result::mode_t::expect_copy_out );
   }

   table_reader::~table_reader()
   {
      if( m_transaction ) {
         PQputCopyEnd( m_transaction->underlying_raw_ptr(), "cancelled in dtor" );
      }
   }

   bool table_reader::fetch_next()
   {
      char* buffer = nullptr;
      const auto result = PQgetCopyData( m_transaction->underlying_raw_ptr(), &buffer, 0 );
      m_buffer.reset( buffer );
      switch( result ) {
         case 0:
            // unreachable
            break;
         case -1:
            return false;
         case -2:
            throw std::runtime_error( "PQgetCopyData() failed: " + m_transaction->m_connection->error_message() );
      }
      return true;
   }

   const char* table_reader::get_data()
   {
      return static_cast< const char* >( m_buffer.get() );
   }

   // auto table_reader::commit() -> std::size_t
   // {
   //    const int r = PQputCopyEnd( m_transaction->underlying_raw_ptr(), nullptr );
   //    if( r != 1 ) {
   //       throw std::runtime_error( "PQputCopyEnd() failed: " + m_transaction->m_connection->error_message() );
   //    }
   //    const auto rows_affected = result( PQgetResult( m_transaction->underlying_raw_ptr() ) ).rows_affected();
   //    m_transaction.reset();
   //    m_previous.reset();
   //    return rows_affected;
   // }

}  // namespace tao::pq