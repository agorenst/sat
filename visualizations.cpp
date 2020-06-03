#include "cnf.h"
#include "trail.h"

#include <stack>
#include <map>
#include <string>
#include <set>

std::map<literal_t, std::string> literal_to_defn;

// This is a helper for render_action.
std::string render_literal(literal_t x) {
  std::string ret = "";
  if (x < 0) {
    ret += "\\overline{";
  };
  ret += "l_{" + std::to_string(std::abs(x)) + "}";
  if (x < 0) {
    ret += "}";
  }
  return ret;
}

// Given an action, it generates the node.
// This can be used in, e.g., declaring the node, drawing an edge., etc. etc.
std::string render_action(const cnf_t& cnf, const action_t& a) {
  if (a.has_literal()) {
    auto it = literal_to_defn.find(a.get_literal());
    if (it != literal_to_defn.end()) {
      return it->second;
    }
  }

  std::string res;
  res += "\"$";
  if (a.is_unit_prop()) {
    const clause_t& c = cnf[a.get_clause()];
    res += "(";
    for (literal_t l : c) {
      res += render_literal(l);
      res += " ";
    }
    res += ")";
    res += "\\to ";
    res += render_literal(a.get_literal());
  } else if (a.is_decision()) {
    res += "\\Delta \\to ";
    res += render_literal(a.get_literal());
  } else if (a.is_conflict()) {
    const clause_t& c = cnf[a.get_clause()];
    res += "(";
    for (literal_t l : c) {
      res += render_literal(l);
      res += " ";
    }
    res += ")";
    res += "\\to \\chi";
  } else {
    assert(0);
  }
  res += "$\"";

  if (a.has_literal()) {
    literal_to_defn[a.get_literal()] = res;
  }

  return res;
}

std::string draw_edge(const cnf_t& cnf, const trail_t& t, literal_t l, const action_t& p) {
  auto al = std::find_if(std::begin(t), std::end(t),
                      [l](action_t a) {
                        if (a.has_literal()) return a.get_literal() == l;
                        return false;
                      });

  assert(al != std::end(t));

  std::string res = render_action(cnf, *al) + " -> " + render_action(cnf, p) + ";\n";
  return res;
}

std::map<size_t, std::set<std::string>> layers;

void print_conflict_graph(const cnf_t& cnf, const trail_t& trail) {
  std::ostream& o = std::cerr;
  //o << "digraph implication {" << std::endl;
  o << 
    "\\RequirePackage{luatex85}" << std::endl <<
    "\\documentclass[tikz]{standalone}" << std::endl <<
    "%\\usepackage{forest}" << std::endl <<
    "%\\usetikzlibrary{er}" << std::endl <<
    "\\usetikzlibrary{graphs,graphdrawing,quotes}" << std::endl <<
    "\\usegdlibrary{trees,layered}" << std::endl <<
    "\\begin{document}" << std::endl;
  o << "\\tikz\\graph[layered layout]{" << std::endl;
  std::stack<action_t> worklist;
  for (action_t a : trail) {
    if (a.is_conflict()) {
      worklist.push(a);
    }
  }
  while (!worklist.empty()) {
    action_t a = worklist.top(); worklist.pop();
    auto r = render_action(cnf, a);
    size_t level = trail.level(a);
    auto& level_set = layers[level];
    level_set.insert(r);

    o << r << ";" << std::endl; // render the appropriate node.
    if (a.is_unit_prop()) {
      const clause_t& c = cnf[a.get_clause()];
      for (literal_t l : c) {
        if (l == a.get_literal()) { continue; }

        o << draw_edge(cnf, trail, -l, a) << std::endl;

        auto al = std::find_if(std::begin(trail), std::end(trail),
                               [l](action_t a) {
                                 if (a.has_literal()) return a.get_literal() == -l;
                                 return false;
                               });
        assert(al != std::end(trail));
        worklist.push(*al);
      }
    }
    if (a.is_conflict()) {
      const clause_t& c = cnf[a.get_clause()];
      for (literal_t l : c) {
        o << draw_edge(cnf, trail, -l, a) << std::endl;

        auto al = std::find_if(std::begin(trail), std::end(trail),
                               [l](action_t a) {
                                 if (a.has_literal()) return a.get_literal() == -l;
                                 return false;
                               });
        assert(al != std::end(trail));
        worklist.push(*al);
      }
    }
  }
  /*
  for (action_t a : trail) {
    o << render_action(cnf, a) << ";" << std::endl; // render the appropriate node.
    if (a.is_unit_prop()) {
      const clause_t& c = cnf[a.get_clause()];
      for (literal_t l : c) {
        if (l == a.get_literal()) { continue; }
        auto it = literal_to_defn.find(-l);
        assert(it != std::end(literal_to_defn));
        o << draw_edge(cnf, trail, -l, a) << std::endl;
      }
    }
    if (a.is_conflict()) {
      const clause_t& c = cnf[a.get_clause()];
      for (literal_t l : c) {
        if (l == a.get_literal()) { continue; }
        auto it = literal_to_defn.find(-l);
        if (it != literal_to_defn.end()) {
          o << draw_edge(cnf, trail, -l, a) << std::endl;
        }
      }
    }
  }
  */
  /*
  for (auto&& [level, levelset] : layers) {
    o << "{ [same layer] ";
    for (auto it = levelset.begin(); it != levelset.end(); it++) {
      o << *it;
      if (std::next(it) != std::end(levelset)) {
        o << ", ";
      }
    }
    o << "};\n";
  }
  */
  o << "};\n\\end{document}" << std::endl;
}
