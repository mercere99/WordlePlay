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

#include "WordSet.hpp"
#include "5-letter-words.hpp"
//#include "5-letter-words-mid.hpp"

class WordleDriver {
private:
  size_t word_size = 5;

  emp::vector<std::string> words;
  WordSet<5> word_set5;

  enum class Status { NONE=0, NOWHERE, ELSEWHERE, HERE };
  emp::vector< emp::vector<Status> > status;


  std::string GetColor(Status status) {
    switch (status) {
    case Status::NONE: return "white";
    case Status::NOWHERE: return "gray";
    case Status::ELSEWHERE: return "yellow";
    case Status::HERE: return "#90FF90";
    }
  }

public:
  WordleDriver()
  : word_set5(wordlist5)
  {

  }
  WordleDriver(const WordleDriver &) = delete;
  WordleDriver(WordleDriver &&) = delete;

  void Start() {
    std::cout << "Wordle Analyzer!\n"
      << "Type clue followed by a word guess and the result to get more information.\n"
      << "Results should be in the form: N=Nowhere, E=Elsewhere, H=Here\n"
      << "  Example: \"clue start EHNNN\" would indicate that there is an 's', but not at\n"
      << "           the front, a 't' second, no 'a's or 'r's, and no additional 't's.\n"
      << "Other commands:\n"
      << "  exit : quit the program.\n"
      << "  status : show the current clue stack.\n"
      << "  clear : erase all current clues."
    ;
  
    std::cout << "Preprocessing...\n"
              << "-----------------------------------------------------" << std::endl;

    while (!word_set5.Preprocess()) {
      size_t progress = (size_t) word_set5.GetProgress();  // 0 to 100
      if (progress % 2 == 0) { std::cout << '#'; std::cout.flush(); }
    }
    std::cout << "\n..." << word_set5.GetSize() << " words are analyzed; " << word_set5.GetNumResults() << " results each..." << std::endl;

    // Collect inputs.
    std::string input;
    emp::vector<std::string> args;
    while (true) {
      std::cout << "> ";
      std::cout.flush();
      getline(std::cin, input);
      emp::slice(input, args, ' ');

      if (args[0] == "clue") {
        if (args.size() != 3) {
          std::cout << "Error: 'clue' command requires exactly two arguments." << std::endl;
        }
        else if (args[1].size() != word_size) {
          std::cout << "Error: Word size " << word_size << " currently active." << std::endl;
        }
        else if (args[2].size() != word_size) {
          std::cout << "Error: Results size must also be " << word_size << " characters." << std::endl;
        }
        else {
          emp::vector<Status> clue_status(word_size);
          for (size_t i = 0; i < word_size; ++i) {
            switch (args[2][i]) {
            case 'h': case 'H': clue_status[i] = Status::HERE; break;
            case 'e': case 'E': clue_status[i] = Status::ELSEWHERE; break;
            case 'n': case 'N': clue_status[i] = Status::NOWHERE; break;
            default:
              std::cout << "Error: Unknown status '" << args[2][i] << "'; using 'none'." << std::endl;
              clue_status[i] = Status::NONE;
            }
          }
          words.push_back(args[1]);
          status.push_back(clue_status);
        }
      }

      else if (args[0] == "exit" || args[0] == "quit") break;

      else if (args[0] == "status") {
        std::cout << words.size() << " clues currently enforced.\n";
        for (size_t word_id = 0; word_id < words.size(); ++word_id) {
          std::cout << "  ";
          for (size_t let_id = 0; let_id < word_size; ++let_id) {
            switch (status[word_id][let_id]) {
              case Status::HERE:      std::cout << emp::ANSI_Green()  << emp::ANSI_BlackBG(); break;
              case Status::ELSEWHERE: std::cout << emp::ANSI_Yellow() << emp::ANSI_BlackBG(); break;
              case Status::NOWHERE:   std::cout << emp::ANSI_Black()  << emp::ANSI_WhiteBG(); break;
              case Status::NONE:      std::cout << emp::ANSI_Red()    << emp::ANSI_BlackBG(); break;
            }
            std::cout << words[word_id][let_id];
          }
          std::cout << emp::ANSI_Reset() << "\n";
        }
        std::cout.flush();
      }

      else {
        std::cout << "Error: Unknown command '" << args[0] << "'." << std::endl;
      }
    }

  }
};

int main()
{
  WordleDriver driver;
  driver.Start();
}
