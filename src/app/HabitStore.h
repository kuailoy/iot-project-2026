#pragma once

#include "Habits.h"

// Habits kept in flash (NVS, namespace "habits"), so a restart or a power
// cut doesn't lose the checked-off days. Saved after every check-off.
namespace HabitStore {

void load(Habits& habits);
void save(const Habits& habits);

}
