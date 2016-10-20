/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "interactive_dba_cluster.h"
#include "interactive_global_dba.h"
#include "modules/adminapi/mod_dba_cluster.h"
//#include "modules/adminapi/mod_dba_instance.h"
#include "shellcore/shell_registry.h"
#include "modules/mysqlxtest_utils.h"
#include "utils/utils_general.h"
#include "modules/adminapi/mod_dba_common.h"
#include <boost/format.hpp>
#include <string>

using namespace std::placeholders;
using namespace shcore;

void Interactive_dba_cluster::init() {
  add_method("addInstance", std::bind(&Interactive_dba_cluster::add_instance, this, _1), "data");
  add_method("rejoinInstance", std::bind(&Interactive_dba_cluster::rejoin_instance, this, _1), "data");
  add_method("removeInstance", std::bind(&Interactive_dba_cluster::remove_instance, this, _1), "data");
  add_varargs_method("dissolve", std::bind(&Interactive_dba_cluster::dissolve, this, _1));
}

shcore::Value Interactive_dba_cluster::add_seed_instance(const shcore::Argument_list &args) {
  shcore::Value ret_val;
  std::string function;

  std::shared_ptr<mysh::dba::ReplicaSet> object;
  auto cluster = std::dynamic_pointer_cast<mysh::dba::Cluster> (_target);

  if (cluster)
    object = cluster->get_default_replicaset();

  if (object) {
    std::string answer;

    if (prompt("The default ReplicaSet is already initialized. Do you want to add a new instance? [Y|n]: ", answer)) {
      if (!answer.compare("y") || !answer.compare("Y") || answer.empty())
        function = "addInstance";
    }
  } else
    function = "addSeedInstance";

  if (!function.empty()) {
    auto options = mysh::dba::get_instance_options_map(args, false);
    mysh::dba::resolve_instance_credentials(options, _delegate);

    shcore::Argument_list new_args;
    new_args.push_back(shcore::Value(options));
    ret_val = call_target(function, new_args);
  }

  return ret_val;
}

shcore::Value Interactive_dba_cluster::add_instance(const shcore::Argument_list &args) {
  shcore::Value ret_val;
  std::string function;

  args.ensure_count(1, 2, get_function_name("addInstance").c_str());

  shcore::Value::Map_type_ref options;

  try {
    std::shared_ptr<mysh::dba::ReplicaSet> object;
    auto cluster = std::dynamic_pointer_cast<mysh::dba::Cluster> (_target);

    if (cluster)
      object = cluster->get_default_replicaset();

    if (!object) {
      std::string answer;

      if (prompt("The default ReplicaSet is not initialized. Do you want to initialize it adding a seed instance? [Y|n]: ", answer)) {
        if (!answer.compare("y") || !answer.compare("Y") || answer.empty())
          function = "addSeedInstance";
      }
    } else
      function = "addInstance";

    if (!function.empty()) {
      std::string message = "A new instance will be added to the InnoDB cluster. Depending on the amount of\n"
                            "data on the cluster this might take from a few seconds to several hours.\n\n";

      print(message);

      options = mysh::dba::get_instance_options_map(args, false);

      shcore::Argument_map opt_map(*options);
      opt_map.ensure_keys({"host"}, {"name", "host", "port", "user", "dbUser", "password", "dbPassword", "socket", "ssl_ca", "ssl_cert", "ssl_key", "ssl_key"}, "instance definition");

      mysh::dba::resolve_instance_credentials(options, _delegate);
    }
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("addInstance"));

  if (options) {
    shcore::Argument_list new_args;
    new_args.push_back(shcore::Value(options));

    println("Adding instance to the cluster ...");
    println();
    ret_val = call_target(function, new_args);


    println("The instance '" + build_connection_string(options, false) + "' was successfully added to the cluster.");
    println();
  }

  return ret_val;
}

shcore::Value Interactive_dba_cluster::rejoin_instance(const shcore::Argument_list &args) {
  shcore::Value ret_val;

  shcore::Value::Map_type_ref options;

  std::string message = "The instance will try rejoining the InnoDB cluster. Depending on the original\n"
                        "problem that made the instance unavailable the rejoin, operation might not be\n"
                        "successful and further manual steps will be needed to fix the underlying\n"
                        "problem.\n"
                        "\n"
                        "Please monitor the output of the rejoin operation and take necessary action if\n"
                        "the instance cannot rejoin.\n";

  std::string answer;

  if (password("Please provide the password for '" + args.string_at(0) + "': ", answer)) {
    shcore::Argument_list new_args;
    new_args.push_back(args[0]);
    new_args.push_back(shcore::Value(answer));
    print(message);
    ret_val = call_target("rejoinInstance", new_args);

    println("The instance '" + build_connection_string(options, false) + "' was successfully rejoined on the cluster.");
    println();
  }

  return ret_val;
}

shcore::Value Interactive_dba_cluster::remove_instance(const shcore::Argument_list &args) {
  shcore::Value ret_val;
  std::string uri;
  shcore::Value::Map_type_ref options; // Map with the connection data

  args.ensure_count(1, get_function_name("removeInstance").c_str());

  std::string message = "The instance will be removed from the InnoDB cluster. Depending on the \n"
                        "instance being the Seed or not, the Metadata session might become invalid. \n"
                        "If so, please start a new session to the Metadata Storage R/W instance.\n\n";

  print(message);

  std::string name;

  //auto instance = args.object_at<mysh::dba::Instance> (0);

  // Identify the type of connection data (String or Document)
  if (args[0].type == String) {
    uri = args.string_at(0);
    options = get_connection_data(uri, false);
  }

  // TODO: what if args[0] is a String containing the name of the instance?

  // Connection data comes in a dictionary
  else if (args[0].type == Map)
    options = args.map_at(0);

  else
    throw shcore::Exception::argument_error("Invalid connection options, expected either a URI or a Dictionary");

  //if (instance)
  //  name = instance->get_name();
  //else
  name = build_connection_string(options, false);

  ret_val = call_target("removeInstance", args);

  println("The instance '" + name + "' was successfully removed from the cluster.");
  println();

  return ret_val;
}

shcore::Value Interactive_dba_cluster::dissolve(const shcore::Argument_list &args) {
  shcore::Value ret_val;
  bool force = false;
  shcore::Value::Map_type_ref options;

  args.ensure_count(0, 1, get_function_name("dissolve").c_str());

  try {
    if (args.size() == 1)
      options = args.map_at(0);

    if (options) {
      shcore::Argument_map opt_map(*options);

      opt_map.ensure_keys({}, {"force"}, "dissolve options");

      if (opt_map.has_key("force"))
        force = opt_map.bool_at("force");
    }
  }

  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("dissolve"));

  if (!force) {
    std::shared_ptr<mysh::dba::ReplicaSet> object;
    auto cluster = std::dynamic_pointer_cast<mysh::dba::Cluster> (_target);

    if (cluster)
      object = cluster->get_default_replicaset();

    if (object) {
      println("The cluster still has active ReplicaSets.");
      println("Please use cluster.dissolve({force: true}) to deactivate replication");
      println("and unregister the ReplicaSets from the cluster.");
      println();

      println("The following replicasets are currently registered:");

      ret_val = call_target("describe", shcore::Argument_list());
    }
  } else {
    ret_val = call_target("dissolve", args);

    println("The cluster was successfully dissolved.");
    println("Replication was disabled but user data was left intact.");
    println();
  }

  return ret_val;
}

