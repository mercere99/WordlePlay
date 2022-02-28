#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "emp/base/Ptr.hpp"
#include "emp/base/vector.hpp"
#include "emp/bits/BitSet.hpp"
#include "emp/bits/BitVector.hpp"
//#include "emp/config/command_line.hpp"
#include "emp/datastructs/map_utils.hpp"
#include "emp/datastructs/vector_utils.hpp"
//#include "emp/io/File.hpp"
#include "emp/tools/string_utils.hpp"
#include "emp/debug/alert.hpp"

#include "Result.hpp"
#include "WordleEngineBase.hpp"

#define STORE_NEXT_WORDS

class WordleEngine : public WordleEngineBase {
private:
  static constexpr size_t MAX_LETTER_REPEAT = 4;
  using word_list_t = emp::BitVector;

#ifdef STORE_NEXT_WORDS
  using next_words_t = std::vector<word_list_t>;
#endif

  // Get the ID (0-26) associated with a letter.
  static size_t ToID(char letter) {
    emp_assert(letter >= 'a' && letter <= 'z');
    return static_cast<size_t>(letter - 'a');
  }

  static char ToLetter(size_t id) {
    emp_assert(id < 26);
    return static_cast<char>(id + 'a');
  }

  Result ToResult(size_t id) const { return Result(word_size, id); }

  // All of the clues for a given position.
  struct PositionClues {
    size_t pos;
    std::array<word_list_t, 26> here;      // Is a given letter at this position?

    void SetNumWords(size_t num_words) {
      for (auto & x : here) { x.resize(num_words); x.Clear(); }
    }
  };

  // All of the clues for zero or more instances of a given letter.
  struct LetterClues {
    size_t letter;  // [0-25]
    std::array<word_list_t, MAX_LETTER_REPEAT+1> at_least; ///< Are there at least x instances of letter? (0 is meaningless)
    std::array<word_list_t, MAX_LETTER_REPEAT+1> exactly;  ///< Are there exactly x instances of letter?

    void SetNumWords(size_t num_words) {
      for (auto & x : at_least) { x.resize(num_words); x.Clear(); }
      for (auto & x : exactly) { x.resize(num_words); x.Clear(); }
    }
  };

  struct WordStats {
    std::string word;
    size_t max_options = 0;     // Maximum number of word options after used as a guess.
    double ave_options = 0.0;   // Average number of options after used as a guess.
    double entropy = 0.0;       // What is the entropy (and thus information gained) for this choice?
    Result max_result;          // Which result gives us the max options?

    WordStats() : word() { }
    WordStats(const std::string & in_word) : word(in_word) { }
    WordStats(const WordStats &) = default;
    WordStats(WordStats &&) = default;

    WordStats & operator=(const WordStats &) = default;
    WordStats & operator=(WordStats &&) = default;
  };

  struct WordData : public WordStats {
    // Pre=processed data
    emp::BitSet<26> letters;        // What letters are in this word?
    emp::BitSet<26> multi_letters;  // What letters are in this word more than once?

    using WordStats::word;

#ifdef STORE_NEXT_WORDS
    next_words_t next_words;
    bool has_next_words = false;
#endif

    WordData(const std::string & in_word) : WordStats(in_word) {
      for (char x : word) {
        size_t let_id = ToID(x);
        if (letters.Has(let_id)) multi_letters.Set(let_id);
        else letters.Set(let_id);
      }
    }

  };

  //////////////////////////////////////////////////////////////////////////////////////////
  //  DATA MEMBERS
  //////////////////////////////////////////////////////////////////////////////////////////

  size_t word_size;
  size_t num_ids;

  emp::vector<WordData> words;                     ///< Data about all words in this Wordle
  emp::vector<PositionClues> pos_clues;            ///< A PositionClues object for each position.
  emp::array<LetterClues,26> let_clues;            ///< Clues based off the number of letters.
  std::unordered_map<std::string, size_t> pos_map; ///< Map of words to their position ids.
  std::vector<word_list_t> next_words;             ///< Place to calculate map of results to next options
  word_list_t start_options;                       ///< Current options.
  size_t start_count;                              ///< Count of start options (cached)

  bool verbose = true;
  size_t pp_stage = 0;                             ///< How much pre-processing has been done?
  size_t pp_id_next = 0;                           ///< Word to be pre-processed for next, next.
  size_t pp_progress = 0;                          ///< Track how much progress we've made pre-processing.

public:
  WordleEngine(size_t _word_size)
  : word_size(_word_size)
  , num_ids(Result::CalcNumIDs(_word_size))
  , pos_clues(word_size)
  , next_words(num_ids)
  { }
  WordleEngine(const emp::vector<std::string> & in_words, size_t _word_size)
  : word_size(_word_size)
  , num_ids(Result::CalcNumIDs(_word_size))
  , pos_clues(word_size)
  , next_words(num_ids)
  {
    Load(in_words);
  }

  size_t GetSize() const { return words.size(); }
  size_t GetNumResults() const { return num_ids; }
   const word_list_t & GetOptions() { return start_options; }

  void SetWordSize(size_t in_size) {
    word_size = in_size;
    num_ids = Result::CalcNumIDs(word_size);
    pos_clues.resize(word_size);
    next_words.clear();
    next_words.resize(num_ids);
    words.clear();
  }

  void AddClue(const std::string & in_word, Result in_result) {
    start_options = LimitWithGuess(in_word, in_result);
  }

  struct WordSet {
    emp::vector<WordStats> words;

    size_t size() const { return words.size(); }

    bool Sort(const std::string & sort_type="alpha") {
      if (sort_type == "max") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2) {
          if (w1.max_options == w2.max_options) return w1.ave_options < w2.ave_options; // tiebreak
          return w1.max_options < w2.max_options;
        } );
      } else if (sort_type == "ave") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2){
          if (w1.ave_options == w2.ave_options) return w1.max_options < w2.max_options; // tiebreak
          return w1.ave_options < w2.ave_options;
        } );
      } else if (sort_type == "info") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2){ return w1.entropy > w2.entropy; } );
      } else if (sort_type == "alpha") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2){ return w1.word < w2.word; } );
      } else if (sort_type == "r-max") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2) {
          if (w1.max_options == w2.max_options) return w1.ave_options > w2.ave_options; // tiebreak
          return w1.max_options > w2.max_options;
        } );
      } else if (sort_type == "r-ave") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2){
          if (w1.ave_options == w2.ave_options) return w1.max_options > w2.max_options; // tiebreak
          return w1.ave_options > w2.ave_options;
        } );
      } else if (sort_type == "r-info") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2){ return w1.entropy < w2.entropy; } );
      } else if (sort_type == "r-alpha") {
        emp::Sort(words, [](const WordStats & w1, const WordStats & w2){ return w1.word > w2.word; } );
      }
      else return false;
      return true;
    }

    void Write(std::size_t max_count, std::ostream & os=std::cout) const {
      for (size_t i = 0; i < max_count && i < words.size(); ++i) {
        os << words[i].word << ", "
           << words[i].ave_options << ", "
           << words[i].max_options << ", "
           << words[i].entropy << std::endl;
      }
      if (max_count < words.size()) {
        os << "...plus " << (words.size() - max_count) << " more." << std::endl;
      }
    }
  };

  WordSet GetWords(const word_list_t & word_ids) {
    WordSet out_set;
    for (int id = word_ids.FindOne(); id >= 0; id = word_ids.FindOne(id+1)) {
      out_set.words.push_back(words[id]);
    }
    return out_set;
  }

  WordSet GetWords() { return GetWords(start_options); }
  WordSet GetAllWords() {
    WordSet out_set;
    out_set.words.reserve(words.size());
    for (const auto & word : words) out_set.words.push_back(word);
    return out_set;
  }

  /// Include a single word into this WordleEngine.
  void AddWord(const std::string & in_word) override {
    size_t id = words.size();      // Set a unique ID for this word.
    pos_map[in_word] = id;         // Keep track of the ID for this word.
    words.emplace_back(in_word);   // Setup the word data.
  }

  /// Load a whole series for words (from a file) into this WordleEngine
  void Load(const emp::vector<std::string> & in_words) override {
    // Load in all of the words.
    size_t wrong_size_count = 0;
    size_t invalid_char_count = 0;
    size_t dup_count = 0;
    for (const std::string & in_word : in_words) {
      // Only keep words of the correct size and all lowercase.
      if (in_word.size() != word_size) { wrong_size_count++; continue; }
      if (!emp::is_lower(in_word)) { invalid_char_count++; continue; }
      if (emp::Has(pos_map, in_word)) { dup_count++; continue; }
      AddWord(in_word);
    }

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

    if (verbose) std::cout << "Loaded " << words.size() << " valid words." << std::endl;
  }

  void Load(const std::string & filename) override {
    std::ifstream is(filename);
    emp::vector<std::string> in_words;
    std::string new_word;
    while (is) {
      is >> new_word;
      in_words.push_back(new_word);
    }
    Load(in_words);
  }  

  /// Clear out all prior guess information.
  void ResetOptions() override {
    start_count = words.size();
    start_options.resize(start_count);
    start_options.SetAll();
  }

  // Limit the current options based on a single guess and its result.

  word_list_t LimitWithGuess(
    const std::string & guess,
    const Result & result,
    word_list_t word_options
  ) {
    emp_assert(guess.size() == WORD_SIZE);
    emp_assert(result.size() == WORD_SIZE);

    emp::array<uint8_t, 26> letter_counts;
    std::fill(letter_counts.begin(), letter_counts.end(), 0);
    emp::BitSet<26> letter_fail;

    // First add letter clues and collect letter information.
    for (size_t i = 0; i < word_size; ++i) {
      const size_t cur_letter = ToID(guess[i]);
      if (result[i] == Result::HERE) {
        word_options &= pos_clues[i].here[cur_letter];
        ++letter_counts[cur_letter];
      } else if (result[i] == Result::ELSEWHERE) {
        word_options &= ~pos_clues[i].here[cur_letter];
        ++letter_counts[cur_letter];
      } else {  // Must be 'N'
        word_options &= ~pos_clues[i].here[cur_letter];
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

  word_list_t LimitWithGuess(const std::string & guess, const Result & result) {
    return LimitWithGuess(guess, result, start_options);
  }

  void CalculateNextWords(WordData & word_data) {
#ifdef STORE_NEXT_WORDS
    if (word_data.has_next_words) {
      next_words = word_data.next_words;
      return;
    }
#endif

    // Step through each possible result and determine what words that would leave.
    for (size_t result_id = 0; result_id < num_ids; ++result_id) {
      Result result(word_size, result_id);
      if (!result.IsValid(word_data.word)) {
        next_words[result_id].resize(0);
      } else {
        next_words[result_id] = LimitWithGuess(word_data.word, ToResult(result_id));
      }
    }    
#ifdef STORE_NEXT_WORDS
    word_data.next_words = next_words;
    word_data.has_next_words = true;
#endif
  }

  void CalculateNextWords(size_t word_id) override {
    CalculateNextWords(words[word_id]);
  }

  /// Analyze all of the possible results associated with a given word to fill out its stats.
  void AnalyzeGuess(WordData & guess, const word_list_t & cur_words) {
    size_t max_options = 0;
    size_t total_options = 0;
    double entropy = 0.0;
    const double word_count = static_cast<double>(cur_words.CountOnes());
    Result max_result;

    // Make sure we have the needed next words.
    CalculateNextWords(guess);

    // Scan through all of the possible result IDs.
    for (size_t result_id = 0; result_id < num_ids; ++result_id) {
      if (next_words[result_id].GetSize() == 0) continue;  // If next words was invalid, it's empty.
      word_list_t next_options = next_words[result_id] & cur_words;
      size_t num_options = next_options.CountOnes();
      if (num_options == 0) continue;                           // No words here; ignore.
      if (num_options > max_options) {
        max_options = num_options; // Track maximum options.
        max_result = Result(word_size, result_id);
      }
      total_options += num_options * num_options;               // Total gets added once per hit.
      double p = static_cast<double>(num_options) / word_count;
      if (p > 0.0) entropy -= p * std::log2(p);
    }
    
    // Save all of the collected data.
    guess.max_options = max_options;
    guess.ave_options = static_cast<double>(total_options) / word_count;
    guess.entropy = entropy;
    guess.max_result = max_result;
  }

  void AnalyzeGuess(size_t word_id, const word_list_t & cur_words) override {
    AnalyzeGuess(words[word_id], cur_words);
  }


  /// For solving absurdles...
  void AnalyzeMaxPairs() {
    size_t min_pair_max = 10000000;

    for (size_t id1 = 0; id1 < words.size(); ++id1) {
      WordData & word1 = words[id1];

      std::cout << "Scanning '" << word1.word << "'..." << std::endl;

      AnalyzeGuess(word1, start_options);

      // Absurdle will always pick the result with the most options.
      Result result1 = word1.max_result;
      auto remaining_options = LimitWithGuess(word1.word, result1);

      // Now try all possible second words.
      for (size_t id2 = 0; id2 < words.size(); ++id2) {
        if (id1 == id2) continue;
        WordData & word2 = words[id2];
        AnalyzeGuess(word2, remaining_options);

        size_t pair_max = word2.max_options;
        if (pair_max < min_pair_max) {
          min_pair_max = pair_max;
          std::cout << "New best: '" << word1.word << "' and '" << word2.word << "': " << pair_max << std::endl;
        }
      }
    }
  }


  bool Preprocess_SetupClues() {
    // std::cout << "Beginning pre-process phase..." << std::endl;

    // Setup all position clue info to know the number of words.
    for (size_t i=0; i < word_size; ++i) {
      pos_clues[i].pos = i;
      pos_clues[i].SetNumWords(words.size());
    }

    // Setup all letter clue information
    for (size_t let=0; let < 26; let++) {
      let_clues[let].letter = let;
      let_clues[let].SetNumWords(words.size());
    }

    return true;
  }

  bool Preprocess_LinkClues() {
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
        let_clues[letter_id].exactly[cur_count].Set(word_id);
        for (uint8_t count = 0; count <= cur_count; ++count) {
          let_clues[letter_id].at_least[count].Set(word_id);
        }
      }

      // Now figure out what POSITION clues it is consistent with.
      for (size_t pos=0; pos < word.size(); ++pos) {
        const size_t cur_letter = ToID(word[pos]);
        pos_clues[pos].here[cur_letter].Set(word_id);
      }
    }

    // std::cout << "...clues are initialized for all " << words.size() << " words..." << std::endl;

    return true;
  }

  bool Preprocess_IdentifyNextWords() {
    // std::cout << "Beginning to identify next-word options for " << words.size() << " words." << std::endl;

    // Loop through words one more time, filling out result lists and collecting data.
    size_t word_count = 0;
    const size_t step = words.size() / 98;
    while (pp_id_next < words.size()) {
      if (++word_count == step) { pp_progress++; return false; } // Keep taking breaks to update screen...
      WordData & word_data = words[pp_id_next++];
      // std::cout << "Cur word = " << word_data.word << std::endl;
      AnalyzeGuess(word_data, start_options);
    }

    return true;
  }


  /// Return a value 0.0 to 100.0 indicating current progress.
  double GetProgress() const { return (pp_progress < 100.0) ? pp_progress : 100.0; }
  size_t GetPPStage() const { return pp_stage; }

  /// If we are going to run Preprocess more than once, we need to reset it.
  void ResetPreprocess() {
    pp_stage = 0;
    pp_id_next = 0;
    pp_progress = 0;
  }

  /// Once the words are loaded, Preprocess will collect info.
  bool Preprocess() override {
    if (pp_stage == 0) {
      // emp::Alert("Ping1!");
      if (Preprocess_SetupClues()) pp_stage++;
      return false;
    }
    if (pp_stage == 1) {
      // emp::Alert("Ping2!");
      if (Preprocess_LinkClues()) pp_stage++;
      return false;
    }
    if (pp_stage == 2) {
      // emp::Alert("Ping3!");
      ResetOptions();
      pp_stage++;
      return false;
    }
    if (pp_stage == 3) {
      // emp::Alert("Ping4!");
      if (Preprocess_IdentifyNextWords()) pp_stage++;
      return false;
    }

    pp_stage = 100;
    return true;
  }

  /// If we don't want to reset everything, just identify the next words given our current state.
  bool Process() {
    return Preprocess_IdentifyNextWords();
  }

  /// Print all of the words with a given set of IDs.
  void PrintWords(const word_list_t & word_ids, size_t max_count=(size_t)-1) const {
    std::cout << "(" << word_ids.CountOnes() << " words) ";
    size_t count = 0;
    for (int id = word_ids.FindOne(); id >= 0; id = word_ids.FindOne(id+1)) {
      if (count) std::cout << ",";
      std::cout << words[id].word;
      if (++count == max_count) {
        if (id > 0) std::cout << " ...";
        break;
      }
    }
    // std::cout << " (" << word_is.CountOnes() << " words)" << std::endl;
  }

  void PrintPosClues(size_t pos) const {
    const PositionClues & clue = pos_clues[pos];
    std::cout << "Position " << pos << ":\n";
    for (uint8_t i = 0; i < 26; ++i) {
      std::cout << " '" << ToLetter(i) << "' : ";
      PrintWords(clue.here[i], 10);
      std::cout << std::endl;
    }
  }

  void PrintLetterClues(char letter) const {
    const LetterClues & clue = let_clues[ToID(letter)];
    std::cout << "Letter '" << clue.letter << "':\n";
    for (size_t i = 0; i <= MAX_LETTER_REPEAT; ++i) {
      std::cout << "EXACTLY " << i << ":  ";
      PrintWords(clue.exactly[i], 20);
      std::cout << std::endl;
    }
    for (size_t i = 0; i <= MAX_LETTER_REPEAT; ++i) {
      std::cout << "AT LEAST " << i << ": ";
      PrintWords(clue.at_least[i], 20);
      std::cout << std::endl;
    }
  }

  void PrintWordData(WordData & word_data) {
    std::cout << "WORD:     " << word_data.word << std::endl;
    std::cout << "Letters:  " << word_data.letters << std::endl;
    std::cout << "Multi:    " << word_data.multi_letters << std::endl;
    std::cout << "MAX Opts: " << word_data.max_options << std::endl;
    std::cout << "AVE Opts: " << word_data.ave_options << std::endl;
    std::cout << "Entropy:  " << word_data.entropy << std::endl;
    std::cout << std::endl;

    size_t total_count = 0;
    CalculateNextWords(word_data);
    for (size_t result_id = 0; result_id < num_ids; ++result_id) {
      Result result(word_size, result_id);
      word_list_t result_words = next_words[result_id];
      std::cout << result_id << " - " << result.ToString() << " ";
      PrintWords(result_words, 10);
      total_count += result_words.CountOnes();
      std::cout << std::endl;
    }
    std::cout << "Total Count: " << total_count << std::endl;
  }

  void PrintWordData(size_t id) { PrintWordData(words[id]); }
  void PrintWordData(const std::string & word) {
    PrintWordData(words[pos_map[word]]);
  }

  // Reorder words.  NOTE: This is destructive to all word_list data!
  void SortWords(const std::string & sort_type="max") {
    using wd_t = const WordData &;
    if (sort_type == "max") {
      emp::Sort(words, [](wd_t w1, wd_t w2){
        if (w1.max_options == w2.max_options) return w1.ave_options < w2.ave_options; // tiebreak
        return w1.max_options < w2.max_options;
      } );
    } else if (sort_type == "ave") {
      emp::Sort(words, [](wd_t w1, wd_t w2){
        if (w1.ave_options == w2.ave_options) return w1.max_options < w2.max_options; // tiebreak
        return w1.ave_options < w2.ave_options;
      } );
    } else if (sort_type == "entropy") {
      emp::Sort(words, [](wd_t w1, wd_t w2){ return w1.entropy > w2.entropy; } );
    } else if (sort_type == "word") {
      emp::Sort(words, [](wd_t w1, wd_t w2){ return w1.word < w2.word; } );
    }
    for (size_t i = 0; i < words.size(); i++) { pos_map[words[i].word] = i; } // Update ID tracking.
  }

  /// Print all of the results, sorted by max number of options.
  void PrintResults() {
    SortWords();
    for (auto & word : words) {
      std::cout << word.word
                << ", " << word.max_options
                << ", " << word.ave_options
                << ", " << word.entropy
                << std::endl;
    }
  }

  /// Print out all words as HTML.
  void PrintHTMLWord(WordData & word) {
    std::string filename = emp::to_string("web/words/", word.word, ".html");
    std::ofstream of(filename);

    // const std::string black("&#11035;");
    static const std::string white("&#11036;");
    static const std::string green("&#129001;");
    static const std::string yellow("&#129000;");

    of << "<!doctype html>\n<html lang=\"en\">\n<head>\n <title>Wordle Analysis: '"
       << word.word << "'</title>\n</head>\n<body>\n";

    of << "<h3>Wordle Analysis: " << word.word << "</h3>\n\n";
    of << "Worst case words remaining: " << word.max_options << "<br>\n";
    of << "Expected words remaining: " << word.ave_options << "<br>\n";
    of << "Information provided: " << word.entropy << "<br>\n<p>\n";

    // Loop through all possible results.
    CalculateNextWords(word);
    // for (size_t result_id = 0; result_id < NUM_IDS; ++result_id) {
    for (size_t result_id = num_ids-1; result_id < num_ids; --result_id) {
      Result result(word_size, result_id);
      word_list_t result_words = next_words[result_id];

      of << result.ToString(green, yellow, white) << " (" << result_words.CountOnes() << " words) : ";

      for (int id = result_words.FindOne(); id >= 0; id = result_words.FindOne(id+1)) {
        of << "<a href=\""  << words[id].word << ".html\">" << words[id].word << "</a> ";
      }

      of << "<br>\n";
    }

    of << "</body>\n</html>\n";

    std::cout << "Printed file '" << filename << "'." << std::endl;
  }

  void PrintHTMLWordID(int id) { PrintHTMLWord(words[(size_t) id]); }
  void PrintHTMLWord(const std::string & word) {
    PrintHTMLWord(words[pos_map[word]]);
  }

  void PrintHTMLIndex(const std::string & order) {
    SortWords(order);
    std::string filename = emp::to_string("web/index-", order, ".html");
    std::ofstream of(filename);

    of << "<!doctype html>\n<html lang=\"en\">\n<head>\n <title>Wordle Analysis: INDEX"
       "</title>\n</head>\n<body>\n"
       "<h2>Analysis of Wordle Guesses</h2>\n"
       "<p>\nWhen a guess is made in a game of Wordle, the results limit the set of words for the answer."
       " A more useful guess will limit the remaining possibilities to be as small as possible."
       " But the question remains: Which word should we choose first?"
       " Here are some analyses to help make that decision.\n"
       "<p>\nBelow are a list of 5-letter words "
       "(from <a href=\"https://www-cs-faculty.stanford.edu/~knuth/sgb-words.txt\">here</a>)"
       " with data on each.  The columns are:<br>\n"
       "<table><tr><td><b>ExpectedWords</b>:"
       "           <td>The average number of possible words if this were your first guess. (smaller is better!)</tr>\n"
       "       <tr><td><b>MaximumWords</b>:"
       "           <td>The largest possible number of words remaining after this guess. (smaller is better!)</tr>\n"
       "       <tr><td><b>Information</b>:"
       "           <td>The number of bits of information this guess provides about the final answer. (larger is better!)</tr>\n"
       "</table><p>\n"
       "Click on any column to sort by it. "
       "Click on any word to see the exact breakdown of how possible first guesses limit future options.\n"
       "<p>\n";

    of << "<table><tr><th><a href=\"index-word.html\">Word</a>"
       << "<th><a href=\"index-ave.html\">ExpectedWords</a>"
       << "<th><a href=\"index-max.html\">MaximumWords</a>"
       << "<th><a href=\"index-entropy.html\">Information</a></tr>\n";
    for (const auto & word : words) {
      of << "<tr><td><a href=\"words/" << word.word << ".html\">" << word.word << "</a>"
         << "<td>" << word.ave_options
         << "<td>" << word.max_options
         << "<td>" << word.entropy
         << "</tr>\n";        
    }
  }

  void PrintHTML() {
    size_t count = 0;
    std::cout << "Printing HTML files..." << std::endl;
    size_t step = words.size() / 100;
    for (auto & word : words) {
      if (count % step == 0) { std::cout << "."; std::cout.flush(); }
      PrintHTMLWord(word);
    }
    PrintHTMLIndex("ave");
    PrintHTMLIndex("entropy");
    PrintHTMLIndex("max");
    PrintHTMLIndex("word");
  }

};
