#ifndef WORDLE_RESULT_HPP
#define WORDLE_RESULT_HPP

#include "emp/base/array.hpp"
#include "emp/base/error.hpp"
#include "emp/bits/BitVector.hpp"
#include "emp/math/math.hpp"

class Result {
public:
  static constexpr size_t MAX_WORD_SIZE = 15;
  enum PositionResult { NOWHERE, ELSEWHERE, HERE, NONE };

  // Return the number of IDs for a given length result.
  static constexpr size_t CalcNumIDs(size_t result_size) { return emp::Pow<size_t>(3, result_size); }

private:
  using results_t = emp::vector<PositionResult>;

  results_t results;
  size_t id;

  /// Return a result array where each index is an associated (unique) possible result set.
  static const results_t & LookupResult(const size_t result_size, const size_t result_id) {
    emp_assert( result_size < MAX_WORD_SIZE );
    emp_assert( result_id < CalcNumIDs(result_size) );

    static emp::array< emp::vector<results_t>, MAX_WORD_SIZE > result_matrix;

    emp::vector<results_t> & result_vector = result_matrix[result_size];

    // If this is our first time requsting the result array, generate it.
    if (result_vector.size() == 0) {
      const size_t num_IDs = CalcNumIDs(result_size);
      result_vector.resize(num_IDs);
      for (size_t id = 0; id < num_IDs; ++id) {
        result_vector[id].resize(result_size);
        size_t tmp_id = id;
        for (size_t pos = result_size-1; pos < result_size; --pos) {
          const size_t magnitude = static_cast<size_t>(emp::Pow(3, static_cast<int>(pos)));
          const size_t cur_result = tmp_id / magnitude;
          result_vector[id][pos] = static_cast<PositionResult>(cur_result);
          tmp_id -= cur_result * magnitude;
        }
      }
    }

    return result_vector[result_id];
  }

  /// Assume that we have results, calculate the associated ID.
  void CalcID() {
    emp_assert(results.size() > 0 && results.size() < MAX_WORD_SIZE, results.size());
    size_t base = 1;
    id = 0;
    for (PositionResult r : results) { id += static_cast<size_t>(r) * base; base *= 3; }
    emp_assert(id < CalcNumIDs(results.size()), id);
  }

  /// Assume that we have an ID, lookup the correct results.
  void CalcResults() { results = LookupResult(results.size(), id); }
  void CalcResults(size_t result_size) { results = LookupResult(result_size, id); }

  /// Convert a results string of 'N's, 'E's, and 'W's into a Results object.
  void FromString(const std::string & result_str) {
    results.resize(result_str.size());
    for (size_t i=0; i < result_str.size(); ++i) {
      switch (result_str[i]) {
      case 'N': case 'n': results[i] = NOWHERE;   break;
      case 'E': case 'e': results[i] = ELSEWHERE; break;
      case 'H': case 'h': results[i] = HERE;      break;
      default:
        // @CAO Use exception?
        results[i] = NONE;
      };
    }
    CalcID();
  }

public:
  /// Default constructor (to make arrays of results easier to deal with)
  Result() : id(0) {}

  /// Create a result by id.
  Result(size_t result_size, size_t _id) : id(_id) { 
    emp_assert( id < CalcNumIDs(result_size), id );
    CalcResults(result_size);
  }

  /// Create a result by a result array.
  Result(const results_t & _results) : results(_results) { CalcID(); }

  /// Create a result by a result string.
  Result(const std::string & result_str) { FromString(result_str); }

  /// Create a result by an guess and answer pair.
  Result(const std::string & guess, const std::string & answer) {
    emp_assert(guess.size() == answer.size());
    results.resize(guess.size());
    emp::BitVector used(answer.size());
    // Test perfect matches.
    for (size_t i = 0; i < guess.size(); ++i) {
      if (guess[i] == answer[i]) { results[i] = HERE; used.Set(i); }
    }
    // Test offset matches.
    for (size_t i = 0; i < guess.size(); ++i) {
      if (guess[i] == answer[i]) continue;            // already matched.
      bool found = false;
      for (size_t j = 0; j < answer.size(); ++j) {    // seek a match elsewhere in answer!
        if (!used.Has(j) && guess[i] == answer[j]) {
          results[i] = ELSEWHERE;                     // found letter elsewhere!
          used.Set(j);                                // make sure this letter is noted as used.
          found = true;
          break;                                      // move on to next letter; we found this one.
        }
      }
      if (!found) results[i] = NOWHERE;
    }
    CalcID();                                         // Now that we know the symbols, figure out the ID.
  }

  Result(const Result & result) = default;
  Result(Result && result) = default;

  Result & operator=(const std::string & result_str) { FromString(result_str); return *this; }
  Result & operator=(const Result & result) = default;
  Result & operator=(Result && result) = default;

  size_t GetID() const { return id; }
  size_t GetSize() const { return results.size(); }
  size_t size() const { return results.size(); }

  bool operator==(const Result & in) const { return size() == in.size() && id == in.id; }
  bool operator!=(const Result & in) const { return size() != in.size() || id != in.id;; }
  bool operator< (const Result & in) const { return size()==in.size() ? id <  in.id : size() < in.size(); }
  bool operator<=(const Result & in) const { return size()==in.size() ? id <= in.id : size() < in.size(); }
  bool operator> (const Result & in) const { return size()==in.size() ? id >  in.id : size() > in.size(); }
  bool operator>=(const Result & in) const { return size()==in.size() ? id >= in.id : size() > in.size(); }

  PositionResult operator[](size_t id) const { return results[id]; }

  // Test if this result is valid for the given word.
  bool IsValid(const std::string & word) const {
    // Result must be the correct size.
    if (word.size() != results.size()) return false;

    // Disallow letters marked "NOWHERE" that are subsequently marked "ELSEWHERE"
    // (other order is okay).
    for (size_t pos = 0; pos < results.size()-1; ++pos) {
      if (results[pos] == NOWHERE) {
        for (size_t pos2 = pos+1; pos2 < results.size(); ++pos2) {
          if (results[pos2] == ELSEWHERE && word[pos] == word[pos2]) return false;
        }
      }
    }

    return true;
  }

  std::string ToString(
    const std::string & here="H",
    const std::string & elsewhere="E",
    const std::string & nowhere="N"
  ) const {
    std::string out; // = emp::to_string(id, "-");
    for (auto x : results) {
      if (x == HERE) out += here;
      else if (x == ELSEWHERE) out += elsewhere;
      else if (x == NOWHERE) out += nowhere;
    }
    return out;
  }
};

#endif
