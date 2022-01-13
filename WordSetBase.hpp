#include <string>

#include "emp/bits/BitVector.hpp"

/// An abstract base class for all WordSets of different sizes.
class WordSetBase {
public:
  using word_list_t = emp::BitVector;

  virtual ~WordSetBase() { }

  virtual void AddWord(const std::string & in_word) = 0;
  virtual void Load(const emp::vector<std::string> & in_words) = 0;
  virtual void Load(const std::string & filename) = 0;
  virtual void ResetOptions() = 0;
  virtual void CalculateNextWords(size_t word_id) = 0;
  virtual void AnalyzeGuess(size_t word_id, const word_list_t & cur_words) = 0;
  virtual bool Preprocess() = 0;
};