#pragma once

#include <stdbool.h>

// Applies the latest complete LanderViz frame. Returns true while LanderViz
// owns the RGB matrix; false lets the generated Oryx layer lighting run.
bool landerviz_render(void);
