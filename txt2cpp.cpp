#include <iostream>
#include <string>

#include "../../Dropbox/Development/Empirical/include/emp/base/vector.hpp"

int main()
{
  std::string word;
  bool start = true;
  std::cout << "emp::vector<std::string> wordlist{";
  while (std::cin) {
    std::cin >> word;
    if (!start) std::cout << ",";
    std::cout << "\n\"" << word << "\"";
    start = false;
  }
  std::cout << "};\n";
}
