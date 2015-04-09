#ifndef CLOUDLM_UTIL__
#define CLOUDLM_UTIL__

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <limits>
#include <map>
#include <cstdlib>
#include <cstring>
#include "cloud-lm/config.hh"
#include "util/exception.hh"

namespace cloudlm {

#ifdef TRACE_ENABLE
#define TRACE_ERR(str) do { std::cerr << str; } while (false)
#else
#define TRACE_ERR(str) do {} while (false)
#endif

/** verbose macros
 * */

#define VERBOSE(level,str) { if (cloudlm::ngram::Config::Instance()->verbose_level >= level) { TRACE_ERR(str); } }
#define IFVERBOSE(level) if (cloudlm::ngram::Config::Instance()->verbose_level >= level)

} // namespace

#endif
