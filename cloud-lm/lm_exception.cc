#include "cloud-lm/lm_exception.hh"

#include<errno.h>
#include<stdio.h>

namespace cloudlm {

ConfigException::ConfigException() throw() {}
ConfigException::~ConfigException() throw() {}

LoadException::LoadException() throw() {}
LoadException::~LoadException() throw() {}

FormatLoadException::FormatLoadException() throw() {}
FormatLoadException::~FormatLoadException() throw() {}

VocabLoadException::VocabLoadException() throw() {}
VocabLoadException::~VocabLoadException() throw() {}

SpecialWordMissingException::SpecialWordMissingException() throw() {}
SpecialWordMissingException::~SpecialWordMissingException() throw() {}

SolrException::SolrException() throw() {}
SolrException::~SolrException() throw() {}

} // namespace cloudlm
