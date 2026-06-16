#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include <memory>
#include <string>
#include <chrono>

// Reset Counter Plugin (reconstructed + extended)
// Counts wheels-on-ball "flip resets" in a freeplay/freestyle chain, using the
// game's own GetLastWheelsHitBallTime() signal. Landing zeroes the chain.
// Adds console command: resetcounter_resetcounter (bindable) to zero on demand.

class ResetCounterPlugin :
    public BakkesMod::Plugin::BakkesModPlugin,
    public BakkesMod::Plugin::PluginSettingsWindow
{
public:
    void onLoad() override;
    void onUnload() override;

    void RenderSettings() override;
    std::string GetPluginName() override;
    void SetImGuiContext(uintptr_t ctx) override;

private:
    void onTick(std::string eventName);     // per-tick poll (SetVehicleInput)
    void onRoundStart(std::string eventName);// shot reset / new round -> zero
    void resetCounter();                      // manual + internal zero
    void render(CanvasWrapper canvas);        // HUD

    // ---- state ----
    int   resetCount    = 0;      // "Resets:" — wheels-on-ball count in this chain
    float lastCarHeight = 0.0f;   // car center Z at the last reset
    float lastBallHeight= 0.0f;   // ball center Z at the last reset
    float timeBetween   = 0.0f;   // "Time:"   — seconds between last two resets
    float totalInterval = 0.0f;   // sum of all intervals in this chain (for average)
    int   intervalCount = 0;      // number of intervals counted (resets after the first)

    float lastHitTime  = 0.0f;    // previous GetLastWheelsHitBallTime() value (contact detection)
    float lastResetTime = 0.0f;   // timestamp of the last counted reset (interval timing)
    bool  haveBaseline = false;   // whether lastHitTime is initialized this chain
    bool  flipUsedSinceReset = false; // flip/dodge spent since the last counted reset

    // "Keep count after reset": snapshot of the just-finished run, shown frozen
    // for a few seconds after landing instead of immediately zeroing the HUD.
    int   snapResetCount    = 0;
    float snapCarHeight     = 0.0f;
    float snapBallHeight    = 0.0f;
    float snapTimeBetween   = 0.0f;
    float snapTotalInterval = 0.0f;
    int   snapIntervalCount = 0;
    std::chrono::steady_clock::time_point landingTime; // when the run landed
};
