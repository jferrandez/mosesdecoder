// Separate header because this is used often.
#ifndef CLOUDLM_WORD_INDEX_H
#define CLOUDLM_WORD_INDEX_H

#include <limits.h>
#include <boost/unordered_map.hpp>

namespace cloudlm {
/*struct WordIndex {
	unsigned int index;
	std::string word;

	WordIndex(unsigned int i, std::string w) {
		index = i;
		word = w;
	}
};*/
typedef unsigned int WordIndex;
const WordIndex kMaxWordIndex = UINT_MAX;

typedef boost::unordered_map< WordIndex, std::string > WordIndexToString;
} // namespace lm

#endif
