## 
## Basic Call Testing
##
## @file CMakeLists.txt
## 
## -----------------------------------------------------------------------------
## Enduro/X Middleware Platform for Distributed Transaction Processing
## Copyright (C) 2015, ATR Baltic, SIA. All Rights Reserved.
## This software is released under one of the following licenses:
## GPL or ATR Baltic's license for commercial use.
## -----------------------------------------------------------------------------
## GPL license:
## 
## This program is free software; you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free Software
## Foundation; either version 2 of the License, or (at your option) any later
## version.
##
## This program is distributed in the hope that it will be useful, but WITHOUT ANY
## WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
## PARTICULAR PURPOSE. See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along with
## this program; if not, write to the Free Software Foundation, Inc., 59 Temple
## Place, Suite 330, Boston, MA 02111-1307 USA
##
## -----------------------------------------------------------------------------
## A commercial use license is available from ATR Baltic, SIA
## contact@atrbaltic.com
## -----------------------------------------------------------------------------
##
cmake_minimum_required(VERSION 2.8)

# Make sure the compiler can find include files from UBF library
include_directories (${ENDUROX_SOURCE_DIR}/libubf
					 ${ENDUROX_SOURCE_DIR}/include
					 ${ENDUROX_SOURCE_DIR}/libnstd
					 ${ENDUROX_SOURCE_DIR}/ubftest)


# Add debug options
# By default if RELEASE_BUILD is not defined, then we run in debug!
IF ($ENV{RELEASE_BUILD})
	# do nothing
ELSE ($ENV{RELEASE_BUILD})
	ADD_DEFINITIONS("-D NDRX_DEBUG")
ENDIF ($ENV{RELEASE_BUILD})

# Make sure the linker can find the UBF library once it is built.
link_directories (${ENDUROX_BINARY_DIR}/libubf) 

############################# Test - executables ###############################
add_executable (atmi.sv21 atmisv21.c ../../libatmisrv/rawmain_integra.c)
add_executable (atmiclt21 atmiclt21.c)

# Conversations
add_executable (convsv21 convsv21.c ../../libatmisrv/rawmain_integra.c)
add_executable (convclt21 convclt21.c)

# shell testing
add_executable (atmiclt21-cli atmiclt21-cli.c)
################################################################################
############################# Test - executables ###############################
# Link the executable to the ATMI library & others...
target_link_libraries (atmi.sv21 m ubf nstd atmi atmisrvinteg ${RT_LIB} xadrv pthread)
target_link_libraries (atmiclt21 m ubf nstd atmi atmiclt ${RT_LIB} xadrv pthread)

# Conversations test
target_link_libraries (convsv21 m ubf nstd atmi atmisrvinteg ${RT_LIB} xadrv pthread)
target_link_libraries (convclt21 m ubf nstd atmi atmiclt ${RT_LIB} xadrv pthread)

# shell testing
target_link_libraries (atmiclt21-cli m ubf nstd atmi atmiclt ${RT_LIB} xadrv pthread)

set_target_properties(atmi.sv21 PROPERTIES LINK_FLAGS "$ENV{MYLDFLAGS}")
set_target_properties(atmiclt21 PROPERTIES LINK_FLAGS "$ENV{MYLDFLAGS}")

# Conversation test
set_target_properties(convsv21 PROPERTIES LINK_FLAGS "$ENV{MYLDFLAGS}")
set_target_properties(convclt21 PROPERTIES LINK_FLAGS "$ENV{MYLDFLAGS}")

# shell testing
set_target_properties(atmiclt21-cli PROPERTIES LINK_FLAGS "$ENV{MYLDFLAGS}")


add_library (xadrv SHARED
                xabackend.c
            )

add_library (xadrv_s SHARED
                xabackend_s.c
                xabackend_common.c
            )

add_library (xadrv_d SHARED
                xabackend_d.c
                xabackend_common.c
            )


if(${CMAKE_OS_NAME} STREQUAL "CYGWIN")
    target_link_libraries(xadrv atmi ubf nstd)
    target_link_libraries(xadrv_s atmi ubf nstd)
    target_link_libraries(xadrv_d atmi ubf nstd)
elseif(${CMAKE_OS_NAME} STREQUAL "DARWIN")
    target_link_libraries(xadrv atmi ubf nstd)
    target_link_libraries(xadrv_s atmi ubf nstd)
    target_link_libraries(xadrv_d atmi ubf nstd)
endif()
################################################################################
