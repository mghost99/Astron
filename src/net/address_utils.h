#pragma once

#include <string>
#include <vector>

#include "NetTypes.h"

bool is_valid_address(const std::string &hostspec);

std::vector<NetAddress> resolve_address(const std::string &hostspec, uint16_t port);
