#ifndef CLOUDLM_MODEL_TYPE_H
#define CLOUDLM_MODEL_TYPE_H

namespace cloudlm {
namespace ngram {

/* Not the best numbering system, but it grew this way for historical reasons
 * and I want to preserve existing binary files. */
typedef enum {CLOUD_PROBING=0} ModelType;

} // namespace ngram
} // namespace cloudlm
#endif // CLOUDLM_MODEL_TYPE_H
