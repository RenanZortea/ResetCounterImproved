// Reset Counter Improved — a BakkesMod flip-reset counter for Rocket League.
//
// Based on the original "Reset Counter Plugin" by Blaku
// (https://bakkesplugins.com/plugin/178, https://github.com/blaku-rl). The
// original was distributed compiled-only; this is an independent reimplementation
// with expanded features. MIT licensed.

#include "pch.h"
#include "ResetCounterImproved.h"

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/canvaswrapper.h"

#include <sstream>
#include <string>
#include <iomanip>
#include <chrono>

BAKKESMOD_PLUGIN(ResetCounterImproved, "Reset Counter Improved",
                 "1.3", PLUGINTYPE_FREEPLAY)

// ---------------------------------------------------------------------------
// cvar names (verified from the original 1.1 binary)
// ---------------------------------------------------------------------------
static const char* CV_ENABLED          = "resetcounter_enabled";
static const char* CV_POSX             = "resetcounter_posx";
static const char* CV_POSY             = "resetcounter_posy";
static const char* CV_TEXTSIZE         = "resetcounter_textsize";
static const char* CV_DROPSHADOWS      = "resetcounter_dropshadows";
static const char* CV_HEIGHTDECIMAL    = "resetcounter_heightdecimal";
static const char* CV_COLOR            = "resetcounter_color";

// Per-line visibility: let the user pick which HUD lines to show.
static const char* CV_SHOWRESETS       = "resetcounter_showresets";
static const char* CV_SHOWHEIGHT       = "resetcounter_showheight";
static const char* CV_SHOWTIME         = "resetcounter_showtime";
static const char* CV_SHOWAVGTIME      = "resetcounter_showavgtime";

// Height source: ball height (default, like the original plugin) and/or car height.
static const char* CV_HEIGHTBALL       = "resetcounter_heightball";
static const char* CV_HEIGHTCAR        = "resetcounter_heightcar";

// Keep the reset count on screen for N seconds after it resets.
static const char* CV_NOTIFY_ENABLED   = "resetcounter_keepcount_enabled";
static const char* CV_NOTIFY_DURATION  = "resetcounter_keepcount_duration";

// Misc: append an encouragement message to the kept count ("nice job", etc.).
static const char* CV_SHOW_MESSAGE     = "resetcounter_message_enabled";

// Personal best: highest reset count reached (persisted via cvar).
static const char* CV_PB               = "resetcounter_pb";

// Custom free text overlay (for content creators / clips). Independent position.
static const char* CV_CUSTOMENABLED    = "resetcounter_customtext_enabled";
static const char* CV_CUSTOMTEXT       = "resetcounter_customtext";
static const char* CV_CUSTOMPOSX       = "resetcounter_customtext_posx";
static const char* CV_CUSTOMPOSY       = "resetcounter_customtext_posy";
static const char* CV_CUSTOMSIZE       = "resetcounter_customtext_size";

static const char* CMD_RESET           = "resetcounter_resetcounter";

// Height scale: map a center Z (Unreal units) to a 0-100 scale where 0 = object
// resting on the floor and 100 = as high as that object's center can reach at
// the ceiling. Standard RL field geometry. Ball and car differ by their radius.
static constexpr float CEILING_Z       = 2044.0f;   // ceiling height
static constexpr float CAR_GROUND_Z    = 17.0f;     // car center resting on the floor
static constexpr float BALL_RADIUS     = 92.75f;    // ball center resting on the floor

// Convert a center Z into a 0-100 height percentage, clamped.
static float toHeightPct(float z, float groundZ, float ceilingZ)
{
    float p = (z - groundZ) / (ceilingZ - groundZ) * 100.0f;
    if (p < 0.0f)   p = 0.0f;
    if (p > 100.0f) p = 100.0f;
    return p;
}

// Encouraging message shown with the reset count after landing.
static const char* resetMessage(int n)
{
    if (n <= 1) return "you can do better";
    if (n == 2) return "keep going";
    if (n == 3) return "nice job";
    if (n == 4) return "well done";
    if (n == 5) return "great job";
    if (n <= 7) return "awesome";
    if (n <= 9) return "incredible";
    return "godlike";
}

void ResetCounterImproved::onLoad()
{
    _globalCvarManager = cvarManager;

    cvarManager->registerCvar(CV_ENABLED,       "1", "Enable plugin", true, true, 0, true, 1);
    cvarManager->registerCvar(CV_POSX,          "5", "Reset Counter Text Position X");
    cvarManager->registerCvar(CV_POSY,          "5", "Reset Counter Text Position Y");
    cvarManager->registerCvar(CV_TEXTSIZE,      "2", "Reset Counter Text Size");
    cvarManager->registerCvar(CV_DROPSHADOWS,   "1", "Display text with drop shadows", true, true, 0, true, 1);
    cvarManager->registerCvar(CV_HEIGHTDECIMAL, "0", "Show decimals on the height", true, true, 0, true, 1);
    cvarManager->registerCvar(CV_COLOR,         "#FFFFFF", "Color of all text");
    cvarManager->registerCvar(CV_SHOWRESETS,    "1", "Show the resets count line",  true, true, 0, true, 1);
    cvarManager->registerCvar(CV_SHOWHEIGHT,    "1", "Show the height line",        true, true, 0, true, 1);
    cvarManager->registerCvar(CV_SHOWTIME,      "1", "Show the time line",          true, true, 0, true, 1);
    cvarManager->registerCvar(CV_SHOWAVGTIME,   "1", "Show the average time line",  true, true, 0, true, 1);
    cvarManager->registerCvar(CV_NOTIFY_ENABLED,  "1", "Keep the reset count on screen after it resets", true, true, 0, true, 1);
    cvarManager->registerCvar(CV_NOTIFY_DURATION, "3", "How long to keep the count (seconds)");
    cvarManager->registerCvar(CV_SHOW_MESSAGE,    "0", "Append an encouragement message", true, true, 0, true, 1);
    cvarManager->registerCvar(CV_PB,              "0", "Personal best reset count");
    cvarManager->registerCvar(CV_CUSTOMENABLED, "0", "Show the custom text overlay",true, true, 0, true, 1);
    cvarManager->registerCvar(CV_CUSTOMTEXT,    "",  "Custom overlay text");
    cvarManager->registerCvar(CV_CUSTOMPOSX,    "5",   "Custom text position X");
    cvarManager->registerCvar(CV_CUSTOMPOSY,    "200", "Custom text position Y");
    cvarManager->registerCvar(CV_CUSTOMSIZE,    "3",   "Custom text size");
    cvarManager->registerCvar(CV_HEIGHTBALL,    "1", "Show the ball's height",      true, true, 0, true, 1);
    cvarManager->registerCvar(CV_HEIGHTCAR,     "0", "Show the car's height",       true, true, 0, true, 1);

    // manual reset command (bindable)
    cvarManager->registerNotifier(CMD_RESET,
        [this](std::vector<std::string> args) {
            resetCounter();
            cvarManager->log("Reset counter zeroed.");
        },
        "Resets the freeplay reset counter to zero", PERMISSION_ALL);

    // per-tick poll: SetVehicleInput fires every physics tick while you have a car
    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        std::bind(&ResetCounterImproved::onTick, this, std::placeholders::_1));

    // shot reset / new round / ball explode -> zero the chain
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        std::bind(&ResetCounterImproved::onRoundStart, this, std::placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        std::bind(&ResetCounterImproved::onRoundStart, this, std::placeholders::_1));

    gameWrapper->RegisterDrawable(
        std::bind(&ResetCounterImproved::render, this, std::placeholders::_1));
}

void ResetCounterImproved::onUnload()
{
    gameWrapper->UnregisterDrawables();
}

// ---------------------------------------------------------------------------
// core: poll the game's wheels-on-ball timestamp each tick
// ---------------------------------------------------------------------------
void ResetCounterImproved::onTick(std::string)
{
    if (!gameWrapper->IsInFreeplay() && !gameWrapper->IsInCustomTraining())
        return;

    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull())
        return;

    // Landing ends the chain -> zero everything and drop the baseline.
    if (car.AnyWheelTouchingGround())
    {
        if (resetCount != 0 || lastCarHeight != 0.0f || lastBallHeight != 0.0f || timeBetween != 0.0f)
        {
            // Snapshot the run that just ended so the HUD can keep showing it.
            if (resetCount > 0)
            {
                snapResetCount    = resetCount;
                snapCarHeight     = lastCarHeight;
                snapBallHeight    = lastBallHeight;
                snapTimeBetween   = timeBetween;
                snapTotalInterval = totalInterval;
                snapIntervalCount = intervalCount;
                landingTime       = std::chrono::steady_clock::now();
            }
            resetCount     = 0;
            lastCarHeight  = 0.0f;
            lastBallHeight = 0.0f;
            timeBetween    = 0.0f;
            totalInterval  = 0.0f;
            intervalCount  = 0;
        }
        haveBaseline       = false;
        flipUsedSinceReset = false;
        return;
    }

    // Airborne: watch the game's "last time wheels hit the ball" timestamp and
    // whether the car currently holds a flip/dodge.
    float t       = car.GetLastWheelsHitBallTime();
    bool  hasFlip = car.HasFlip() != 0;

    // A flip reset hands the car its flip/dodge back, so HasFlip() goes true.
    // Track whether that flip has since been spent: a genuine *new* reset only
    // happens after the previous one's flip was used (dodge/flip). While the
    // car still holds the reset's flip, re-touching the ball is not a new reset.
    if (!hasFlip)
        flipUsedSinceReset = true;

    if (!haveBaseline)
    {
        // first airborne tick of this chain: establish baseline, don't count.
        lastHitTime  = t;
        haveBaseline = true;
        return;
    }

    // A new wheels-on-ball contact pushes this timestamp forward.
    if (t > lastHitTime + 0.0001f)
    {
        bool firstReset = (resetCount == 0);

        // Re-touch while still holding the previous reset's flip -> not a new
        // reset. Acknowledge the contact (so we don't re-evaluate it) and bail.
        if (!firstReset && !flipUsedSinceReset)
        {
            lastHitTime = t;
            return;
        }

        // Confirm the touch actually granted a flip (a real reset restores the
        // dodge -> HasFlip() true). The flip can be restored a tick or two AFTER
        // the contact, so do NOT consume the contact yet: leave lastHitTime in
        // place and keep waiting until HasFlip() reads true. A graze that never
        // grants a flip just keeps waiting until a real reset supersedes its
        // timestamp, so it never produces a count.
        if (!hasFlip)
            return;

        // The first reset has no previous reset to measure against, so it
        // produces no interval. Later resets yield a real "time between".
        if (!firstReset)
        {
            timeBetween    = t - lastResetTime; // seconds since previous reset
            totalInterval += timeBetween;       // accumulate for the running average
            intervalCount++;
        }
        else
        {
            timeBetween = 0.0f;
        }

        resetCount++;
        lastHitTime        = t;       // now consume the confirmed contact
        lastResetTime      = t;       // reference for the next interval
        flipUsedSinceReset = false;   // must spend this flip before the next count
        lastCarHeight      = car.GetLocation().Z;

        // Update the persisted personal best if this chain just beat it.
        CVarWrapper pbCvar = cvarManager->getCvar(CV_PB);
        if (pbCvar && resetCount > pbCvar.getIntValue())
            pbCvar.setValue(resetCount);

        // Capture the ball's height at the same moment (default HUD metric).
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (!server.IsNull())
        {
            BallWrapper ball = server.GetBall();
            if (!ball.IsNull())
                lastBallHeight = ball.GetLocation().Z;
        }
    }
}

void ResetCounterImproved::onRoundStart(std::string)
{
    resetCounter();
}

void ResetCounterImproved::resetCounter()
{
    resetCount     = 0;
    lastCarHeight  = 0.0f;
    lastBallHeight = 0.0f;
    timeBetween    = 0.0f;
    totalInterval  = 0.0f;
    intervalCount  = 0;
    lastResetTime  = 0.0f;
    haveBaseline   = false;
    flipUsedSinceReset = false;
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
void ResetCounterImproved::render(CanvasWrapper canvas)
{
    CVarWrapper enabledCvar = cvarManager->getCvar(CV_ENABLED);
    if (!enabledCvar) return;
    if (!enabledCvar.getBoolValue()) return;

    if (!gameWrapper->IsInFreeplay() && !gameWrapper->IsInCustomTraining())
        return;

    CVarWrapper posXCvar  = cvarManager->getCvar(CV_POSX);
    CVarWrapper posYCvar  = cvarManager->getCvar(CV_POSY);
    CVarWrapper sizeCvar  = cvarManager->getCvar(CV_TEXTSIZE);
    CVarWrapper shadCvar  = cvarManager->getCvar(CV_DROPSHADOWS);
    CVarWrapper decCvar   = cvarManager->getCvar(CV_HEIGHTDECIMAL);
    CVarWrapper colCvar   = cvarManager->getCvar(CV_COLOR);
    CVarWrapper showResetsCvar = cvarManager->getCvar(CV_SHOWRESETS);
    CVarWrapper showHeightCvar = cvarManager->getCvar(CV_SHOWHEIGHT);
    CVarWrapper showTimeCvar   = cvarManager->getCvar(CV_SHOWTIME);
    CVarWrapper showAvgCvar    = cvarManager->getCvar(CV_SHOWAVGTIME);
    CVarWrapper heightBallCvar = cvarManager->getCvar(CV_HEIGHTBALL);
    CVarWrapper heightCarCvar  = cvarManager->getCvar(CV_HEIGHTCAR);
    if (!posXCvar || !posYCvar || !sizeCvar || !shadCvar || !decCvar || !colCvar ||
        !showResetsCvar || !showHeightCvar || !showTimeCvar || !showAvgCvar ||
        !heightBallCvar || !heightCarCvar)
        return;

    float posX     = posXCvar.getFloatValue();
    float posY     = posYCvar.getFloatValue();
    float textSize = sizeCvar.getFloatValue();
    bool  shadow   = shadCvar.getBoolValue();
    bool  heightDecimal = decCvar.getBoolValue();
    LinearColor col = colCvar.getColorValue();
    bool  showResets = showResetsCvar.getBoolValue();
    bool  showHeight = showHeightCvar.getBoolValue();
    bool  showTime   = showTimeCvar.getBoolValue();
    bool  showAvg    = showAvgCvar.getBoolValue();
    bool  heightBall = heightBallCvar.getBoolValue();
    bool  heightCar  = heightCarCvar.getBoolValue();

    // Draw one string at an explicit position/size with an 8-direction black
    // outline (crisper / more like RL's HUD text than a single drop shadow).
    auto drawAt = [&](const std::string& text, float x, float yy, float size) {
        if (shadow) {
            // We draw the outline ourselves, so pass dropShadow=false per pass.
            const float o = 1.0f;
            const float dx[8] = { -o, 0.0f,  o, -o,  o, -o, 0.0f,  o };
            const float dy[8] = { -o,  -o, -o, 0.0f, 0.0f,  o,   o,  o };
            canvas.SetColor(0, 0, 0, 255);
            for (int i = 0; i < 8; ++i) {
                canvas.SetPosition(Vector2F{ x + dx[i], yy + dy[i] });
                canvas.DrawString(text, size, size, false);
            }
        }
        canvas.SetColor(col);
        canvas.SetPosition(Vector2F{ x, yy });
        canvas.DrawString(text, size, size, false);
    };

    // Counter lines all share the counter block's position and text size.
    auto drawLine = [&](const std::string& text, float y) {
        drawAt(text, posX, y, textSize);
    };

    // Pick what to display: the live run, or — if we've just landed and the
    // "keep count" window is still open — the frozen snapshot of the last run.
    int   dResetCount    = resetCount;
    float dCarHeight     = lastCarHeight;
    float dBallHeight    = lastBallHeight;
    float dTimeBetween   = timeBetween;
    float dTotalInterval = totalInterval;
    int   dIntervalCount = intervalCount;
    bool  keeping        = false;

    if (resetCount == 0 && !haveBaseline && snapResetCount > 0) {
        CVarWrapper keepOnCvar  = cvarManager->getCvar(CV_NOTIFY_ENABLED);
        CVarWrapper keepDurCvar = cvarManager->getCvar(CV_NOTIFY_DURATION);
        if (keepOnCvar && keepDurCvar && keepOnCvar.getBoolValue()) {
            float elapsed = std::chrono::duration<float>(
                std::chrono::steady_clock::now() - landingTime).count();
            if (elapsed < keepDurCvar.getFloatValue()) {
                dResetCount    = snapResetCount;
                dCarHeight     = snapCarHeight;
                dBallHeight    = snapBallHeight;
                dTimeBetween   = snapTimeBetween;
                dTotalInterval = snapTotalInterval;
                dIntervalCount = snapIntervalCount;
                keeping        = true;
            }
        }
    }

    std::ostringstream resets;
    resets << "Resets: " << dResetCount;
    // Misc: append an encouragement message while the kept count is shown.
    if (keeping) {
        CVarWrapper msgCvar = cvarManager->getCvar(CV_SHOW_MESSAGE);
        if (msgCvar && msgCvar.getBoolValue())
            resets << ", " << resetMessage(dResetCount);
    }

    // Format a 0-100 height percentage with the user's decimal preference.
    auto fmtHeight = [&](const std::string& label, float pct) {
        std::ostringstream os;
        os << label;
        if (heightDecimal) os << std::fixed << std::setprecision(1) << pct;
        else               os << (int)pct;
        return os.str();
    };

    float ballPct = toHeightPct(dBallHeight, BALL_RADIUS,  CEILING_Z - BALL_RADIUS);
    float carPct  = toHeightPct(dCarHeight,  CAR_GROUND_Z, CEILING_Z);

    // If both sources are shown, label them distinctly; otherwise a plain "Height:".
    bool both = heightBall && heightCar;
    std::string ballLine = fmtHeight(both ? "Ball Height: " : "Height: ", ballPct);
    std::string carLine  = fmtHeight(both ? "Car Height: "  : "Height: ", carPct);

    std::ostringstream timer;
    timer << "Time: " << std::fixed << std::setprecision(2) << dTimeBetween;

    float avg = dIntervalCount > 0 ? dTotalInterval / dIntervalCount : 0.0f;
    std::ostringstream avgTimer;
    avgTimer << "Avg: " << std::fixed << std::setprecision(2) << avg;

    // Draw only the enabled lines, stacking them so hidden lines leave no gap.
    float lineH = 20.0f * textSize;
    float y = posY;
    if (showResets) { drawLine(resets.str(),   y); y += lineH; }
    if (showHeight) {
        if (heightBall) { drawLine(ballLine, y); y += lineH; }
        if (heightCar)  { drawLine(carLine,  y); y += lineH; }
    }
    if (showTime)   { drawLine(timer.str(),    y); y += lineH; }
    if (showAvg)    { drawLine(avgTimer.str(), y); y += lineH; }

    // Custom creator text — independent toggle/position/size, shares color & outline.
    CVarWrapper customOnCvar = cvarManager->getCvar(CV_CUSTOMENABLED);
    if (customOnCvar && customOnCvar.getBoolValue()) {
        CVarWrapper customTextCvar = cvarManager->getCvar(CV_CUSTOMTEXT);
        CVarWrapper customPosXCvar = cvarManager->getCvar(CV_CUSTOMPOSX);
        CVarWrapper customPosYCvar = cvarManager->getCvar(CV_CUSTOMPOSY);
        CVarWrapper customSizeCvar = cvarManager->getCvar(CV_CUSTOMSIZE);
        if (customTextCvar && customPosXCvar && customPosYCvar && customSizeCvar) {
            std::string text = customTextCvar.getStringValue();
            if (!text.empty())
                drawAt(text, customPosXCvar.getFloatValue(),
                             customPosYCvar.getFloatValue(),
                             customSizeCvar.getFloatValue());
        }
    }
}

// ---------------------------------------------------------------------------
// settings window
// ---------------------------------------------------------------------------
std::string ResetCounterImproved::GetPluginName() { return "Reset Counter Improved"; }

void ResetCounterImproved::SetImGuiContext(uintptr_t ctx)
{
    // BakkesMod hands us its ImGui context here. We MUST adopt it, otherwise
    // every ImGui call in RenderSettings() runs on a null context and the game
    // access-violates the moment the settings page is opened.
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void ResetCounterImproved::RenderSettings()
{
    // Simple MVP settings: enable toggle, position/size, a couple of style
    // options, and a manual reset. Each control is guarded by a null check.
    CVarWrapper enabled = cvarManager->getCvar(CV_ENABLED);
    if (enabled) {
        bool bEnabled = enabled.getBoolValue();
        if (ImGui::Checkbox("Enable Reset Counter", &bEnabled))
            enabled.setValue(bEnabled);
    }

    ImGui::Separator();
    ImGui::Text("Lines to show");

    CVarWrapper showResets = cvarManager->getCvar(CV_SHOWRESETS);
    if (showResets) {
        bool v = showResets.getBoolValue();
        if (ImGui::Checkbox("Resets count", &v))
            showResets.setValue(v);
    }

    CVarWrapper showHeight = cvarManager->getCvar(CV_SHOWHEIGHT);
    bool bShowHeight = false;
    if (showHeight) {
        bShowHeight = showHeight.getBoolValue();
        if (ImGui::Checkbox("Height", &bShowHeight))
            showHeight.setValue(bShowHeight);
    }

    // Height source sub-options (only meaningful when the Height line is shown).
    if (bShowHeight) {
        ImGui::Indent();
        CVarWrapper heightBall = cvarManager->getCvar(CV_HEIGHTBALL);
        if (heightBall) {
            bool v = heightBall.getBoolValue();
            if (ImGui::Checkbox("Ball height", &v))
                heightBall.setValue(v);
        }
        CVarWrapper heightCar = cvarManager->getCvar(CV_HEIGHTCAR);
        if (heightCar) {
            bool v = heightCar.getBoolValue();
            if (ImGui::Checkbox("Car height", &v))
                heightCar.setValue(v);
        }
        ImGui::Unindent();
    }

    CVarWrapper showTime = cvarManager->getCvar(CV_SHOWTIME);
    if (showTime) {
        bool v = showTime.getBoolValue();
        if (ImGui::Checkbox("Time (since last reset)", &v))
            showTime.setValue(v);
    }

    CVarWrapper showAvg = cvarManager->getCvar(CV_SHOWAVGTIME);
    if (showAvg) {
        bool v = showAvg.getBoolValue();
        if (ImGui::Checkbox("Average time between resets", &v))
            showAvg.setValue(v);
    }

    ImGui::Separator();

    CVarWrapper posX = cvarManager->getCvar(CV_POSX);
    if (posX) {
        float v = posX.getFloatValue();
        if (ImGui::SliderFloat("Position X", &v, 0.0f, 1920.0f))
            posX.setValue(v);
    }

    CVarWrapper posY = cvarManager->getCvar(CV_POSY);
    if (posY) {
        float v = posY.getFloatValue();
        if (ImGui::SliderFloat("Position Y", &v, 0.0f, 1080.0f))
            posY.setValue(v);
    }

    CVarWrapper size = cvarManager->getCvar(CV_TEXTSIZE);
    if (size) {
        float v = size.getFloatValue();
        if (ImGui::SliderFloat("Text size", &v, 1.0f, 6.0f))
            size.setValue(v);
    }

    CVarWrapper shadow = cvarManager->getCvar(CV_DROPSHADOWS);
    if (shadow) {
        bool v = shadow.getBoolValue();
        if (ImGui::Checkbox("Drop shadows", &v))
            shadow.setValue(v);
    }

    CVarWrapper dec = cvarManager->getCvar(CV_HEIGHTDECIMAL);
    if (dec) {
        bool v = dec.getBoolValue();
        if (ImGui::Checkbox("Height decimals", &v))
            dec.setValue(v);
    }

    CVarWrapper col = cvarManager->getCvar(CV_COLOR);
    if (col) {
        LinearColor lc = col.getColorValue();
        float rgba[4] = { lc.R / 255.0f, lc.G / 255.0f, lc.B / 255.0f, lc.A / 255.0f };
        if (ImGui::ColorEdit4("Text color", rgba)) {
            LinearColor out{ rgba[0] * 255.0f, rgba[1] * 255.0f,
                             rgba[2] * 255.0f, rgba[3] * 255.0f };
            col.setValue(out);
        }
    }

    ImGui::Separator();
    ImGui::Text("Keep count after reset");

    CVarWrapper notifyOn = cvarManager->getCvar(CV_NOTIFY_ENABLED);
    bool bNotifyOn = false;
    if (notifyOn) {
        bNotifyOn = notifyOn.getBoolValue();
        if (ImGui::Checkbox("Keep the reset count after it resets", &bNotifyOn))
            notifyOn.setValue(bNotifyOn);
    }
    if (bNotifyOn) {
        ImGui::Indent();
        CVarWrapper notifyDur = cvarManager->getCvar(CV_NOTIFY_DURATION);
        if (notifyDur) {
            float v = notifyDur.getFloatValue();
            if (ImGui::SliderFloat("Keep for (s)", &v, 0.5f, 10.0f))
                notifyDur.setValue(v);
        }
        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::Text("Misc");

    CVarWrapper msgOn = cvarManager->getCvar(CV_SHOW_MESSAGE);
    if (msgOn) {
        bool v = msgOn.getBoolValue();
        if (ImGui::Checkbox("Encouragement message (e.g. \"nice job\")", &v))
            msgOn.setValue(v);
    }

    ImGui::Separator();
    ImGui::Text("Personal best");

    CVarWrapper pb = cvarManager->getCvar(CV_PB);
    if (pb) {
        ImGui::Text("Most resets: %d", pb.getIntValue());
        ImGui::SameLine();
        if (ImGui::Button("Reset PB"))
            pb.setValue(0);
    }

    ImGui::Separator();
    ImGui::Text("Custom text (for clips)");

    CVarWrapper customOn = cvarManager->getCvar(CV_CUSTOMENABLED);
    bool bCustomOn = false;
    if (customOn) {
        bCustomOn = customOn.getBoolValue();
        if (ImGui::Checkbox("Show custom text", &bCustomOn))
            customOn.setValue(bCustomOn);
    }

    if (bCustomOn) {
        ImGui::Indent();

        CVarWrapper customText = cvarManager->getCvar(CV_CUSTOMTEXT);
        if (customText) {
            char buf[256] = { 0 };
            std::string cur = customText.getStringValue();
            strncpy(buf, cur.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText("Text", buf, sizeof(buf)))
                customText.setValue(std::string(buf));
        }

        CVarWrapper cpx = cvarManager->getCvar(CV_CUSTOMPOSX);
        if (cpx) {
            float v = cpx.getFloatValue();
            if (ImGui::SliderFloat("Custom X", &v, 0.0f, 1920.0f))
                cpx.setValue(v);
        }
        CVarWrapper cpy = cvarManager->getCvar(CV_CUSTOMPOSY);
        if (cpy) {
            float v = cpy.getFloatValue();
            if (ImGui::SliderFloat("Custom Y", &v, 0.0f, 1080.0f))
                cpy.setValue(v);
        }
        CVarWrapper csz = cvarManager->getCvar(CV_CUSTOMSIZE);
        if (csz) {
            float v = csz.getFloatValue();
            if (ImGui::SliderFloat("Custom size", &v, 1.0f, 10.0f))
                csz.setValue(v);
        }

        ImGui::Unindent();
    }

    ImGui::Separator();

    if (ImGui::Button("Reset counter now"))
        resetCounter();
    ImGui::SameLine();
    ImGui::TextDisabled("(or bind: %s)", CMD_RESET);
}
