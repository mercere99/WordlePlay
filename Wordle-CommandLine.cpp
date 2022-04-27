/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2022.
 *
 *  @file Wordle.cpp
 */

#include "emp/base/vector.hpp"
#include "emp/tools/string_utils.hpp"

#include "Result.hpp"
#include "WordleEngine.hpp"

#include "5-letter-words.hpp"
//#include "5-letter-words-mid.hpp"

class WordleDriver {
private:
  size_t word_size;
  WordleEngine word_set;
  emp::vector<std::string> history;

  struct Clue {
    std::string word;
    Result result;
    bool is_filter;  // Some clues can be in the form of a filter.

    Clue() { };
    Clue(const std::string & _w, const Result & _r, bool _f=false)
      : word(_w), result(_r), is_filter(_f) { }
  };
  emp::vector<Clue> clues;

  template <typename... Ts>
  void Error(Ts... args) {
    std::cerr << emp::ANSI_Red() << "Error: " << emp::ANSI_Reset()
              << emp::to_string(std::forward<Ts>(args)...)
              << std::endl;
  }

public:
  WordleDriver()
  : word_size(5)
  , word_set(wordlist5, word_size)
  { }
  WordleDriver(const std::string & filename, size_t _size)
  : word_size(_size)
  , word_set(filename, _size)
  { }
  WordleDriver(const WordleDriver &) = delete;
  WordleDriver(WordleDriver &&) = delete;

  void PrintHelp() {
    std::cout << "Wordle Analyzer!\n"
      << "Type 'clue' followed by a word guess and the result to get more information.\n"
      << "Results should be in the form: N=Nowhere, E=Elsewhere, H=Here\n"
      << "  Example: \"clue start EHNNN\" would indicate that there is an 's', but not at\n"
      << "           the front, a 't' second, no 'a's or 'r's, and no additional 't's.\n"
      << "Commands:\n"
      << "   analyze    [a] perform additional tests on the words\n"
      << "                  format: analyze [mode]\n"
      << "   clue       [c] provide a new clue and its result.\n"
      << "                  format: clue [word] [result]\n"
      << "   dict       [d] list all words from the full dictionary.\n"
      << "                  format: dict [sort=alpha] [count=10] [output=screen]\n"
      << "   filter     [f] limit current words using a pattern or allowed/rejected letters.\n"
      << "                  use . for wildcard; follow with + for anywhere and - for nowhere\n"
      << "                  e.g.: 'find a..e. -pri +s' keeps words like aloes, asked, asset, etc\n"
      << "   help       [h] provide addition information about a command.\n"
      << "                  format: help [command]\n"
      << "   load       [l] load in a new dictionary\n"
      << "                  format: load [filename] [letters=5]\n"
      << "   pop        [p] remove most recently added clue.\n"
      << "   quit       [q] exit the program.\n"
      << "   reset      [r] erase all current clues.\n"
      << "   status     [.] show the current clue stack.\n"
      << "   transcript [t] print a history of all prior commands.\n"
      << "   words      [w] list top legal words (type 'help words' for full information).\n"
      << "                  format: words [sort=alpha] [count=10] [output=screen]\n"
      << "Single character shortcut given in brackets for each commenand.\n"
      << std::endl;
    ;
  }

  void PrintHelp(const std::string & term) {
    if (term == "analyze") {
      std::cout << "The 'analyze' command allows you to perform more intensive scans through words.\n"
        << "Format: analyze [command] {extra...}\n"
        << "  [command] determines the specific analysis to perform.  Options are:\n"
        << "            'loci'    [l] to list possible letters at each position (w/current words).\n"
        << "            'stats'   [s] to output statistics about the words that follow.\n"
        << "            'pairs'   [p] to scan through data for all pairs of words.\n"
        << "            'triples' [t] to scan through data for all sets of 3 words.\n"
        << "            (...more to come...)\n)"
        << std::endl;
    } else if (term == "clue") {
      std::cout << "The 'clue' command provides a Wordle guess and result limiting words appropriately.\n"
        << "Format: clue [guess] [result]\n"
        << "  [guess] is a word of the appropriate length.\n"
        << "  [result] is a string of the same length, but using N=Nowhere, E=Elsewhere, H=Here\n"
        << "Afterward, 'words' will be reduced to words that are consistent with this clue.\n"
        << std::endl;
    } else if (term == "dict") {
      std::cout << "The 'dict' command outputs all dictionary words.\n"
        << "Format: dict [sort=alpha] [count=10] [output=screen]\n"
        << "  [sort] is the order the words should be printed.  Options are:\n"
        << "         'alpha' for alphabetical (A to Z)\n"
        << "         'ave' for average words remaining after guessing (ascending).\n"
        << "         'max' for maximum words remaining after guessing (ascending).\n"
        << "         'info' for the amount of information provided about the answer (descending).\n"
        << "         Place an 'r-' in front of a sorting method to reverse it.\n"
        << "  [count] is the maximum number of words to print (10 by default).\n"
        << "  [output] prints to the filename provided (e.g, 'data.csv') or to the 'screen'.\n"
        << std::endl;
    } else if (term == "filter") {
      std::cout << "'filter' will limit the current words using a pattern or restrictions.\n"
        << "Format: filter {pattern/restrictions...}\n"
        << "  {pattern} is a series of letters of . for wildcard.\n"
        << "  {restrictions} are optional '+' or '-' followed by letters the either\n"
        << "      must be in the word or must not be, respectively.\n"
        << "Example: filter q...e +t\n"
        << "  would give 'quite' and 'quote' with the base word set.\n"
        << std::endl;
    } else if (term == "find") {
      std::cout << "'find' will search the current words for a pattern, using restrictions.\n"
        << "Format: find [pattern] {restrictions}\n"
        << "  [pattern] is a series of letters of . for wildcard.\n"
        << "  {restrictions} are optional '+' or '-' followed by letters the either\n"
        << "      must be in the word or must not be, respectively.\n"
        << "Example: find q...e +t\n"
        << "  would give 'quite' and 'quote' with the base word set.\n"
        << std::endl;
    } else if (term == "quit") {
      std::cout << "'quit' will exit the program.\n" << std::endl;
    } else if (term == "reset") {
      std::cout << "'reset' will erase all state; same as restarting the program.\n" << std::endl;
    } else if (term == "status") {
      std::cout << "'status' will print out all of the clues you have entered so far.\n" << std::endl;
    } else if (term == "transcript") {
      std::cout << "'transcript' will print out a history of all prior commands.\n" << std::endl;
    } else if (term == "words") {
      std::cout << "The 'words' command outputs all words that meet the current clues.\n"
        << "Format: words [sort=alpha] [count=10] [output=screen]\n"
        << "  [sort] is the order the words should be printed.  Options are:\n"
        << "         'alpha' for alphabetical (A to Z)\n"
        << "         'ave' for average words remaining after guessing (ascending).\n"
        << "         'max' for maximum words remaining after guessing (ascending).\n"
        << "         'info' for the amount of information provided about the answer (descending).\n"
        << "         Place an 'r-' in front of a sorting method to reverse it.\n"
        << "  [count] is the maximum number of words to print (10 by default).\n"
        << "  [output] prints to the filename provided (e.g, 'data.csv') or to the 'screen'.\n"
        << std::endl;
    }
    else {
      Error("Unknown help term '", term, "'.");
    }
  }

  void Process() {
    std::cout << "Processing...\n"
              << "------------------------------------------------------" << std::endl;

    while (!word_set.Process()) {
      std::cout << '#'; std::cout.flush();
    }
    std::cout << std::endl;
  }

  void CommandAnalyze(emp::vector<std::string> args) {
    std::string mode = args[1];

    if (mode == "loci" || mode == "l") {
      std::cout << "== Analyzing Words at each Location ==" << std::endl;
      word_set.AnalyzeLoci();
    }
    else if (mode == "pairs" || mode == "p") {
      std::cout << "== Analyzing Pairs ==" << std::endl;
      word_set.AnalyzePairs();
    }
    else if (mode == "stats" || mode == "s") {
      args.erase(args.begin(), args.begin()+2); // Remove first two args from input.
      std::cout << "== Analyzing Stats ==" << std::endl;
      word_set.AnalyzeStats(args);
    }
    else if (mode == "triples" || mode == "t") {
      std::cout << "== Analyzing Triples ==" << std::endl;
      word_set.AnalyzeTriples();
    }
    else Error("Unknown analyze mode '", mode, "'.");
  }

  void CommandClue(const std::string & clue_word, const std::string clue_result) {
    if (clue_word.size() != word_size) {
      Error("Word size ", word_size, " currently active.");
      return;
    }

    if (clue_result.size() != word_size) {
      Error("Results size must be the same as guess (", word_size, ").");
      return;
    }

    if (word_set.HasWord(clue_word) == false) {
      Error("Illegal clue word '", clue_word, "'.");
      return;
    }

    clues.emplace_back(clue_word, clue_result);

    CommandStatus();

    word_set.AddClue(clue_word, clue_result);

    std::cout << "There are " << word_set.GetOptions().size()
              << " possible words remaining (of " << word_set.GetSize() << " total)."
              << std::endl;
  }

  void CommandFilter(const emp::vector<std::string> & args) {
    if (args.size() < 2) {
      Error("Must include at least one argument to filter on.");
      return;
    }
    std::string pattern = std::string(word_size, '.'); // Default pattern is all dots.
    emp::array<size_t, 26> include_count{};
    emp::array<size_t, 26> exclude_count{};


    // Step through arguments.
    for (size_t i = 1; i < args.size(); ++i) {
      char let1 = args[i][0];
      // INCLUSION -- letters that MUST be in the word.
      if (let1 == '+') {
        for (size_t j = 1; j < args[i].size(); ++j) {
          char let = args[i][j];
          if (!emp::is_lower_letter(let)) {
            Error("Inclusions in filter must be lowercase; not '", let, "'.");
            return;
          }
          include_count[ static_cast<size_t>(let - 'a') ]++;
        }
      }
      // EXCLUSION -- letters that cannot be in the word.
      else if (let1 == '-') {
        for (size_t j = 1; j < args[i].size(); ++j) {
          char let = args[i][j];
          if (!emp::is_lower_letter(let)) {
            Error("Exclusions in filter must be lowercase; not '", let, "'.");
            return;
          }
          exclude_count[ static_cast<size_t>(let - 'a') ]++;
        }
      }
      // PATTERN -- letters that must be in specific positions.
      else if (let1 == '.' || emp::is_lower_letter(let1) || let1 == '[') {
        pattern = args[i];
      }
      // UNKNOWN
      else {
        Error("Requirements with find must begin with '+' or '-', not '", args[i][0], "'.");
        return;
      }
    }

    // Build clean include and exclude strings.
    std::string include;
    std::string exclude;
    for (size_t i = 0; i < 26; ++i) {
      for (size_t j = 0; j < include_count[i]; ++j) include.push_back('a'+i);
      if (exclude_count[i]) exclude.push_back('a'+i);
    }

    // And make the call.
    auto cur_words = word_set.FilterCurWords(pattern, include, exclude);
    word_set.SetOptions(cur_words);
    std::cout << "Filtered down to " << cur_words.GetSize() << " words." << std::endl;
    std::string filter_str;
    for (size_t i = 1; i < args.size(); ++i) {
      if (i>1) filter_str += " ";
      filter_str += args[i];
    }
    clues.emplace_back(filter_str, Result(), true);
  }
  
  void CommandFind(const emp::vector<std::string> & args) {
    if (args.size() < 2) {
      Error("Must include a pattern with find.  Eg: a..e.");
      return;
    }
    std::string pattern = args[1];
    if (pattern.size() != word_size) {
      Error("Pattern with find must be word size (", word_size, "); received '", pattern, "'");
      return;
    }

    emp::array<size_t, 26> include_count{};
    emp::array<size_t, 26> exclude_count{};

    for (size_t i = 2; i < args.size(); ++i) {
      if (args[i][0] == '+') {
        for (size_t j = 1; j < args[i].size(); ++j) {
          char let = args[i][j];
          if (!emp::is_lower_letter(let)) {
            Error("Inclusions in find must be lowercase; not '", let, "'.");
            return;
          }
          include_count[ static_cast<size_t>(let - 'a') ]++;
        }
      } else if (args[i][0] == '-') {
        for (size_t j = 1; j < args[i].size(); ++j) {
          char let = args[i][j];
          if (!emp::is_lower_letter(let)) {
            Error("Exclusions in find must be lowercase; not '", let, "'.");
            return;
          }
          exclude_count[ static_cast<size_t>(let - 'a') ]++;
        }
      } else {
        Error("Requirements with find must begin with '+' or '-', not '", args[i][0], "'.");
        return;
      }
    }

    // Build clean include and exclude strings.
    std::string include;
    std::string exclude;
    for (size_t i = 0; i < 26; ++i) {
      for (size_t j = 0; j < include_count[i]; ++j) include.push_back('a'+i);
      if (exclude_count[i]) exclude.push_back('a'+i);
    }

    // And make the call.
    auto out_words = word_set.FilterCurWords(pattern, include, exclude);
    word_set.WriteWords(out_words.AsList(), 100);
  }

  void CommandLoad(const std::string & filename, size_t in_size) {
    word_size = in_size;
    word_set.SetWordSize(word_size);
    word_set.Load(filename);
    Process();
  }

  void PrintClue(const Clue & clue) const {
    if (clue.is_filter) {
      auto filters = emp::slice(clue.word);
      for (const auto & f : filters) {
        if (f[0] == '+') std::cout << emp::ANSI_Yellow() << emp::ANSI_BlackBG();
        else if (f[0] == '-') std::cout << emp::ANSI_Red() << emp::ANSI_BlackBG();
        else std::cout << emp::ANSI_Green() << emp::ANSI_BlackBG();
        std::cout << f << " ";
      }
      std::cout << emp::ANSI_Reset() << "\n";
      return;
    }

    for (size_t let_id = 0; let_id < word_size; ++let_id) {
      switch (clue.result[let_id]) {
        case Result::HERE:      std::cout << emp::ANSI_Black() << emp::ANSI_GreenBG(); break;
        case Result::ELSEWHERE: std::cout << emp::ANSI_Black() << emp::ANSI_YellowBG(); break;
        case Result::NOWHERE:   std::cout << emp::ANSI_White() << emp::ANSI_BlackBG(); break;
        case Result::NONE:      std::cout << emp::ANSI_Red()   << emp::ANSI_BlackBG(); break;
      }
      std::cout << clue.word[let_id];
    }
    std::cout << emp::ANSI_Reset() << "\n";
  }

  void CommandPop() {
    std::cout << "Regenerating results without final clue." << std::endl;
    clues.pop_back();                           // Remove last clue.
    word_set.ResetOptions();                    // Reset available words.
    for (auto clue : clues) {                   // Add remaining clues.
      // If this clue is a filter, reconstruct it.
      if (clue.is_filter) {
        std::string filters = clue.word;
        std::string pattern(word_size,'.'), include, exclude;
        while (filters.size()) {
          if (filters[0] == '+') include = emp::string_pop_word(filters);
          if (filters[0] == '-') exclude = emp::string_pop_word(filters);
          else pattern = emp::string_pop_word(filters);
        }
        emp::remove_chars(include,"+");
        emp::remove_chars(exclude,"-");
        word_set.FilterCurWords(pattern, include, exclude);
      }
      // Otherwise process it as a proper clue.
      else word_set.AddClue(clue.word, clue.result);
    }
  }

  void CommandStatus() {
    if (clues.size() == 0) {
      std::cout << "No clues currently enforced.\n";
      return;
    }

    for (size_t clue_id = 0; clue_id < clues.size(); ++clue_id) {
      std::cout << "  [" << clue_id << "] : ";
      PrintClue(clues[clue_id]);
    }
    std::cout.flush();
  }

  void CommandTranscript() {
    std::cout << "History:\n";
    for (size_t i = 0; i < history.size(); ++i) {
      std::cout << "  " << i << " : " << history[i] << std::endl;
    }
  }

  void CommandDict(std::string sort_type, size_t count, const std::string & output) {
    if (word_set.GetSize() == 0) {
      Error("No words in dictionary.");
      return;
    }

    // If a number was used for sort type, assume that's the count and use alpha.
    if (emp::is_number(sort_type)) {
      count = emp::from_string<size_t>(sort_type);
      sort_type = "alpha";
    }

    // Get the sorted words
    auto out_words = word_set.SortAllWords(sort_type);
    if (out_words.size() == 0) {
      Error("Unknown sort type '", sort_type, "'.");
      return;
    }

    if (output == "screen") word_set.WriteWords(out_words, count);
    else {
      std::ofstream of(output);
      word_set.WriteWords(out_words, count, of);
      of.close();
    }
  }

  void CommandWords(std::string sort_type, size_t count, const std::string & output) {
    if (word_set.GetOptions().size() == 0) {
      Error("No words remaining in word list.");
      return;
    }

    // If a number was used for sort type, assume that's the count and use alpha.
    if (emp::is_number(sort_type)) {
      count = emp::from_string<size_t>(sort_type);
      sort_type = "alpha";
    }

    // Get the sorted words
    auto out_words = word_set.SortCurWords(sort_type);
    if (out_words.size() == 0) {
      Error("Unknown sort type '", sort_type, "'.");
      return;
    }

    if (output == "screen") word_set.WriteWords(out_words, count);
    else {
      std::ofstream of(output);
      word_set.WriteWords(out_words, count, of);
      of.close();
    }
  }

  bool ProcessCommandLine() {
    std::string input;
    emp::vector<std::string> args;

    std::cout << "> ";
    std::cout.flush();
    getline(std::cin, input);
    emp::compress_whitespace(input);
    if (input.size() == 0) return true; // Empty line!

    // --- handle special characters ---
    bool print_line = false;
    // Repeat last line?
    for (size_t pos = input.find("!!");
         pos != std::string::npos;
         pos = input.find("!!", pos+1))
    {
      input.replace(pos, 2, history.back());
      print_line = true;
    }

    history.push_back(input);
    if (print_line) std::cout << input << std::endl;
    emp::slice(input, args, ' ');

    // --- Process the commands ---

    if (args[0] == "analyze" || args[0] == "a") {
      if (args.size() < 2) Error("'analyze' requires specification of analyze mode.");
      else CommandAnalyze(args);
    }

    else if (args[0] == "clue" || args[0] == "c") {
      if (args.size() != 3) Error("'clue' command requires exactly two arguments.");
      else CommandClue(args[1], args[2]);
    }

    else if (args[0] == "dict" || args[0] == "d") {
      std::string sort   = (args.size() > 1) ? args[1] : "alpha";
      size_t count       = (args.size() > 2) ? std::stoul(args[2]) : 10;
      std::string output = (args.size() > 3) ? args[3] : "screen";
      CommandDict(sort, count, output);
    }

    else if (args[0] == "filter" || args[0] == "f") {
      CommandFilter(args);
    }

    else if (args[0] == "find" || args[0] == "F") {
      CommandFind(args);
    }

    else if (args[0] == "help" || args[0] == "h") {
      if (args.size() == 1) PrintHelp();
      else PrintHelp(args[1]);
    }

    else if (args[0] == "load" || args[0] == "l") {
      if (args.size() < 2) Error("'load' requires a filename.");
      else {
        size_t count = (args.size() > 2) ? std::stoul(args[2]) : 5;
        CommandLoad(args[1], count);
      }
    }

    else if (args[0] == "pop" || args[0] == "p") {
      if (clues.size() == 0) Error("Error: No clues to pop.");
      else CommandPop();
    }

    else if (args[0] == "quit" || args[0] == "exit" || args[0] == "q") return false;

    else if (args[0] == "reset" || args[0] == "r") {
      std::cout << "Clearing all current clues." << std::endl;
      clues.resize(0);
      word_set.ResetOptions();
    }

    else if (args[0] == "status" || args[0] == ".") {
      CommandStatus();
    }

    else if (args[0] == "transcript" || args[0] == "t") {
      CommandTranscript();
    }

    else if (args[0] == "words" || args[0] == "w") {
      std::string sort   = (args.size() > 1) ? args[1] : "alpha";
      size_t count       = (args.size() > 2) ? std::stoul(args[2]) : 10;
      std::string output = (args.size() > 3) ? args[3] : "screen";
      CommandWords(sort, count, output);
    }

    else Error("Unknown command '", args[0], "'.");

    return true;
  }

  void Start() {
    PrintHelp();
    Process();
    std::cout << "..." << word_set.GetSize() << " words are analyzed; " << word_set.GetNumResults() << " results each..." << std::endl;

    // Collect inputs.
    while (ProcessCommandLine());  // Process the command line until it returns 'false'.
  }

};

int main(int argc, char * argv[])
{
  if (argc > 1) {  // If arguments are provided, use them!
    std::string filename = argv[1];
    size_t word_size = 5;
    if (argc > 2) word_size = std::stod(std::string(argv[2]));
    WordleDriver driver(filename, word_size);
    driver.Start();
  }
  else { // Otherwise use the default driver.
    WordleDriver driver;
    driver.Start();
  }
}
