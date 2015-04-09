#include "cloud-lm/config.hh"

#include <iostream>

namespace cloudlm {
namespace ngram {

Config *Config::s_instance = new Config();

Config::Config() :
		// TODO remove commented lines
  show_progress(true),
  messages(&std::cerr),
  //enumerate_vocab(NULL),
  unknown_missing(COMPLAIN),
  sentence_marker_missing(THROW_UP),
  positive_log_probability(THROW_UP),
  unknown_missing_logprob(-100.0),
  probing_multiplier(1.5),
  //building_memory(1073741824ULL), // 1 GB
  temporary_directory_prefix(NULL),
  //arpa_complain(ALL),
  //write_mmap(NULL),
  //write_method(WRITE_AFTER),
  //include_vocab(true),
  //rest_function(REST_MAX),
  //prob_bits(8),
  //backoff_bits(8),
  //pointer_bhiksha_bits(22),
  //load_method(util::POPULATE_OR_READ),
  max_order(0),
  verbose_level(0)
  {}

Config::~Config() {}

} // namespace ngram
} // namespace cloudlm
