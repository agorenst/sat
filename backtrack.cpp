#include "backtrack.h"

#include "trace.h" // for flag modes

// return the level to which we backtrack.
action_t *backtrack(const clause_t &c, trail_t &actions) {
  if (false && backtrack_mode == backtrack_mode_t::simplest) {
    SAT_ASSERT(std::prev(actions.end())->action_kind ==
               action_t::action_kind_t::halt_conflict);
    actions.pop();
    // it can't be completely dumb: we have to leave the prefix of our automatic
    // unit-props

    auto to_erase = std::find_if(std::begin(actions), std::end(actions),
                                 [](action_t &a) { return a.is_decision(); });
    actions.drop_from(to_erase);
  } else { // if (backtrack_mode == backtrack_mode_t::nonchron) {
    // std::cerr << "Backtracking with " << c << std::endl;
    SAT_ASSERT(std::prev(actions.end())->action_kind ==
               action_t::action_kind_t::halt_conflict);
    actions.pop();
    SAT_ASSERT(actions.clause_unsat(c));

    // Find the most recent decision. If we pop "just" this, we'll have naive
    // backtracking (that somehow still doesn't work, in that there are unsat
    // clauses in areas where there shouldn't be).

    // Find the latest decision, this naively makes us an implication
    auto bit = std::find_if(std::rbegin(actions), std::rend(actions),
                            [](const action_t &a) { return a.is_decision(); });

    // Look backwards for the next trail entry that negates something in c.
    // /That's/ the actual thing we can't pop.
    // Note if we actually pass a decision...
    bool worth_it = true;
    auto needed_for_implication =
        std::find_if(bit + 1, std::rend(actions), [&](const action_t &a) {
          // if (a.is_decision()) worth_it = true;
          return contains(c, neg(a.get_literal()));
        });

    auto del_it = needed_for_implication.base() - 1;
    SAT_ASSERT(&(*del_it) == &(*needed_for_implication));

    // Ok, we actually can get a win
    if (worth_it) {
      // std::cerr << "Case 1" << std::endl;
      // Look forwards again, starting at the trail entry after that necessary
      // one.
      // We look for the first decision we find. We basically scanned backwards,
      // finding the first thing we /couldn't/ pop without compromising the
      // unit-ness of our new clause c. Then we scan forward -- the next
      // decision we find is the "chronologically earliest" one we can pop. So
      // we do that.
      auto to_erase =
          std::find_if(del_it + 1, std::end(actions),
                       [](const action_t &a) { return a.is_decision(); });
      // if (c.size() == 1) std::cerr << c << std::endl << actions << std::endl;
      SAT_ASSERT(to_erase != std::end(actions));

      // diagnostistic: how many levels can be pop back?
      // int popcount = std::count_if(to_erase, std::end(actions), [](const
      // action_t& a) { return a.is_decision(); }); std::cout << "Popcount: " <<
      // popcount << std::endl;

      // Actually do the erasure:
      return to_erase;
      // actions.drop_from(to_erase);

      // SAT_ASSERT(actions.count_unassigned_literals(c) == 1);
      // literal_t l = find_unassigned_literal(c);
      // SAT_ASSERT(l == new_implied); // derived from when we learned the
      // clause, this passed all our tests. SAT_ASSERT(cnf[cnf.size()-1] == c);

      // Reset whatever could be units (TODO: only reset what we know we've
      // invalidated?)

      // std::cout << "Pushing " << l << " " << c << std::endl;
      // push_unit_queue(l, cnf.size()-1);
      // std::cout << "Trail is now: " << *this << std::endl;
    }
    // The simple backtrack:
    else {
      // std::cerr << "Case 2" << std::endl;
      while (actions.level(*del_it) == actions.level())
        del_it--;
      return del_it;
    }
  }
}
