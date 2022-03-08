#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "emp/base/Ptr.hpp"
#include "emp/base/vector.hpp"
#include "emp/bits/BitSet.hpp"
#include "emp/bits/BitVector.hpp"
#include "emp/datastructs/map_utils.hpp"
#include "emp/datastructs/vector_utils.hpp"
#include "emp/tools/string_utils.hpp"
#include "emp/debug/alert.hpp"

#include "Result.hpp"

class IDSet {
  friend class IDGroup;
private:
  emp::vector<uint16_t> ids;
  bool sorted = true;

public:
  IDSet() { }
  IDSet(size_t count) { SetAll(count); }
  IDSet(const emp::vector<uint16_t> & _ids, size_t start_id, size_t end_id, bool _sorted=false)
    : ids(_ids.begin()+start_id, _ids.begin()+end_id), sorted(_sorted)
    { }
  IDSet(const IDSet &) = default;
  IDSet(IDSet &&) = default;

  IDSet & operator=(const IDSet &) = default;
  IDSet & operator=(IDSet &&) = default;

  auto begin() const { return ids.begin(); }
  auto end() const { return ids.end(); }
  auto begin() { return ids.begin(); }
  auto end() { return ids.end(); }

  size_t size() const { return ids.size(); }
  void Sort() {
    if (!sorted) {     // Only sort if needed.
      emp::Sort(ids);  // Always sort in ID order.
      sorted = true;   // Mark as now sorted.
    }
  }
  template <typename T>
  void Sort(T fun_less) {
    std::sort(ids.begin(), ids.end(), fun_less);
  }

  uint16_t operator[](size_t pos) const { return ids[pos]; }

  void Clear() { ids.resize(0); sorted=true; }

  void Add(uint16_t id) {
    if (ids.size() && ids.back() >= id) sorted = false;
    ids.push_back(id);
  }

  void SetAll(size_t count) {
    ids.resize(count);
    for (uint16_t i = 0; i < count; ++i) ids[i] = i;
    sorted = true;
  }

  // Intersection
  IDSet & operator&=(const IDSet & in) {
    // Make sure both sets are sorted.
    emp_assert(in.sorted == true);
    Sort();

    size_t this_next = 0;
    size_t in_next = 0;
    size_t keep_count = 0;

    // While there is more to compare...
    while (this_next < ids.size() && in_next < in.size()) {
      // If this a match?  Keep it!
      if (ids[this_next] == in[in_next]) {
        ids[keep_count] =ids[this_next];
        ++keep_count; ++this_next; ++in_next;
      }
      else if (ids[this_next] < in[in_next]) ++this_next;
      else ++in_next;
    }

    ids.resize(keep_count);

    return *this;
  }

  // Removal
  IDSet & operator-=(IDSet & in) {
    // Make sure both sets are sorted.
    Sort();
    in.Sort();

    size_t this_next = 0;
    size_t in_next = 0;
    size_t keep_count = 0;

    // While there is more to compare...
    while (this_next < ids.size() && in_next < in.size()) {
      // If we've moved past this word in the exclusion list, keep it!
      if (ids[this_next] < in[in_next]) {
        ids[keep_count] = ids[this_next];
        ++keep_count; ++this_next;
      }
      else if (ids[this_next] == in[in_next]) { ++this_next; ++in_next; }
      else ++in_next;
    }

    // Any leftovers in the main list should be kept, remove all else.
    if (keep_count != this_next) {
      ids.erase(ids.begin()+keep_count, ids.begin()+this_next);
    }

    return *this;
  }
};

struct IDGroups {
  emp::vector<uint16_t> ids;
  emp::vector<uint16_t> starts;

  IDGroups() { }
  IDGroups(const IDGroups &) = default;
  IDGroups(IDGroups &&) = default;

  IDGroups & operator=(const IDGroups &) = default;
  IDGroups & operator=(IDGroups &&) = default;

  void AddGroup(const IDSet & new_group) {
    starts.push_back(ids.size());
    ids.insert(ids.end(), new_group.begin(), new_group.end());
  }

  IDSet GetGroup(size_t group_id) const {
    auto start_id = starts[group_id];
    auto end_id = (group_id+1 < starts.size()) ? starts[group_id+1] : ids.size();
    return IDSet(ids, start_id, end_id, true);
  }
};

class WordleEngine {
private:
  static constexpr size_t MAX_LETTER_REPEAT = 4;

  // Get the ID (0-25) associated with a letter.
  static size_t ToID(char letter) {
    if (letter >= 'a' && letter <= 'z') return static_cast<size_t>(letter - 'a');
    if (letter >= 'A' && letter <= 'Z') return static_cast<size_t>(letter - 'A');
    emp_error("Character is not a letter: ", letter);
    return 26;
  }

  static char ToLetter(size_t id) {
    emp_assert(id < 26, id);
    return static_cast<char>(id + 'a');
  }

  Result ToResult(size_t id) const { return Result(word_size, id); }

  // All of the clues for a given position.
  struct PositionClues {
    size_t pos;
    std::array<IDSet, 26> here;      // Is a given letter at this position?
  };

  // All of the clues for zero or more instances of a given letter.
  struct LetterClues {
    size_t letter;  // [0-25]
    std::array<IDSet, MAX_LETTER_REPEAT+1> at_least; ///< Are there at least x instances of letter? (0 is meaningless)
    std::array<IDSet, MAX_LETTER_REPEAT+1> exactly;  ///< Are there exactly x instances of letter?
  };

  struct WordStats {
    std::string word;
    size_t max_options = 0;     // Maximum number of word options after used as a guess.
    double ave_options = 0.0;   // Average number of options after used as a guess.
    double entropy = 0.0;       // What is the entropy (and thus information gained) for this choice?
    Result max_result;          // Which result gives us the max options?

    WordStats() { }
    WordStats(const std::string & in_word) : word(in_word) { }
    WordStats(const WordStats &) = default;
    WordStats(WordStats &&) = default;

    WordStats & operator=(const WordStats &) = default;
    WordStats & operator=(WordStats &&) = default;
  };

  struct WordData : public WordStats {
    // Pre=processed data
    emp::array<uint8_t, 26> let_count;

    using WordStats::word;
    IDGroups next_words;   // What words will each clue lead to?

    WordData(const std::string & in_word) : WordStats(in_word) {
      for (auto & x : let_count) x = 0;
      for (char x : word) {
        size_t let_id = ToID(x);
        ++let_count[let_id];
      }
    }

  };

  //////////////////////////////////////////////////////////////////////////////////////////
  //  DATA MEMBERS for WorldEngine
  //////////////////////////////////////////////////////////////////////////////////////////

  size_t word_size;
  size_t num_ids;                                  ///< Number of possible result IDs

  emp::vector<WordData> words;                     ///< Data about all words in this Wordle
  emp::vector<PositionClues> pos_clues;            ///< A PositionClues object for each position.
  emp::array<LetterClues,26> let_clues;            ///< Clues based off the number of letters.
  std::unordered_map<std::string, size_t> id_map; ///< Map of words to their position ids.
  IDSet cur_options;                               ///< Current word options.

public:
  WordleEngine(size_t _word_size)
  : word_size(_word_size)
  , num_ids(Result::CalcNumIDs(_word_size))
  , pos_clues(word_size)
  { }

  WordleEngine(const emp::vector<std::string> & in_words, size_t _word_size)
  : WordleEngine(_word_size) { Load(in_words); }

  size_t GetSize() const { return words.size(); }
  size_t GetNumResults() const { return num_ids; }
  const IDSet & GetOptions() const { return cur_options; }
  IDSet GetAllOptions() const { return IDSet(words.size()); }
  void ResetOptions() { cur_options.SetAll(words.size()); }


  void SetWordSize(size_t in_size) {
    word_size = in_size;
    num_ids = Result::CalcNumIDs(word_size);
    pos_clues.resize(word_size);
    words.clear();
    cur_options.Clear();
  }

  void AddClue(const std::string & word, Result result) {
    size_t word_id = id_map[word];
    const auto & word_data = words[word_id];
    cur_options &= word_data.next_words.GetGroup(result.GetID());
  }

  bool SortWords(IDSet & ids, const std::string & sort_type="alpha") const {
    if (sort_type == "max") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        if (w1.max_options == w2.max_options) return w1.ave_options < w2.ave_options; // tiebreak
        return w1.max_options < w2.max_options;
      } );
    } else if (sort_type == "ave") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        if (w1.ave_options == w2.ave_options) return w1.max_options < w2.max_options; // tiebreak
        return w1.ave_options < w2.ave_options;
      } );
    } else if (sort_type == "info") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        return w1.entropy > w2.entropy;
      } );
    } else if (sort_type == "alpha") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        return w1.word < w2.word;
      } );
    } else if (sort_type == "r-max") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        if (w1.max_options == w2.max_options) return w1.ave_options > w2.ave_options; // tiebreak
        return w1.max_options > w2.max_options;
      } );
    } else if (sort_type == "r-ave") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        if (w1.ave_options == w2.ave_options) return w1.max_options > w2.max_options; // tiebreak
        return w1.ave_options > w2.ave_options;
      } );
    } else if (sort_type == "r-info") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        return w1.entropy < w2.entropy;
      } );
    } else if (sort_type == "r-alpha") {
      ids.Sort( [this](uint16_t id1, uint16_t id2) {
        const WordStats & w1 = words[id1];
        const WordStats & w2 = words[id2];
        return w1.word > w2.word;
      } );
    }
    else return false;
    return true;
  }

  void WriteWords(
    const IDSet & ids,
    std::size_t max_count,
    std::ostream & os=std::cout,
    bool extra_data=true,
    std::string col_break = ", ",
    std::string line_break = "\n"
  ) const {
    for (size_t i = 0; i < max_count && i < ids.size(); ++i) {
      size_t id = ids[i];
      os << words[id].word;
      if (extra_data) {
        os << col_break << words[id].ave_options
          << col_break << words[id].max_options
          << col_break << words[id].entropy;
      }
      os << line_break;
    }
    if (max_count < ids.size()) {
      os << "...plus " << (ids.size() - max_count) << " more." << line_break;
    }
    os.flush();
  }


  /// Load a whole series for words (from a file) into this WordleEngine
  void Load(const emp::vector<std::string> & in_words) {
    // Load in all of the words.
    size_t wrong_size_count = 0;
    size_t invalid_char_count = 0;
    size_t dup_count = 0;
    for (const std::string & in_word : in_words) {
      // Only keep words of the correct size and all lowercase.
      if (in_word.size() != word_size) { wrong_size_count++; continue; }
      if (!emp::is_lower(in_word)) { invalid_char_count++; continue; }
      if (emp::Has(id_map, in_word)) { dup_count++; continue; }
      size_t id = words.size();      // Set a unique ID for this word.
      id_map[in_word] = id;         // Keep track of the ID for this word.
      words.emplace_back(in_word);   // Setup the word data.
    }

    cur_options.SetAll(words.size());

    if (wrong_size_count) {
      std::cerr << "Warning: eliminated " << wrong_size_count << " words of the wrong size."
                << std::endl;
    }
    if (invalid_char_count) {
      std::cerr << "Warning: eliminated " << invalid_char_count << " words with invalid characters."
                << std::endl;
    }
    if (dup_count) {
      std::cerr << "Warning: eliminated " << dup_count << " words that were duplicates."
                << std::endl;
    }

    std::cout << "Loaded " << words.size() << " valid words." << std::endl;
  }

  /// Load in words from a file.
  void Load(const std::string & filename) {
    std::ifstream is(filename);
    emp::vector<std::string> in_words;
    std::string new_word;
    while (is) {
      is >> new_word;
      in_words.push_back(new_word);
    }
    Load(in_words);
  }

  // Identify which words satisfy each clue.
  bool PreprocessClues() {
    for (size_t i=0; i < word_size; ++i) { pos_clues[i].pos = i; }
    for (size_t let=0; let < 26; let++) { let_clues[let].letter = let; }

    // Counters for number of letters.
    emp::array<uint8_t, 26> letter_counts;

    // Loop through each word, indicating which clues it is consistent with.
    for (size_t word_id = 0; word_id < words.size(); ++word_id) {
      const std::string & word = words[word_id].word;

      // Figure out which letters are in this word.
      std::fill(letter_counts.begin(), letter_counts.end(), 0);      // Reset counters to zero.
      for (const char letter : word) ++letter_counts[ToID(letter)];  // Count letters.

      // Setup the LETTER clues that word is consistent with.
      for (size_t letter_id = 0; letter_id < 26; ++letter_id) { 
        const size_t cur_count = letter_counts[letter_id];       
        let_clues[letter_id].exactly[cur_count].Add(word_id);
        for (uint8_t count = 0; count <= cur_count; ++count) {
          let_clues[letter_id].at_least[count].Add(word_id);
        }
      }

      // Now figure out what POSITION clues it is consistent with.
      for (size_t pos=0; pos < word.size(); ++pos) {
        const size_t cur_letter = ToID(word[pos]);
        pos_clues[pos].here[cur_letter].Add(word_id);
      }
    }

    return true;
  }
  
  
  /// Given a guess and a result, determine available options.
  // @CAO: OPTIMIZATION NOTES:
  //       - Make words options static to prevent memory allocation.
  //       - Figure out all incluse/exclude ID sets and run through them from smallest.
  //       - Speed up intersection and subtraction when Set sizes are very different.
  IDSet PreprocessResultGroup(const std::string & guess, const Result & result) {
    emp::array<uint8_t, 26> letter_counts;
    std::fill(letter_counts.begin(), letter_counts.end(), 0);
    emp::BitSet<26> letter_fail;
    IDSet word_options(words.size());  // Start with all options available.

    // First add HERE clues and collect letter counts.
    for (size_t i = 0; i < word_size; ++i) {
      const uint16_t cur_letter = ToID(guess[i]);
      if (result[i] == Result::HERE) {
        word_options &= pos_clues[i].here[cur_letter];
        ++letter_counts[cur_letter];
      } else if (result[i] == Result::ELSEWHERE) {
        word_options -= pos_clues[i].here[cur_letter];
        ++letter_counts[cur_letter];
      } else {  // Must be 'N'
        word_options -= pos_clues[i].here[cur_letter];
        letter_fail.Set(cur_letter);
      }
    }

    // Next add letter frequency clues.
    for (size_t letter_id = 0; letter_id < 26; ++letter_id) { 
      const size_t let_count = letter_counts[letter_id];
      // If a letter failed, we know exactly how many there are.
      if (letter_fail.Has(letter_id)) {
        word_options &= let_clues[letter_id].exactly[let_count];
      }

      // Otherwise we know it's at least the number that we tested.
      else if (let_count) {
        word_options &= let_clues[letter_id].at_least[let_count];
      }
    }

    return word_options;
  }
  
  /// Analyze the current set of words and store result groups for each one.
  void PreprocessWords() {
    for (size_t id = 0; id < words.size(); ++id) {
      WordData & word_data = words[id];

      IDSet result_words;

      // Step through each possible result and determine what words that would leave.
      for (size_t result_id = 0; result_id < num_ids; ++result_id) {
        result_words.Clear();
        Result result(word_size, result_id);
        if (result.IsValid(word_data.word)) {
          result_words = PreprocessResultGroup(word_data.word, ToResult(result_id));
        }
        word_data.next_words.AddGroup(result_words);
      }
    }
  }

  /// Handle all of the needed preprocessing.
  void Preprocess() {
    PreprocessClues();
    PreprocessWords();
  }


  void PrintPosClues(size_t pos) const {
    const PositionClues & clue = pos_clues[pos];
    std::cout << "Position " << pos << ":\n";
    for (uint8_t i = 0; i < 26; ++i) {
      std::cout << " '" << ToLetter(i) << "' : ";
      WriteWords(clue.here[i], 10, std::cout, false, "", " ");
      std::cout << std::endl;
    }
  }

  void PrintLetterClues(char letter) const {
    const LetterClues & clue = let_clues[ToID(letter)];
    std::cout << "Letter '" << clue.letter << "':\n";
    for (size_t i = 0; i <= MAX_LETTER_REPEAT; ++i) {
      std::cout << "EXACTLY " << i << ":  ";
      WriteWords(clue.exactly[i], 20, std::cout, false, "", " ");
      std::cout << std::endl;
    }
    for (size_t i = 0; i <= MAX_LETTER_REPEAT; ++i) {
      std::cout << "AT LEAST " << i << ": ";
      WriteWords(clue.at_least[i], 20, std::cout, false, "", " ");
      std::cout << std::endl;
    }
  }


};
