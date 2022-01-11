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
#include "emp/web/Animate.hpp"
//#include "emp/web/canvas_utils.hpp"
#include "emp/web/emfunctions.hpp"
#include "emp/web/KeypressManager.hpp"
//#include "emp/web/Selector.hpp"
#include "emp/web/web.hpp"

namespace UI = emp::web;

class WordleDriver : public UI::Animate {
private:
  UI::Document doc;
  UI::Table layout;
  UI::Table word_table;
  UI::Div info_panel;
  UI::KeypressManager keypress_manager;

  size_t word_size = 5;
  size_t max_tries = 6;

  bool needs_update = true;
  size_t active_row = 0;
  size_t active_col = 0;
  size_t full_rows = 0;
  std::string message;

  emp::vector<std::string> words;
  // UI::Selector mode_select;

  enum class Status { NONE=0, NOWHERE, ELSEWHERE, HERE };
  emp::vector< emp::vector<Status> > status;

  // emp::Random random;

  std::string GetColor(Status status) {
    switch (status) {
    case Status::NONE: return "white";
    case Status::NOWHERE: return "gray";
    case Status::ELSEWHERE: return "yellow";
    case Status::HERE: return "#90FF90";
    }
  }

  /// Helper to ensure active_row and active_col are legal.
  void ValidateActive() {
    full_rows = CountFullRows();
    if (active_row > full_rows) active_row = full_rows;
    if (active_row >= max_tries) active_row = max_tries-1;
    if (active_col > word_size) {
      active_col = word_size-1;
    }
  }

  void SetActive(size_t row, size_t col) {
    active_row = row;
    active_col = col;
    ValidateActive();
    needs_update = true;
  }

  bool TestFullRow(size_t row_id) {
    for (size_t pos = 0; pos < word_size; pos++) {
      if (status[row_id][pos] == Status::NONE) return false;
    }
    return emp::is_upper(words[row_id]);
  }

  size_t CountFullRows() {
    for (size_t row_id = 0; row_id < max_tries; row_id++) {
      if (!TestFullRow(row_id)) return row_id;
    }
    return max_tries;
  }

  void KeypressLetter(char key) {
    // Set the active position to this letter.
    words[active_row][active_col] = key;

    // Advance the cursor.
    active_col++;
    if (active_col == word_size) { active_row++; active_col = 0; }

    // Indicate need to update the screen.
    needs_update = true;
  }

  void KeypressSpace() {
    Status & s = status[active_row][active_col];
    switch (s) {
    case Status::NONE: s = Status::NOWHERE; break;
    case Status::NOWHERE: s = Status::ELSEWHERE; break;
    case Status::ELSEWHERE: s = Status::HERE; break;
    case Status::HERE: s = Status::NOWHERE; break;
    }
    needs_update = true;
  }

  void KeypressBackspace() {
    words[active_row][active_col] = ' ';
    status[active_row][active_col] = Status::NONE;
    KeypressLeft();
  }

  void KeypressLeft() {
    if (active_col == 0) {
      if (active_row > 0) { active_row--; active_col = word_size-1; }
      // If at 0,0, don't do anything.
    } else { active_col--; }
    needs_update = true;
  }

  void KeypressUp() {
    if (active_row > 0) --active_row;
    needs_update = true;
  }

  void KeypressRight() {
    if (active_col == word_size - 1) { active_row++; active_col = 0; }
    else { active_col++; }
    needs_update = true;
  }

  void KeypressDown() {
    active_row++;
    needs_update = true;
  }

  bool DoKeyPress(const UI::KeyboardEvent & evt)
  {
    ValidateActive();

    // If any meta keys are pressed, ignore this input.
    if (evt.altKey) return false;
    if (evt.ctrlKey) return false;
    if (evt.metaKey) return false;
    if (evt.shiftKey) return false;

    // A letter key should set the associated cell.
    char key = static_cast<char>(evt.keyCode);
    if (key >= 'A' && key <= 'Z') {
      KeypressLetter(key);
      return true;
      // doc.Redraw();
    }

    switch (key) {
    case 8: KeypressBackspace(); break;  // BACKSPACE
    case 32: KeypressSpace(); break;     // SPACEBAR
    case 37: KeypressLeft(); break;      // LEFT
    case 38: KeypressUp(); break;        // UP
    case 39: KeypressRight(); break;     // RIGHT
    case 40: KeypressDown(); break;      // DOWN
    case 46: KeypressBackspace(); break; // DELETE
    default:
      message = emp::to_string("Unknown key: '", key, "' (", (size_t) key, ")");
      needs_update = true;
      return false;
    }

    return true;
  }

public:
  WordleDriver()
  : doc("emp_base")
  , layout(1,2)
  , word_table(6,5,"word_table")
  , info_panel("info_panel")
  {
    // Initialize variables
    words.resize(max_tries);
    status.resize(max_tries);
    for (size_t word_id = 0; word_id < max_tries; ++word_id) {
      words[word_id].resize(word_size);
      status[word_id].resize(word_size);
      for (size_t let_id = 0; let_id < word_size; ++let_id) {
        words[word_id][let_id] = ' ';
        status[word_id][let_id] = Status::NONE;
      }
    }

    // Build the initial screen.
    doc << "<h2>Wordle Explorer</h2>";

    doc << layout;
    layout.GetCell(0,0) << word_table;
    layout.GetCell(0,1) << info_panel;

    info_panel << "Word Options:";

    layout.GetCell(0,0).SetCSS("vertical-align", "top");
    layout.GetCell(0,1).SetCSS("vertical-align", "top");

    // Do other web initialization
    keypress_manager.AddKeydownCallback(
      [this](const UI::KeyboardEvent & evt){ return DoKeyPress(evt); }
    );

    // graph_canvas.OnMouseDown([this](int x, int y){ MouseDown(x,y); });
    // graph_canvas.OnMouseUp([this](){ MouseUp(); });
    // graph_canvas.OnMouseMove([this](int x, int y){ MouseMove(x,y); });

    // mode_select.SetOption("Adjacency Matrix", [this](){ ActivateAdjMatrix(); });
    // mode_select.SetOption("Adjacency List", [this](){ ActivateAdjList(); });
    // mode_select.SetOption("Vertex Info", [this](){ ActivateNodeViewer(); });

    // doc << UI::Text("fps") << "FPS = " << UI::Live( [this](){return 1000.0 / GetStepTime();} ) ;
    Start();
  }
  WordleDriver(const WordleDriver &) = delete;
  WordleDriver(WordleDriver &&) = delete;

  void DoFrame() {
    if (needs_update == false) return;
    needs_update = false;    

    ValidateActive();

    // Update the adjacency matrix.
    word_table.Freeze();  // About to make major changes; don't update live!
    word_table.Clear();
    word_table.Resize(words.size(), words[0].size());
    for (size_t word_id = 0; word_id < words.size(); ++word_id) {
      std::string & word = words[word_id];
      for (size_t let_id = 0; let_id < word.size(); ++let_id) {
        auto cell = word_table.GetCell(word_id, let_id);
        cell << word[let_id];
        cell.SetCSS("background-color", GetColor(status[word_id][let_id]));
      }
    }

    // Click on a box to add a letter.
    for (size_t row_id = 0; row_id < word_table.GetNumRows(); ++row_id) {
      for (size_t col_id = 0; col_id < word_table.GetNumCols(); ++col_id) {
        auto cell = word_table.GetCell(row_id,col_id);
        cell.OnClick([this,row_id,col_id](){ SetActive(row_id,col_id); });
      }
    }

    word_table.SetCSS("border-collapse", "collapse");
    word_table.SetCSS("border", "3px solid black");
    word_table.CellsCSS("border", "1px solid black");
    word_table.CellsCSS("width", "30px");
    word_table.CellsCSS("height", "30px");
    // word_table.CellsCSS("background-color", "white");
    word_table.CellsCSS("text-align", "center");
    word_table.CellsCSS("font-family", "arial");
    word_table.CellsCSS("font-size", "20pt");

    // Pay extra attention to the styling of the active cell.
    auto active_cell = word_table.GetCell(active_row, active_col);
    if (status[active_row][active_col] == Status::NONE) {
      active_cell.SetCSS("background-color", "#CCCCFF");
    }
    active_cell.SetCSS("border", "3px solid blue");

    word_table.Activate();

    info_panel.Clear();
    info_panel << "active_row: " << active_row << "<br>"
               << "active_col: " << active_col << "<br>"
               << "full_rows: " << full_rows << "<br>"
               << "Status: " << message << "<br>"
    ;
  }
};

WordleDriver driver;

int main()
{
}
