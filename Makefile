#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#***********************DIRS +=

TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure
DIRS += include
DIRS += dbd
DIRS += db
DIRS += src
DIRS += iocBoot

include $(TOP)/configure/RULES_TOP
