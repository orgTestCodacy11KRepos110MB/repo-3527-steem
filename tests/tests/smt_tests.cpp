#if defined IS_TEST_NET && defined STEEM_ENABLE_SMT

#include <boost/test/unit_test.hpp>

#include <steem/protocol/exceptions.hpp>
#include <steem/protocol/hardfork.hpp>

#include <steem/chain/database.hpp>
#include <steem/chain/database_exceptions.hpp>
#include <steem/chain/steem_objects.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace steem::chain;
using namespace steem::protocol;
using fc::string;
using boost::container::flat_set;

BOOST_FIXTURE_TEST_SUITE( smt_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( elevate_account_validate )
{
   try
   {
      smt_elevate_account_operation op;
      op.account = "@@@@@";
      op.fee = ASSET( "1.000 TESTS" );
      STEEM_REQUIRE_THROW( op.validate(), fc::exception );

      op.account = "alice";
      op.validate();

      op.fee.amount = -op.fee.amount;
      STEEM_REQUIRE_THROW( op.validate(), fc::exception );

      // passes with MAX_SHARE_SUPPLY
      op.fee.amount = STEEM_MAX_SHARE_SUPPLY;
      op.validate();

      // fails with MAX_SHARE_SUPPLY+1
      ++op.fee.amount;
      STEEM_REQUIRE_THROW( op.validate(), fc::exception );

      op.fee = ASSET( "1.000 WRONG" );
      STEEM_REQUIRE_THROW( op.validate(), fc::exception );
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( elevate_account_authorities )
{
   try
   {
      smt_elevate_account_operation op;
      op.account = "alice";
      op.fee = ASSET( "1.000 TESTS" );

      flat_set< account_name_type > auths;
      flat_set< account_name_type > expected;

      op.get_required_owner_authorities( auths );
      BOOST_REQUIRE( auths == expected );

      op.get_required_posting_authorities( auths );
      BOOST_REQUIRE( auths == expected );

      expected.insert( "alice" );
      op.get_required_active_authorities( auths );
      BOOST_REQUIRE( auths == expected );
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( elevate_account_apply )
{
   try
   {
      set_price_feed( price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) ) );

      ACTORS( (alice)(bob) )

      fund( "alice", 10 * 1000 * 1000 );

      smt_elevate_account_operation op;

      op.fee = ASSET( "1000.000 TBD" );
      op.account = "alice";

      signed_transaction tx;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx.sign( alice_private_key, db->get_chain_id() );
      // throw due to insufficient balance
      STEEM_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::exception );

      convert( "alice", ASSET( "5000.000 TESTS" ) );
      db->push_transaction( tx, 0 );

      // Check the account cannot elevate itself twice
      STEEM_REQUIRE_THROW( db->push_transaction( tx, database::skip_transaction_dupe_check ), fc::exception );

      // TODO:
      // - Check that 1000 TESTS throws
      // - Check that less than 1000 TBD throws
      // - Check that more than 1000 TBD succeeds
      //
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( set_setup_parameters_validate )
{
   try
   {
      smt_set_setup_parameters_operation op;
      op.control_account = "####";
      STEEM_REQUIRE_THROW( op.validate(), fc::exception ); // invalid account name

      op.control_account = "dany";
      op.validate();

      op.setup_parameters.emplace(smt_param_allow_vesting());
      op.setup_parameters.emplace(smt_param_allow_voting());
      op.validate();

      op.setup_parameters.emplace(smt_param_allow_vesting({false}));
      op.setup_parameters.emplace(smt_param_allow_voting({false}));
      op.validate();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( set_setup_parameters_authorities )
{
   try
   {
      smt_set_setup_parameters_operation op;
      op.control_account = "dany";

      flat_set<account_name_type> auths;
      flat_set<account_name_type> expected;

      op.get_required_owner_authorities( auths );
      BOOST_REQUIRE( auths == expected );

      op.get_required_posting_authorities( auths );
      BOOST_REQUIRE( auths == expected );

      expected.insert( "dany" );
      op.get_required_active_authorities( auths );
      BOOST_REQUIRE( auths == expected );
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( set_setup_parameters_apply )
{
   try
   {
      set_price_feed( price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) ) );
      ACTORS( (dany)(eddy) )

      fund( "dany", 5000 );
      convert( "dany", ASSET( "5000.000 TESTS" ) );
      
      smt_set_setup_parameters_operation op;
      op.control_account = "dany";

      signed_transaction tx;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx.sign( dany_private_key, db->get_chain_id() );
      STEEM_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::exception ); // account not elevated

      // elevate account
      {
         smt_elevate_account_operation op;
         op.fee = ASSET( "1000.000 TBD" );
         op.account = "dany";

         signed_transaction tx;
         tx.operations.push_back( op );
         tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
         tx.sign( dany_private_key, db->get_chain_id() );
         db->push_transaction( tx, 0 );
      }

      db->push_transaction( tx, 0 );

      db->push_transaction( tx, database::skip_transaction_dupe_check );

      signed_transaction tx1;

      op.setup_parameters.clear();
      op.setup_parameters.emplace( smt_param_allow_vesting() );
      op.setup_parameters.emplace( smt_param_allow_voting() );
      tx1.operations.push_back( op );
      tx1.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx1.sign( eddy_private_key, db->get_chain_id() );
      
      STEEM_REQUIRE_THROW( db->push_transaction( tx1, 0 ), fc::exception ); // wrong private key
      
      signed_transaction tx2;

      op.setup_parameters.clear();
      op.setup_parameters.emplace( smt_param_allow_vesting({false}) );
      op.setup_parameters.emplace( smt_param_allow_voting({false}) );
      tx2.operations.push_back( op );
      tx2.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx2.sign( dany_private_key, db->get_chain_id() );

      db->push_transaction( tx2, 0 );
      
      signed_transaction tx3;

      op.setup_parameters.clear();
      op.setup_parameters.emplace( smt_param_allow_vesting() );
      tx3.operations.push_back( op );
      tx3.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx3.sign( dany_private_key, db->get_chain_id() );

      db->push_transaction( tx3, 0 );
      
      // TODO:
      // - check applying smt_set_setup_parameters_operation after setup completed
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif