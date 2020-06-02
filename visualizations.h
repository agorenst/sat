#include "cnf.h"
#include "trail.h"

#include <string>

std::string get_conflict(clause_id cid) {
  return "CONFLICT"+std::to_string(cid);
}
std::string get_node_name(literal_t x) {
  std::string negstring = "";
  if (x < 0) {
    negstring = "neg";
  }
  return "lit"+negstring+std::to_string(std::abs(x));
}
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


void print_conflict_graph(const cnf_t& cnf, const trail_t& trail) {
  std::ostream& o = std::cerr;
  //o << "digraph implication {" << std::endl;
  o << "\\begin{tikzpicture}" << std::endl;
  for (action_t a : trail) {
    if (a.is_decision()) {

      // Create the decision node
      literal_t l = a.get_literal();
      std::string name = get_node_name(l);
      //o << name << "[label=$\\Delta\\to " << render_literal(l) << "$];" << std::endl;
      o << "\\node(" << name << "){$\\Delta\\to " << render_literal(l) << "$};" << std::endl;

    }
    else if (a.is_unit_prop()) {
      const clause_t& c = cnf[a.get_clause()];
      literal_t l = a.get_literal();

      // Create the node
      o << "\\node(" << get_node_name(l) << "){$(";
      for (literal_t p : c) {
        o << render_literal(p) << " ";
      }
      o << ") \\to " << render_literal(l);
      o << "$};" << std::endl;

      // Add all the edges.
      for (literal_t p : c) {
        if (p == l) continue;
        //o << "\\draw ("+get_node_name(-p) << ") -> (" << get_node_name(l) << ");" << std::endl;
        o << "("+get_node_name(-p) << ") -> (" << get_node_name(l) << ");" << std::endl;
      }

    }
    else if (a.action_kind == action_t::action_kind_t::halt_conflict) {
      const clause_t& c = cnf[a.get_clause()];

      // Create the node
      o << "\\node(" << get_conflict(a.get_clause()) << "){$(";
      for (literal_t p : c) {
        o << render_literal(p) << " ";
      }
      o << ") \\to \\Chi";
      o << "$};" << std::endl;

      // Add all the edges.
      // TODO: Not all the literals exist as nodes.
      for (literal_t p : c) {
        o << "("+get_node_name(-p) << ") -> (" << get_conflict(a.get_clause()) << ");" << std::endl;
        //o << "\\draw ("+get_node_name(-p) << ") -> (" << get_conflict(a.get_clause()) << ");" << std::endl;
        //o << get_node_name(-p) << " -> " << get_conflict(a.get_clause()) << ";" << std::endl;
      }
    }
    else {
      assert(0);
    }
  }
  //o << "}" << std::endl;
  o << "\\end{tikzpicture}" << std::endl;
}
