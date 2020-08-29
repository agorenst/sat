#pragma once

#include <cstdint>
#include <vector>

struct action_t;

enum class v_state_t { unassigned, var_true, var_false };

typedef int32_t literal_t;
typedef int32_t variable_t;

variable_t var(literal_t l);
literal_t lit(variable_t l);
literal_t neg(literal_t l);
bool ispos(literal_t l);
literal_t dimacs_to_lit(int x);
