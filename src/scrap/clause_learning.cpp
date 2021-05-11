// This is a core subsidiary function. It is not a plugin. It calls plugins.
// Variant where we build C on-line, and use a bitset to track our resolvent
clause_id solver_t::determine_traditional_conflict_clause() {
  bitset_stamped.reset();  // This is, in effect, the current resolution clause
  std::vector<literal_t> C;        // the clause to learn
  const size_t D = trail.level();  // the conflict level

  // The end of our trail contains the conflicting clause and literal:
  auto it = trail.rbegin();
  MAX_ASSERT(it->is_conflict());

  // Get the conflict clause
  const clause_t &conflict_clause = cnf[it->get_clause()];
  it++;

  size_t counter =
      std::count_if(std::begin(conflict_clause), std::end(conflict_clause),
                    [&](literal_t l) { return trail.level(l) == D; });
  MAX_ASSERT(counter > 1);

  size_t resolvent_size = 0;
  auto last_subsumed = std::rend(trail);
  for (literal_t l : conflict_clause) {
    bitset_stamped.set(l);
    resolvent_size++;
    if (trail.level(neg(l)) < D) {
      C.push_back(l);
    }
  }

  for (; counter > 1; it++) {
    MAX_ASSERT(it->is_unit_prop());
    literal_t L = it->get_literal();

    // We don't expect to be able to resolve against this.
    // i.e., this literal (and its reason) don't play a role
    // in resolution to derive a new clause.
    if (!bitset_stamped.get(neg(L))) {
      continue;
    }

    // L *is* bitset_stamped, so we *can* resolve against it!

    // Report that we're about to resolve against this.
    cdcl_resolve(L, it->get_clause());

    counter--;  // track the number of resolutions we're doing.
    MAX_ASSERT(resolvent_size > 0);
    resolvent_size--;

    const clause_t &d = cnf[it->get_clause()];

    for (literal_t a : d) {
      if (a == L) continue;
      // We care about future resolutions, so we negate "a"
      if (!bitset_stamped.get(a)) {
        resolvent_size++;
        bitset_stamped.set(a);
        if (trail.level(neg(a)) < D) {
          C.push_back(a);
        } else {
          // this is another thing to resolve against.
          counter++;
        }
      }
    }

    if (settings::on_the_fly_subsumption) {
      if (d.size() - 1 == resolvent_size) {
        last_subsumed = it;
        remove_literal(it->get_clause(), it->get_literal());
        MAX_ASSERT(std::find(std::begin(d), std::end(d), L) == std::end(d));
        cond_log(settings::trace_otf_subsumption, cnf[it->get_clause()],
                 lit_to_dimacs(it->get_literal()));

        // We are only good at detecting unit clauses if they're the thing we
        // learned. If on-the-fly-subsumption is able to get an intermediate
        // resolvand into unit form, we would miss that, and some core
        // assumptions of our solver would be violated. So let's look for
        // that. Experimentally things look good, but I am suspicious.
        assert(resolvent_size != 1 || counter == 1);
      }
    }
  }

  clause_id to_return = nullptr;
  bool to_lbd = true;
  if (last_subsumed == std::prev(it)) {
    to_return = last_subsumed->get_clause();
    // hack: will be re-added with "added clause"
    watch.remove_clause(to_return);
    to_lbd = lbd.remove(to_return);
    // QUESTION: how to "LCM" this? LCM with "remove_literal"?
  } else {
    while (!bitset_stamped.get(neg(it->get_literal()))) it++;

    MAX_ASSERT(it->has_literal());
    C.push_back(neg(it->get_literal()));

    MAX_ASSERT(counter == 1);
    MAX_ASSERT(C.size() == bitset_stamped.count());

    clause_t learned_clause{C};

    to_return = cnf.add_clause(std::move(learned_clause));
  }
  // Having learned the clause, now minimize it.
  // Fun fact: there are some rare cases where we're able to minimize the
  // already-existing clause, presumably thanks to our ever-smarter trail.
  // This doesn't really give us a perf benefit in my motivating benchmarks,
  // though.
  if (settings::learned_clause_minimization) {
    learned_clause_minimization(cnf, cnf[to_return], trail);
  }

  // Special, but crucial, case.
  if (cnf[to_return].size() == 0) {
    return nullptr;
  }

#if 0
// TODO: This causes some kind of invariant violation where we induce unit clauses.
  // Are there additional subsumption opportunities?
  // This is "neat" --- it makes back-tracking harder in some instances? As in,
  // it induces more units on the trail? (It's not that the clause is literally
  // reduced to 1, though that may be true too...)
  if (settings::on_the_fly_subsumption) {
    for (; it != std::rend(trail); it++) {
      if (!it->is_unit_prop()) continue;
      literal_t L = it->get_literal();
      if (!bitset_stamped.get(neg(L))) {
        continue;
      }
      bitset_stamped.clear(neg(L));
      resolvent_size--;
      const clause_t &d = cnf[it->get_clause()];
      for (literal_t a : d) {
        if (a == L) continue;
        if (bitset_stamped.get(a)) continue;
        resolvent_size++;
        bitset_stamped.set(a);
      }
      if (d.size() - 1 == resolvent_size && resolvent_size > 2) {
        std::cerr << trail << std::endl;
        std::cerr << cnf[it->get_clause()] << std::endl;
        remove_literal(it->get_clause(), it->get_literal());
        std::cerr << cnf[it->get_clause()] << std::endl;
      }
    }
  }
#endif

  added_clause(trail, to_return);

  if (!to_lbd) {
    // this was an original clause, make it unremovable.
    // This is a *really* painful hack; an artifact of how we want to
    // remove literals in LCM without the pain of triggering constant updates
    // in, e.g., watched literals or the like.
    lbd.remove(to_return);
  }

  return to_return;
}

// This is a core subsidiary function. It is not a plugin. It calls plugins.
clause_id solver_t::determine_bitset_conflict_clause() {
  bitset_stamped.reset();  // This is, in effect, the current resolution clause
  const size_t D = trail.level();  // the conflict level

  // The end of our trail contains the conflicting clause and literal:
  auto it = trail.rbegin();
  MAX_ASSERT(it->is_conflict());

  // Get the conflict clause
  const clause_t &conflict_clause = cnf[it->get_clause()];
  it++;

  size_t counter =
      std::count_if(std::begin(conflict_clause), std::end(conflict_clause),
                    [&](literal_t l) { return trail.level(l) == D; });
  MAX_ASSERT(counter > 1);

  size_t resolvent_size = 0;
  auto last_subsumed = std::rend(trail);
  for (literal_t l : conflict_clause) {
    bitset_stamped.set(l);
    resolvent_size++;
  }

  for (; counter > 1; it++) {
    MAX_ASSERT(it->is_unit_prop());
    literal_t L = it->get_literal();

    // We don't expect to be able to resolve against this.
    // i.e., this literal (and its reason) don't play a role
    // in resolution to derive a new clause.
    if (!bitset_stamped.get(neg(L))) {
      continue;
    }

    // L *is* bitset_stamped, so we *can* resolve against it!

    // Report that we're about to resolve against this.
    cdcl_resolve(L, it->get_clause());

    counter--;  // track the number of resolutions we're doing.
    bitset_stamped.clear(neg(L));
    MAX_ASSERT(resolvent_size > 0);
    resolvent_size--;

    const clause_t &d = cnf[it->get_clause()];
    // resolvent = resolve_ref(resolvent, d, neg(L));

    for (literal_t a : d) {
      if (a == L) continue;
      // We care about future resolutions, so we negate "a"
      if (!bitset_stamped.get(a)) {
        resolvent_size++;
        bitset_stamped.set(a);
        if (trail.level(neg(a)) < D) {
          // C.push_back(a);
        } else {
          // this is another thing to resolve against.
          counter++;
        }
      }
    }

    if (settings::on_the_fly_subsumption) {
      if (d.size() - 1 == resolvent_size) {
        last_subsumed = it;
        remove_literal(it->get_clause(), it->get_literal());
        MAX_ASSERT(std::find(std::begin(d), std::end(d), L) == std::end(d));
        cond_log(settings::trace_otf_subsumption, cnf[it->get_clause()],
                 lit_to_dimacs(it->get_literal()));

        // We are only good at detecting unit clauses if they're the thing we
        // learned. If on-the-fly-subsumption is able to get an intermediate
        // resolvand into unit form, we would miss that, and some core
        // assumptions of our solver would be violated. So let's look for
        // that. Experimentally things look good, but I am suspicious.
        assert(resolvent_size != 1 || counter == 1);
      }
    }
  }

  clause_id to_return = nullptr;
  bool to_lbd = true;
  if (last_subsumed == std::prev(it)) {
    to_return = last_subsumed->get_clause();
    // hack: will be re-added with "added clause"
    watch.remove_clause(to_return);
    to_lbd = lbd.remove(to_return);
    // QUESTION: how to "LCM" this? LCM with "remove_literal"?
  } else {
    while (!bitset_stamped.get(neg(it->get_literal()))) it++;

    MAX_ASSERT(it->has_literal());

    MAX_ASSERT(counter == 1);

    // TODO: is there a "nice" sort we can do here that could be helpful?
    // VSIDS, for instance?

    clause_t learned_clause{bitset_stamped};

    to_return = cnf.add_clause(std::move(learned_clause));
  }
  // Having learned the clause, now minimize it.
  // Fun fact: there are some rare cases where we're able to minimize the
  // already-existing clause, presumably thanks to our ever-smarter trail.
  // This doesn't really give us a perf benefit in my motivating benchmarks,
  // though.
  if (settings::learned_clause_minimization) {
    learned_clause_minimization(cnf, cnf[to_return], trail);
  }

  // Special, but crucial, case.
  if (cnf[to_return].size() == 0) {
    return nullptr;
  }

#if 0
// TODO: This causes some kind of invariant violation where we induce unit clauses.
  // Are there additional subsumption opportunities?
  // This is "neat" --- it makes back-tracking harder in some instances? As in,
  // it induces more units on the trail? (It's not that the clause is literally
  // reduced to 1, though that may be true too...)
  if (settings::on_the_fly_subsumption) {
    for (; it != std::rend(trail); it++) {
      if (!it->is_unit_prop()) continue;
      literal_t L = it->get_literal();
      if (!bitset_stamped.get(neg(L))) {
        continue;
      }
      bitset_stamped.clear(neg(L));
      resolvent_size--;
      const clause_t &d = cnf[it->get_clause()];
      for (literal_t a : d) {
        if (a == L) continue;
        if (bitset_stamped.get(a)) continue;
        resolvent_size++;
        bitset_stamped.set(a);
      }
      if (d.size() - 1 == resolvent_size && resolvent_size > 2) {
        std::cerr << trail << std::endl;
        std::cerr << cnf[it->get_clause()] << std::endl;
        remove_literal(it->get_clause(), it->get_literal());
        std::cerr << cnf[it->get_clause()] << std::endl;
      }
    }
  }
#endif

  added_clause(trail, to_return);

  if (!to_lbd) {
    // this was an original clause, make it unremovable.
    // This is a *really* painful hack; an artifact of how we want to
    // remove literals in LCM without the pain of triggering constant updates
    // in, e.g., watched literals or the like.
    lbd.remove(to_return);
  }

  return to_return;
}