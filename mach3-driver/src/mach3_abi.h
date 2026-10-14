#pragma once
// Official Mach3 Plugin Wizard 1.1a plain-data ABI. See docs/MACH3-ABI.en.md.
// MFC is used for our own UI only, never for Mach3-owned object layouts.
#include "Engine.h"
#include "_TrajectoryControl.h"
#include "_Mach4View.h"
#include <cstddef>
using TrajectoryControl = _TrajectoryControl;
using CMach4View = _CMach4View;
static_assert(sizeof(void *) == 4, "Mach3 uses a 32-bit ABI");
static_assert(offsetof(TrajBuffer, InSigs) == 164592, "Unexpected Mach3 input layout");
static_assert(sizeof(InputInfo) == 16, "Unexpected Mach3 input stride");
static_assert(offsetof(TrajectoryControl, StepsPerAxis) == 586904, "Unexpected Mach3 planner layout");
