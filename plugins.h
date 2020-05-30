#pragma once
#include <functional>
#include "cnf.h"

typedef std::function<void(clause_id cid)> remove_clause_listener;
void remove_clause(clause_id cid);
void remove_clause_listener_reg(remove_clause_listener r);
