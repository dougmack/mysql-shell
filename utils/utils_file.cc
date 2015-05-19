﻿/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

#include "utils_file.h"

#include <stdexcept>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>

#ifdef WIN32
#  include <ShlObj.h>
#  include <comdef.h>
#else
#  include <sys/file.h>
#  include <errno.h>
#  include <cstring>
#  include <sys/types.h>
#  include <dirent.h>
#  include <sys/stat.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#else
#include <linux/limits.h>
#endif
#endif

using namespace shcore;

namespace shcore
{
  /*
   * Returns the config path ($HOME\mysqlx in Unix or %AppData%\mysql\mysql in Windows).
   */
  std::string get_user_config_path()
  {
#ifdef WIN32
    char szPath[MAX_PATH];
    HRESULT hr;

    if (SUCCEEDED(hr = SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, szPath)))
    {
      std::string path(szPath);
      path += "\\MySQL\\mysqlx\\";
      return path;
    }
    else
    {
      _com_error err(hr);
      throw std::runtime_error((boost::format("Error when gathering the APPDATA folder path: %s") % err.ErrorMessage()).str());
    }
#else
    char* cpath = std::getenv("HOME");

    if (cpath == NULL)
    {
      return "";
    }
    else
    {
      std::string path(cpath);
      path += "/.mysqlx/";
      return path;
    }
#endif
  }

  /*
  * Returns what should be considered the HOME folder for the shell.
  * If MYSQLX_HOME is defined, returns its value.
  * If not, it will try to identify the value based on the binary full path:
  * In a standard setup the binary will be at <MYSQLX_HOME>/bin
  * 
  * If that is the case MYSQLX_HOME is determined by trimming out
  * /bin/mysqlx from the full executable name.
  *
  * An empty value would indicate MYSQLX_HOME is unknown.
  */
  std::string get_mysqlx_home_path()
  {
    std::string ret_val;
    std::string exe_path;
    std::string path_separator;
    const char* env_home = getenv("MYSQLX_HOME");

    if (env_home)
      ret_val.assign(env_home);
    else
    {
#ifdef WIN32
      HMODULE hModule = GetModuleHandleA(NULL);
      char path[MAX_PATH];
      GetModuleFileNameA(hModule, path, MAX_PATH);
      exe_path.assign(path);
      path_separator = "\\";
#else
      path_separator = "/";
#ifdef __APPLE__
      char path[PATH_MAX];
      char real_path[PATH_MAX];
      uint32_t buffsize = sizeof(path);
      _NSGetExecutablePath(path, &buffsize);

      // _NSGetExecutablePath may return tricky constructs on paths
      // like symbolic links or things like i.e /path/to/./mysqlx
      // we need to normalize that
      realpath(path, real_path);
      exe_path.assign(real_path);
#else
      char path[PATH_MAX];
      readlink("/proc/self/exe", path, PATH_MAX);
      exe_path.assign(path);
#endif
#endif
      // If the exe path was found now we check if it can be considered the standard installation
      // by checking the parent folder is "bin"
      if (!exe_path.empty())
      {
        std::vector<std::string> tokens;
        boost::algorithm::split(tokens, exe_path, boost::is_any_of(path_separator), boost::token_compress_on);
        tokens.erase(tokens.end() - 1);

        if (tokens.at(tokens.size() - 1) == "bin")
        {
          // It seems to be a standard installation so re remove the bin folder
          // and the parent is MYSQLX_HOME!
          tokens.erase(tokens.end() - 1);
          ret_val = boost::algorithm::join(tokens, path_separator);
        }
      }
    }

    return ret_val;
  }

  /*
   * Returns whether if a file exists (true) or doesn't (false);
   */
  bool file_exists(const std::string& filename)
  {
#ifdef WIN32
    DWORD dwAttrib = GetFileAttributesA(filename.c_str());

    if (dwAttrib != INVALID_FILE_ATTRIBUTES &&
      !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
      return true;
    else
      return false;
#else
    if (access(filename.c_str(), F_OK) != -1)
      return true;
    else
      return false;
#endif
  }

  /*
   * Attempts to create directory if doesn't exists, otherwise just returns.
   * If there is an error, an exception is thrown.
   */
  void ensure_dir_exists(const std::string& path)
  {
    const char *dir_path = path.c_str();
#ifdef WIN32
    DWORD dwAttrib = GetFileAttributesA(dir_path);

    if (dwAttrib != INVALID_FILE_ATTRIBUTES)
      return;
    else if (!CreateDirectoryA(dir_path, NULL))
    {
      throw std::runtime_error((boost::format("Error when creating directory %s with error: %s") % dir_path % shcore::get_last_error()).str());
    }
#else
    DIR* dir = opendir(dir_path);
    if (dir)
    {
      /* Directory exists. */
      closedir(dir);
    }
    else if (ENOENT == errno)
    {
      /* Directory does not exist. */
      if (mkdir(dir_path, 0700) != 0)
        throw std::runtime_error((boost::format("Error when verifying dir %s exists: %s") % dir_path % shcore::get_last_error()).str());
    }
    else
    {
      throw std::runtime_error((boost::format("Error when verifying dir %s exists: %s") % dir_path % shcore::get_last_error()).str());
    }
#endif
  }

  /*
   * Returns the last system specific error description (using GetLastError in Windows or errno in Unix/OSX).
   */
  std::string get_last_error()
  {
#ifdef WIN32
    DWORD dwCode = GetLastError();
    LPTSTR lpMsgBuf;

    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      dwCode,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&lpMsgBuf,
      0, NULL);
    std::string msgerr = "SystemError: ";
    msgerr += lpMsgBuf;
    msgerr += "with error code %d.";
    boost::format fmt(msgerr);
    fmt % dwCode;
    return fmt.str();
#else
    char sys_err[64];
    int errnum = errno;

    strerror_r(errno, sys_err, sizeof(sys_err));
    std::string s = sys_err;
    s += "with errno %d.";
    boost::format fmt(s);
    fmt % errnum;
    return fmt.str();
#endif
  }

  bool load_text_file(const std::string& path, std::string& data)
  {
    bool ret_val = false;

    std::ifstream s(path.c_str());
    if (!s.fail())
    {
      s.seekg(0, std::ios_base::end);
      std::streamsize fsize = s.tellg();
      s.seekg(0, std::ios_base::beg);
      char *fdata = new char[fsize + 1];
      s.read(fdata, fsize);

      // Adds string terminator at the position next to the last
      // read character
      fdata[s.gcount()] = '\0';

      data.assign(fdata);
      ret_val = true;
    }

    return ret_val;
  }
}