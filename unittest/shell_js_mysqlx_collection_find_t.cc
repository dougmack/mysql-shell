/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/pointer_cast.hpp>

#include "gtest/gtest.h"
#include "shellcore/types.h"
#include "shellcore/lang_base.h"
#include "shellcore/types_cpp.h"

#include "shellcore/shell_core.h"
#include "shellcore/shell_jscript.h"
#include "test_utils.h"

#include "../modules/mod_mysqlx_collection_find.h"

namespace shcore {
  class Shell_js_mysqlx_collection_find_tests : public Crud_test_wrapper
  {
  protected:
    // You can define per-test set-up and tear-down logic as usual.
    virtual void SetUp()
    {
      Shell_core_test_wrapper::SetUp();

      bool initilaized(false);
      _shell_core->switch_mode(Shell_core::Mode_JScript, initilaized);

      // Sets the correct functions to be validated
      set_functions("find, fields, groupBy, having, sort, skip, limit, bind, execute");
    }
  };

  TEST_F(Shell_js_mysqlx_collection_find_tests, initialization)
  {
    exec_and_out_equals("var mysqlx = require('mysqlx').mysqlx;");

    exec_and_out_equals("var session = mysqlx.openNodeSession('" + _uri + "');");

    exec_and_out_equals("session.executeSql('drop schema if exists js_shell_test;')");
    exec_and_out_equals("session.executeSql('create schema js_shell_test;')");
    exec_and_out_equals("session.executeSql('use js_shell_test;')");
    exec_and_out_equals("session.executeSql(\"create table `collection1`(`doc` JSON, `_id` VARBINARY(16) GENERATED ALWAYS AS(unhex(json_unquote(json_extract(doc, '$._id')))) stored PRIMARY KEY)\")");
  }

  TEST_F(Shell_js_mysqlx_collection_find_tests, chain_combinations)
  {
    // NOTE: No data validation is done on this test, only tests
    //       different combinations of chained methods.
    exec_and_out_equals("var mysqlx = require('mysqlx').mysqlx;");

    exec_and_out_equals("var session = mysqlx.openSession('" + _uri + "');");

    exec_and_out_equals("var collection = session.js_shell_test.getCollection('collection1');");

    // Creates the collection find object
    exec_and_out_equals("var crud = collection.find();");

    //-------- ----------------------------------------------------
    // Tests the happy path validating only the right functions
    // are available following the chained call
    // this is CollectionFind.find().skip(#).limit(#).execute()
    //-------------------------------------------------------------
    // TODO: Function availability needs to be tested on after the rest of the functions
    //       once they are enabled
    {
      SCOPED_TRACE("Testing function availability after find.");
      ensure_available_functions("fields, groupBy, sort, limit, bind, execute");
    }

    // Now executes limit
    {
      SCOPED_TRACE("Testing function availability after limit.");
      exec_and_out_equals("crud.limit(1)");
      ensure_available_functions("skip, bind, execute");
    }

    // Now executes skip
    {
      SCOPED_TRACE("Testing function availability after skip.");
      exec_and_out_equals("crud.skip(1)");
      ensure_available_functions("bind execute");
    }
  }

  TEST_F(Shell_js_mysqlx_collection_find_tests, find_validations)
  {
    exec_and_out_equals("var mysqlx = require('mysqlx').mysqlx;");

    exec_and_out_equals("var session = mysqlx.openNodeSession('" + _uri + "');");

    exec_and_out_equals("var schema = session.getSchema('js_shell_test');");

    exec_and_out_equals("var collection = schema.getCollection('collection1');");

    // Testing the find function
    {
      SCOPED_TRACE("Testing parameter validation on find");
      exec_and_out_equals("collection.find();");
      exec_and_out_contains("collection.find(5);", "", "CollectionFind::find: Argument #1 is expected to be a string");
      exec_and_out_contains("collection.find('test = \"2');", "", "CollectionFind::find: Unterminated quoted string starting at 8");
      exec_and_out_equals("collection.find('test = \"2\"');");
    }

    {
      SCOPED_TRACE("Testing parameter validation on fields");
      exec_and_out_contains("collection.find().fields();", "", "Invalid number of arguments in CollectionFind::fields, expected 1 but got 0");
      exec_and_out_contains("collection.find().fields(5);", "", "CollectionFind::fields: Argument #1 is expected to be a string");
      exec_and_out_contains("collection.find().fields('name as alias');", "", "");
    }

    {
      SCOPED_TRACE("Testing parameter validation on groupBy");
      exec_and_out_contains("collection.find().groupBy();", "", "Invalid number of arguments in CollectionFind::groupBy, expected 1 but got 0");
      exec_and_out_contains("collection.find().groupBy(5);", "", "CollectionFind::groupBy: Argument #1 is expected to be a string");
      exec_and_out_contains("collection.find().groupBy('name');", "", "");
    }

    {
      SCOPED_TRACE("Testing parameter validation on sort");
      exec_and_out_contains("collection.find().sort();", "", "Invalid number of arguments in CollectionFind::sort, expected 1 but got 0");
      exec_and_out_contains("collection.find().sort(5);", "", "CollectionFind::sort: Argument #1 is expected to be a string");
      exec_and_out_contains("collection.find().sort('');", "", "CollectionFind::sort: not yet implemented.");
    }

    {
      SCOPED_TRACE("Testing parameter validation on limit");
      exec_and_out_contains("collection.find().limit();", "", "Invalid number of arguments in CollectionFind::limit, expected 1 but got 0");
      exec_and_out_contains("collection.find().limit('');", "", "CollectionFind::limit: Argument #1 is expected to be an unsigned int");
      exec_and_out_equals("collection.find().limit(5);");
    }

    {
      SCOPED_TRACE("Testing parameter validation on skip");
      exec_and_out_contains("collection.find().limit(1).skip();", "", "Invalid number of arguments in CollectionFind::skip, expected 1 but got 0");
      exec_and_out_contains("collection.find().limit(1).skip('');", "", "CollectionFind::skip: Argument #1 is expected to be an unsigned int");
      exec_and_out_equals("collection.find().limit(1).skip(5);");
    }

    exec_and_out_contains("collection.find().bind();", "", "CollectionFind::bind: not yet implemented.");
  }

  TEST_F(Shell_js_mysqlx_collection_find_tests, find_execution)
  {
    exec_and_out_equals("var mysqlx = require('mysqlx').mysqlx;");
    exec_and_out_equals("var session = mysqlx.openNodeSession('" + _uri + "');");
    exec_and_out_equals("var schema = session.getSchema('js_shell_test');");
    exec_and_out_equals("var collection = schema.getCollection('collection1');");

    exec_and_out_equals("var result = collection.add({name: 'jack', age: 17, gender: 'male'}).execute();");
    exec_and_out_equals("var result = collection.add({name: 'adam', age: 15, gender: 'male'}).execute();");
    exec_and_out_equals("var result = collection.add({name: 'brian', age: 14, gender: 'male'}).execute();");
    exec_and_out_equals("var result = collection.add({name: 'alma', age: 13, gender: 'female'}).execute();");
    exec_and_out_equals("var result = collection.add({name: 'carol', age: 14, gender: 'female'}).execute();");
    exec_and_out_equals("var result = collection.add({name: 'donna', age: 16, gender: 'female'}).execute();");
    exec_and_out_equals("var result = collection.add({name: 'angel', age: 14, gender: 'male'}).execute();");

    // Testing the find function
    {
      SCOPED_TRACE("Testing full find");
      exec_and_out_equals("var records = collection.find().execute().all();");
      exec_and_out_equals("print(records.length);", "7");
    }

    // Testing the find function with some criteria
    {
      SCOPED_TRACE("Testing full find");
      exec_and_out_equals("var records = collection.find('gender = \"male\"').execute().all();");
      exec_and_out_equals("print(records.length);", "4");

      exec_and_out_equals("var records = collection.find('gender = \"female\"').execute().all();");
      exec_and_out_equals("print(records.length);", "3");

      exec_and_out_equals("var records = collection.find('age = 13').execute().all();");
      exec_and_out_equals("print(records.length);", "1");

      exec_and_out_equals("var records = collection.find('age = 14').execute().all();");
      exec_and_out_equals("print(records.length);", "3");

      exec_and_out_equals("var records = collection.find('age < 17').execute().all();");
      exec_and_out_equals("print(records.length);", "6");

      exec_and_out_equals("var records = collection.find('name like \"a%\"').execute().all();");
      exec_and_out_equals("print(records.length);", "3");

      exec_and_out_equals("var records = collection.find('name like \"a%\" and age < 15').execute().all();");
      exec_and_out_equals("print(records.length);", "2");
    }

    {
      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).execute().all();");
      exec_and_out_equals("print(records.length);", "4");

      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).skip(1).execute().all();");
      exec_and_out_equals("print(records.length);", "4");

      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).skip(2).execute().all();");
      exec_and_out_equals("print(records.length);", "4");

      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).skip(3).execute().all();");
      exec_and_out_equals("print(records.length);", "4");

      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).skip(4).execute().all();");
      exec_and_out_equals("print(records.length);", "3");

      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).skip(5).execute().all();");
      exec_and_out_equals("print(records.length);", "2");

      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).skip(6).execute().all();");
      exec_and_out_equals("print(records.length);", "1");

      SCOPED_TRACE("Testing limit and offset");
      exec_and_out_equals("var records = collection.find().limit(4).skip(7).execute().all();");
      exec_and_out_equals("print(records.length);", "0");
    }
  }
}