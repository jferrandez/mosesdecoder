#ifndef CLOUDLM_LM_EXCEPTION_H
#define CLOUDLM_LM_EXCEPTION_H

// Named to avoid conflict with util/exception.hh.  

#include "util/exception.hh"
#include "util/string_piece.hh"

#include <exception>
#include <string>

namespace cloudlm {

typedef enum {THROW_UP, COMPLAIN, SILENT} WarningAction;

class ConfigException : public util::Exception {
  public:
    ConfigException() throw();
    ~ConfigException() throw();
};

class LoadException : public util::Exception {
   public:
      virtual ~LoadException() throw();

   protected:
      LoadException() throw();
};

class FormatLoadException : public LoadException {
  public:
    FormatLoadException() throw();
    ~FormatLoadException() throw();
};

class VocabLoadException : public LoadException {
  public:
    virtual ~VocabLoadException() throw();
    VocabLoadException() throw();
};

class SpecialWordMissingException : public VocabLoadException {
  public:
    explicit SpecialWordMissingException() throw();
    ~SpecialWordMissingException() throw();
};

} // namespace cloudlm

#endif // CLOUDLM_LM_EXCEPTION_H
