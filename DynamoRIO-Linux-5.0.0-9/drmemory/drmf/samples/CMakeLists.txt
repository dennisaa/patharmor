# **********************************************************
# Copyright (c) 2014 Google, Inc.  All rights reserved.
# **********************************************************

# Dr. Memory: the memory debugger
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License, and no later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

cmake_minimum_required(VERSION 2.8)

# Collapse VS generator into a single config, to match Makefiles.
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)
  else ()
    set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo" CACHE STRING "" FORCE)
  endif ()
  # We want to use the _LOCATION_<config> property
  string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" upper)
  set(location_suffix "_${upper}")
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(location_suffix "")
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

project(DRMF_samples)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # We collapse the Debug and Release subdirs to match Makefile layout:
  foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  endforeach ()
endif ()

# If DR package is already set up, the "dynamorio" target should exist.
if (NOT TARGET dynamorio)
  if (NOT DEFINED DynamoRIO_DIR)
    # Try the location inside a DR+DRMF combined package.
    set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../../../cmake" CACHE PATH
      "DynamoRIO installation's cmake directory")
  endif ()
  find_package(DynamoRIO)
  if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "DynamoRIO package required to build")
  endif ()
endif ()

# If DRMF package is already set up, the "drsyscall" target should exist.
if (NOT TARGET drsyscall)
  # For a combined DR+DRMF package we don't need this step.
  # For a separate DRMF package we do.
  if (NOT DEFINED DrMemoryFramework_DIR)
    set(DrMemoryFramework_DIR "${PROJECT_SOURCE_DIR}/.." CACHE PATH
      "Dr. Memory Framework cmake directory")
  endif ()
  find_package(DrMemoryFramework)
  if (NOT DrMemoryFramework_FOUND)
    message(FATAL_ERROR "DrMemoryFramework package required to build")
  endif ()
endif ()

# Build the samples themselves:

add_library(strace SHARED strace.c)
configure_DynamoRIO_client(strace)
use_DynamoRIO_extension(strace drmgr)
use_DynamoRIO_extension(strace drsyscall)

###########################################################################
