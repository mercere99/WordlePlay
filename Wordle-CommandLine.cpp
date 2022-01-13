/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2022.
 *
 *  @file Wordle.cpp
 */

#include "emp/base/vector.hpp"
#include "emp/datastructs/vector_utils.hpp"
#include "emp/math/Random.hpp"
#include "emp/tools/string_utils.hpp"

#include "Result.hpp"
#include "WordSet.hpp"

#include "5-letter-words.hpp"
//#include "5-letter-words-mid.hpp"

class WordleDriver {
private:
  size_t word_size = 5;

  emp::vector<std::string> words;
  WordSet<5> word_set5;

  emp::vector< Result > status;

  struct Clue {
    std::string word;
    Result result;
  };

  template <typename... Ts>
  void Error(Ts... args) {
    std::cerr << emp::ANSI_Red() << "Error: " << emp::ANSI_Reset()
              << emp::to_string(std::forward<Ts>(args)...)
              << std::endl;
  }

public:
  WordleDriver()
  : word_set5(wordlist5)
  {

  }
  WordleDriver(const WordleDriver &) = delete;
  WordleDriver(WordleDriver &&) = delete;

  void PrintHelp() {
    std::cout << "Wordle Analyzer!\n"
      << "Type 'clue' followed by a word guess and the result to get more information.\n"
      << "Results should be in the form: N=Nowhere, E=Elsewhere, H=Here\n"
      << "  Example: \"clue start EHNNN\" would indicate that there is an 's', but not at\n"
      << "           the front, a 't' second, no 'a's or 'r's, and no additional 't's.\n"
      << "Commands:\n"
      << "   clue   : provide a new clue and its result.\n"
      << "            format: clue [word] [result]\n"
      << "   -help  : Provide addition information about a command.\n"
      << "            format: help [command]\n"
      << "   -pop   : Remove (pop off) the most recent clue\n"
      << "   quit   : exit the program.\n"
      << "   reset  : erase all current clues.\n"
      << "   status : show the current clue stack.\n"
      << "   -words : list top legal words (type 'help words' for full information).\n"
      << "            format: words [sort=alpha] [count=10] [output=screen]\n"
      << "All commands can be shortened to just their first letter.\n"
      << std::endl;
    ;
  }

  void Process() {
    std::cout << "Processing...\n"
              << "-----------------------------------------------------" << std::endl;

    while (!word_set5.Preprocess()) {
      size_t progress = (size_t) word_set5.GetProgress();  // 0 to 100
      if (progress % 2 == 0) { std::cout << '#'; std::cout.flush(); }
    }
    std::cout << "\n..." << word_set5.GetSize() << " words are analyzed; " << word_set5.GetNumResults() << " results each..." << std::endl;
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

    Result clue_status(clue_result);
    words.push_back(clue_word);
    status.push_back(clue_status);

    CommandStatus();
  }

  void CommandStatus() {
    if (words.size() == 0) {
      std::cout << "No clues currently enforced.\n";
      return;
    }

    for (size_t word_id = 0; word_id < words.size(); ++word_id) {
      std::cout << "  [" << word_id << "] : ";
      for (size_t let_id = 0; let_id < word_size; ++let_id) {
        switch (status[word_id][let_id]) {
          case Result::HERE:      std::cout << emp::ANSI_Green()  << emp::ANSI_BlackBG(); break;
          case Result::ELSEWHERE: std::cout << emp::ANSI_Yellow() << emp::ANSI_BlackBG(); break;
          case Result::NOWHERE:   std::cout << emp::ANSI_Black()  << emp::ANSI_WhiteBG(); break;
          case Result::NONE:      std::cout << emp::ANSI_Red()    << emp::ANSI_BlackBG(); break;
        }
        std::cout << words[word_id][let_id];
      }
      std::cout << emp::ANSI_Reset() << "\n";
    }
    std::cout.flush();
  }

  bool ProcessCommandLine() {
    std::string input;
    emp::vector<std::string> args;

    std::cout << "> ";
    std::cout.flush();
    getline(std::cin, input);
    emp::slice(input, args, ' ');

    if (args[0] == "clue" || args[0] == "c") {
      if (args.size() != 3) {
        std::cout << "Error: 'clue' command requires exactly two arguments." << std::endl;
      }
      else CommandClue(args[1], args[2]);
      return true;
    }

    if (args[0] == "quit" || args[0] == "exit" || args[0] == "q") return false;

    if (args[0] == "reset" || args[0] == "r") {
      std::cout << "Clearing all current clues." << std::endl;
      words.resize(0);
      status.resize(0);
      return true;
    }

    if (args[0] == "status" || args[0] == "s") {
      CommandStatus();
      return true;
    }

    std::cout << "Error: Unknown command '" << args[0] << "'." << std::endl;
    return true;
  }

  void Start() {
    PrintHelp();
    Process();

    // Collect inputs.
    while (ProcessCommandLine());  // Process the command line until it returns 'false'.
  }

};

int main()
{
  WordleDriver driver;
  driver.Start();
}
