#pragma once

// Precompiled header for the BakkesMod plugin (template-compatible).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "imgui/imgui.h"

// Common STL
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <sstream>

// Global cvar manager pointer used by the template's GUI/logging helpers.
// Declared here, defined once in pch.cpp.
extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
