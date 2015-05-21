// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.
#define BOOST_PYTHON_STATIC_LIB 

#include <exception>

#include "gattlib.h"
#include "gattservices.h"

DiscoveryService::DiscoveryService(const std::string device = "hci0") {
    throw std::runtime_error("Not implemented!");
}

DiscoveryService::~DiscoveryService() {}

boost::python::dict
DiscoveryService::discover(int timeout) {
    throw std::runtime_error("Not implemented!");
}
