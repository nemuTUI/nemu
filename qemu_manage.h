#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <fstream>

#include <boost/regex.hpp>

std::string get_param(const std::string &cfg, const std::string &regex);

#endif
