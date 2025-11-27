#include "ImguiUI.h"
#include "UIWidgets.h"
#include "ResolutionEditor.h"

#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "libultraship/src/Context.h"

#include <imgui_internal.h>
#include <libultraship/libultraship.h>
#include <Fast3D/interpreter.h>
#include "port/Engine.h"
#include "port/notification/notification.h"
#include "utils/StringHelper.h"

#ifdef __SWITCH__
#include <port/switch/SwitchImpl.h>
#include <port/switch/SwitchPerformanceProfiles.h>
#endif

#ifdef __IOS__
#include "libultraship/ios/StarshipBridge.h"

// Track touch state for proper button behavior on iOS
// We need to track where touch-down occurred and only trigger on touch-up within same button
static ImVec2 g_touchDownPos = ImVec2(-1, -1);
static bool g_touchConsumed = false;
static int g_touchFrameCount = -1;
static bool g_touchDownRecorded = false;  // Track if we recorded a touch-down this frame
static bool g_needsClearNextFrame = false;  // Clear touch-down pos on NEXT frame after release

// Touch scrolling state
static ImVec2 g_scrollTouchDownPos = ImVec2(-1, -1);  // Touch-down position for scroll detection
static bool g_wasTouchingLastFrame = false;  // Track touch state changes manually
static bool g_isDragging = false;
static float g_scrollVelocity = 0.0f;

// Touch-friendly button helper for iOS
// Standard ImGui::Button() relies on hover detection which happens at NewFrame() time,
// before touch events are processed. With touch input, there's no persistent hover -
// the touch arrives after hover calculation with the old (FLT_MAX) position.
// This helper implements proper touch button behavior:
// - Records touch-down position
// - Only triggers on touch-up if still within same button bounds
// - Prevents multiple buttons from reacting to the same touch
static bool TouchFriendlyButton(const char* label, const ImVec2& size) {
    ImGuiIO& io = ImGui::GetIO();

    // Reset flags at start of each frame
    int currentFrame = ImGui::GetFrameCount();
    if (g_touchFrameCount != currentFrame) {
        g_touchFrameCount = currentFrame;
        g_touchConsumed = false;
        g_touchDownRecorded = false;

        // Clear touch-down tracking if flagged from previous frame
        // This ensures we don't clear it on the same frame as the release
        if (g_needsClearNextFrame) {
            g_touchDownPos = ImVec2(-1, -1);
            g_needsClearNextFrame = false;
        }
    }

    // Draw the button for visual display
    bool standardResult = ImGui::Button(label, size);

    // Get the button's screen-space bounds
    ImVec2 buttonMin = ImGui::GetItemRectMin();
    ImVec2 buttonMax = ImGui::GetItemRectMax();

    // Check if current position is within bounds
    bool posInBounds = (io.MousePos.x >= buttonMin.x && io.MousePos.x <= buttonMax.x &&
                        io.MousePos.y >= buttonMin.y && io.MousePos.y <= buttonMax.y);

    // Record touch-down position (only one button should record it)
    if (io.MouseClicked[0] && posInBounds && !g_touchDownRecorded) {
        g_touchDownPos = io.MousePos;
        g_touchDownRecorded = true;
        SPDLOG_INFO("[TouchFriendlyButton] '{}' touch-down recorded at ({:.0f}, {:.0f})",
            label, io.MousePos.x, io.MousePos.y);
    }

    // Check if touch-down was within this button's bounds
    bool touchDownInBounds = (g_touchDownPos.x >= buttonMin.x && g_touchDownPos.x <= buttonMax.x &&
                              g_touchDownPos.y >= buttonMin.y && g_touchDownPos.y <= buttonMax.y);

    // Calculate distance between touch-down and touch-up positions
    // Small movements during a tap are normal on touchscreens
    float touchDeltaX = io.MousePos.x - g_touchDownPos.x;
    float touchDeltaY = io.MousePos.y - g_touchDownPos.y;
    float touchDistance = sqrtf(touchDeltaX * touchDeltaX + touchDeltaY * touchDeltaY);

    // Tap tolerance: if finger moved less than this many pixels, consider it a tap not a scroll
    const float TAP_TOLERANCE = 50.0f;
    bool isValidTap = touchDistance < TAP_TOLERANCE;

    // Trigger on touch-up (MouseReleased) if:
    // 1. Touch-down was within this button
    // 2. Finger didn't move much (it's a tap, not a scroll)
    // 3. Touch hasn't been consumed by another button this frame
    bool touchClicked = io.MouseReleased[0] && touchDownInBounds && isValidTap && !g_touchConsumed;

    if (touchClicked) {
        SPDLOG_INFO("[TouchFriendlyButton] '{}' ACTIVATED!", label);
        g_touchConsumed = true;  // Prevent other buttons from triggering
        g_needsClearNextFrame = true;  // Clear touch-down pos on next frame
    }

    // If mouse was released but no button was clicked, still need to clear for next touch
    if (io.MouseReleased[0] && !g_needsClearNextFrame) {
        g_needsClearNextFrame = true;
    }

    // Return true if either standard ImGui click detection worked OR our touch detection
    return standardResult || touchClicked;
}

// Touch-friendly checkbox for iOS - large toggle button style
// Returns true if value changed
static bool TouchFriendlyCheckbox(const char* label, const char* cvar, bool defaultValue = false) {
    bool currentValue = CVarGetInteger(cvar, defaultValue ? 1 : 0) != 0;

    ImGuiIO& io = ImGui::GetIO();
    float availWidth = ImGui::GetContentRegionAvail().x;
    float toggleWidth = 70.0f;
    float buttonHeight = 44.0f;

    // Draw label on left, toggle on right
    ImGui::PushID(cvar);

    // Create a button for the entire row for easier touch targeting
    ImVec2 rowSize(availWidth, buttonHeight);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Style based on current value
    if (currentValue) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.3f, 1.0f));  // Green when ON
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.4f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));  // Gray when OFF
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    }

    // Format button text: label on left, ON/OFF on right
    char buttonText[256];
    snprintf(buttonText, sizeof(buttonText), "%-30s %s", label, currentValue ? "ON" : "OFF");

    bool clicked = TouchFriendlyButton(buttonText, rowSize);

    ImGui::PopStyleColor(2);
    ImGui::PopID();

    if (clicked) {
        bool newValue = !currentValue;
        CVarSetInteger(cvar, newValue ? 1 : 0);
        Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        return true;
    }

    return false;
}

// Touch-friendly slider for iOS - large touch targets with +/- buttons
// Returns true if value changed
static bool TouchFriendlySliderFloat(const char* label, const char* cvar, float minVal, float maxVal, float defaultVal, float step = 0.0f) {
    float currentValue = CVarGetFloat(cvar, defaultVal);
    if (step == 0.0f) step = (maxVal - minVal) / 20.0f;  // Default to 5% steps

    float availWidth = ImGui::GetContentRegionAvail().x;
    float buttonWidth = 50.0f;
    float buttonHeight = 44.0f;

    ImGui::PushID(cvar);

    // Label
    ImGui::Text("%s", label);

    // Value display and +/- buttons on same row
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.6f, 1.0f));

    bool changed = false;

    // Minus button
    if (TouchFriendlyButton("-", ImVec2(buttonWidth, buttonHeight))) {
        currentValue = fmaxf(minVal, currentValue - step);
        changed = true;
    }

    ImGui::SameLine();

    // Value display (as percentage if 0-1 range, otherwise raw value)
    float displayWidth = availWidth - buttonWidth * 2 - 20.0f;
    char valueText[64];
    if (maxVal <= 1.0f && minVal >= 0.0f) {
        snprintf(valueText, sizeof(valueText), "%.0f%%", currentValue * 100.0f);
    } else {
        snprintf(valueText, sizeof(valueText), "%.1f", currentValue);
    }

    // Center the value text
    ImVec2 textSize = ImGui::CalcTextSize(valueText);
    float textPosX = (displayWidth - textSize.x) / 2.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textPosX);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (buttonHeight - textSize.y) / 2.0f);
    ImGui::Text("%s", valueText);
    ImGui::SameLine();
    ImGui::SetCursorPosX(availWidth - buttonWidth);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (buttonHeight - textSize.y) / 2.0f);

    // Plus button
    if (TouchFriendlyButton("+", ImVec2(buttonWidth, buttonHeight))) {
        currentValue = fminf(maxVal, currentValue + step);
        changed = true;
    }

    ImGui::PopStyleColor(2);
    ImGui::PopID();

    if (changed) {
        CVarSetFloat(cvar, currentValue);
        Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }

    return changed;
}

// Touch-friendly integer slider
static bool TouchFriendlySliderInt(const char* label, const char* cvar, int minVal, int maxVal, int defaultVal, int step = 1) {
    int currentValue = CVarGetInteger(cvar, defaultVal);

    float availWidth = ImGui::GetContentRegionAvail().x;
    float buttonWidth = 50.0f;
    float buttonHeight = 44.0f;

    ImGui::PushID(cvar);

    // Label
    ImGui::Text("%s", label);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.6f, 1.0f));

    bool changed = false;

    // Minus button
    if (TouchFriendlyButton("-", ImVec2(buttonWidth, buttonHeight))) {
        currentValue = (currentValue - step < minVal) ? minVal : currentValue - step;
        changed = true;
    }

    ImGui::SameLine();

    // Value display
    float displayWidth = availWidth - buttonWidth * 2 - 20.0f;
    char valueText[64];
    snprintf(valueText, sizeof(valueText), "%d", currentValue);

    ImVec2 textSize = ImGui::CalcTextSize(valueText);
    float textPosX = (displayWidth - textSize.x) / 2.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textPosX);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (buttonHeight - textSize.y) / 2.0f);
    ImGui::Text("%s", valueText);
    ImGui::SameLine();
    ImGui::SetCursorPosX(availWidth - buttonWidth);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (buttonHeight - textSize.y) / 2.0f);

    // Plus button
    if (TouchFriendlyButton("+", ImVec2(buttonWidth, buttonHeight))) {
        currentValue = (currentValue + step > maxVal) ? maxVal : currentValue + step;
        changed = true;
    }

    ImGui::PopStyleColor(2);
    ImGui::PopID();

    if (changed) {
        CVarSetInteger(cvar, currentValue);
        Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }

    return changed;
}

// Section header for organizing options
static void TouchFriendlySectionHeader(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}
#endif

extern "C" {
#include "sys.h"
#include <sf64audio_provisional.h>
#include <sf64context.h>
}

namespace GameUI {
std::shared_ptr<GameMenuBar> mGameMenuBar;
std::shared_ptr<Ship::GuiWindow> mConsoleWindow;
std::shared_ptr<Ship::GuiWindow> mStatsWindow;
std::shared_ptr<Ship::GuiWindow> mInputEditorWindow;
std::shared_ptr<Ship::GuiWindow> mGfxDebuggerWindow;
std::shared_ptr<Notification::Window> mNotificationWindow;
std::shared_ptr<AdvancedResolutionSettings::AdvancedResolutionSettingsWindow> mAdvancedResolutionSettingsWindow;

void SetupGuiElements() {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();

    auto& style = ImGui::GetStyle();
    style.FramePadding = ImVec2(4.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.Colors[ImGuiCol_MenuBarBg] = UIWidgets::Colors::DarkGray;

    mGameMenuBar = std::make_shared<GameMenuBar>("gOpenMenuBar", CVarGetInteger("gOpenMenuBar", 0));
    gui->SetMenuBar(mGameMenuBar);

    if (gui->GetMenuBar() && !gui->GetMenuBar()->IsVisible()) {
#if defined(__SWITCH__) || defined(__WIIU__)
        Notification::Emit({ .message = "Press - to access enhancements menu", .remainingTime = 10.0f });
#elif defined(__IOS__)
        Notification::Emit({ .message = "Tap ⚙️ in top-right to access settings menu", .remainingTime = 10.0f });
#else
        Notification::Emit({ .message = "Press F1 to access enhancements menu", .remainingTime = 10.0f });
#endif
    }

    mStatsWindow = gui->GetGuiWindow("Stats");
    if (mStatsWindow == nullptr) {
        SPDLOG_ERROR("Could not find stats window");
    }

    mConsoleWindow = gui->GetGuiWindow("Console");
    if (mConsoleWindow == nullptr) {
        SPDLOG_ERROR("Could not find console window");
    }

    mInputEditorWindow = gui->GetGuiWindow("Input Editor");
    if (mInputEditorWindow == nullptr) {
        SPDLOG_ERROR("Could not find input editor window");
        return;
    }

    mGfxDebuggerWindow = gui->GetGuiWindow("GfxDebuggerWindow");
    if (mGfxDebuggerWindow == nullptr) {
        SPDLOG_ERROR("Could not find input GfxDebuggerWindow");
    }

    mAdvancedResolutionSettingsWindow = std::make_shared<AdvancedResolutionSettings::AdvancedResolutionSettingsWindow>("gAdvancedResolutionEditorEnabled", "Advanced Resolution Settings");
    gui->AddGuiWindow(mAdvancedResolutionSettingsWindow);
    mNotificationWindow = std::make_shared<Notification::Window>("gNotifications", "Notifications Window");
    gui->AddGuiWindow(mNotificationWindow);
    mNotificationWindow->Show();
}

void Destroy() {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
    gui->RemoveAllGuiWindows();

    mAdvancedResolutionSettingsWindow = nullptr;
    mConsoleWindow = nullptr;
    mStatsWindow = nullptr;
    mInputEditorWindow = nullptr;
    mNotificationWindow = nullptr;
}

std::string GetWindowButtonText(const char* text, bool menuOpen) {
    char buttonText[100] = "";
    if (menuOpen) {
        strcat(buttonText, ICON_FA_CHEVRON_RIGHT " ");
    }
    strcat(buttonText, text);
    if (!menuOpen) { strcat(buttonText, "  "); }
    return buttonText;
}
}

static const char* filters[3] = {
#ifdef __WIIU__
        "",
#else
        "Three-Point",
#endif
        "Linear", "None"
};

static const char* voiceLangs[] = {
    "Original", /*"Japanese",*/ "Lylat"
};

void DrawSpeakerPositionEditor() {
    static ImVec2 lastCanvasPos;
    ImGui::Text("Speaker Position Editor");
    ImVec2 canvasSize = ImVec2(200, 200); // Static canvas size
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(canvasPos.x + canvasSize.x / 2, canvasPos.y + canvasSize.y / 2);

    // Speaker positions
    static ImVec2 speakerPositions[4];
    static bool initialized = false;
    static float radius = 80.0f;

    // Reset positions if canvas position changed (window resized/moved)
    if (!initialized || (lastCanvasPos.x != canvasPos.x || lastCanvasPos.y != canvasPos.y)) {
        const char* cvarNames[4] = { "gPositionFrontLeft", "gPositionFrontRight", "gPositionRearLeft", "gPositionRearRight" };
        float angles[4] = { 240.f, 300.f, 160.f, 20.f }; // Default angles
        
        for (int i = 0; i < 4; i++) {
            int savedAngle = CVarGetInteger(cvarNames[i], -1);
            if (savedAngle != -1) {
                angles[i] = static_cast<float>(savedAngle);
            }

            float rad = angles[i] * (M_PI / 180.0f);
            speakerPositions[i] = ImVec2(center.x + radius * cosf(rad), center.y + radius * sinf(rad));
        }
        initialized = true;
        lastCanvasPos = canvasPos;
    }

    // Draw canvas
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(26, 26, 26, 255));
    drawList->AddCircleFilled(center, 5.0f, IM_COL32(255, 255, 255, 255)); // Central person

    // Draw circle line for speaker positions
    drawList->AddCircle(center, radius, IM_COL32(163, 163, 163, 255), 100);

    // Add markers at 0, 22.5, 45, etc.
    for (float angle = 0; angle < 360; angle += 22.5f) {
        float rad = angle * (M_PI / 180.0f);
        ImVec2 markerStart = ImVec2(center.x + (radius - 5) * cosf(rad), center.y + (radius - 5) * sinf(rad));
        ImVec2 markerEnd = ImVec2(center.x + radius * cosf(rad), center.y + radius * sinf(rad));
        drawList->AddLine(markerStart, markerEnd, IM_COL32(163, 163, 163, 255));
    }

    const char* speakerLabels[4] = { "L", "R", "RL", "RR" };
    const char* cvarNames[4] = { "gPositionFrontLeft", "gPositionFrontRight", "gPositionRearLeft", "gPositionRearRight" };

    const float snapThreshold = 2.5f; // Degrees within which snapping occurs

    for (int i = 0; i < 4; i++) {
        // Draw speaker as a darker blue circle
        drawList->AddCircleFilled(speakerPositions[i], 10.0f, IM_COL32(34, 52, 78, 255)); // Dark blue color
        drawList->AddText(ImVec2(speakerPositions[i].x - 6, speakerPositions[i].y - 6), IM_COL32(255, 255, 255, 255), speakerLabels[i]);

        // Handle dragging
        ImGui::SetCursorScreenPos(ImVec2(speakerPositions[i].x - 10, speakerPositions[i].y - 10));
        ImGui::InvisibleButton(speakerLabels[i], ImVec2(20, 20));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            ImVec2 newPos = ImVec2(speakerPositions[i].x + mouseDelta.x, speakerPositions[i].y + mouseDelta.y);

            // Constrain position to the circle
            ImVec2 direction = ImVec2(newPos.x - center.x, newPos.y - center.y);
            float length = sqrtf(direction.x * direction.x + direction.y * direction.y);
            ImVec2 constrainedPos = ImVec2(center.x + (direction.x / length) * radius, center.y + (direction.y / length) * radius);

            // Calculate angle of the constrained position
            float angle = atan2f(constrainedPos.y - center.y, constrainedPos.x - center.x) * (180.0f / M_PI);
            if (angle < 0) angle += 360.0f;

            // Snap to the nearest 22.5-degree marker if within the snap threshold
            float snappedAngle = roundf(angle / 22.5f) * 22.5f;
            if (fabsf(snappedAngle - angle) <= snapThreshold) {
                float rad = snappedAngle * (M_PI / 180.0f);
                constrainedPos = ImVec2(center.x + radius * cosf(rad), center.y + radius * sinf(rad));
            }

            speakerPositions[i] = constrainedPos;
            ImGui::ResetMouseDragDelta();

            // Save the updated angle to CVar after dragging
            float updatedAngle = atan2f(speakerPositions[i].y - center.y, speakerPositions[i].x - center.x) * (180.0f / M_PI);
            if (updatedAngle < 0) updatedAngle += 360.0f;
            CVarSetInteger(cvarNames[i], static_cast<int>(updatedAngle));
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame(); // Mark for saving
        }

        // Calculate angle and save to CVar
        float angle = atan2f(speakerPositions[i].y - center.y, speakerPositions[i].x - center.x) * (180.0f / M_PI);
        if (angle < 0) angle += 360.0f;
        CVarSetInteger(cvarNames[i], static_cast<int>(angle));
    }

    // Reset cursor position for button placement
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y + 10));
    if (ImGui::Button("Reset Positions")) {
        float defaultAngles[4] = { 240.f, 300.f, 160.f, 20.f };
        for (int i = 0; i < 4; i++) {
            float rad = defaultAngles[i] * (M_PI / 180.0f);
            speakerPositions[i] = ImVec2(center.x + radius * cosf(rad), center.y + radius * sinf(rad));
            CVarSetInteger(cvarNames[i], static_cast<int>(defaultAngles[i]));
        }
        Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }

    // Reset cursor position to ensure canvas size remains static
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y + 10));
}

void DrawSettingsMenu(){
    if(UIWidgets::BeginMenu("Settings")){
        if (UIWidgets::BeginMenu("Audio")) {
            UIWidgets::CVarSliderFloat("Master Volume", "gGameMasterVolume", 0.0f, 1.0f, 1.0f, {
                .format = "%.0f%%",
                .isPercentage = true,
            });
            if (UIWidgets::CVarSliderFloat("Main Music Volume", "gMainMusicVolume", 0.0f, 1.0f, 1.0f, {
                .format = "%.0f%%",
                .isPercentage = true,
            })) {
                float val = CVarGetFloat("gMainMusicVolume", 1.0f) * 100;
                gSaveFile.save.data.musicVolume = (u8) val;
                Audio_SetVolume(AUDIO_TYPE_MUSIC, (u8) val);
            }
            if (UIWidgets::CVarSliderFloat("Voice Volume", "gVoiceVolume", 0.0f, 1.0f, 1.0f, {
                .format = "%.0f%%",
                .isPercentage = true,
            })) {
                float val = CVarGetFloat("gVoiceVolume", 1.0f) * 100;
                gSaveFile.save.data.voiceVolume = (u8) val;
                Audio_SetVolume(AUDIO_TYPE_VOICE, (u8) val);
            }
            if (UIWidgets::CVarSliderFloat("Sound Effects Volume", "gSFXMusicVolume", 0.0f, 1.0f, 1.0f, {
                .format = "%.0f%%",
                .isPercentage = true,
            })) {
                float val = CVarGetFloat("gSFXMusicVolume", 1.0f) * 100;
                gSaveFile.save.data.sfxVolume = (u8) val;
                Audio_SetVolume(AUDIO_TYPE_SFX, (u8) val);
            }

            static std::unordered_map<Ship::AudioBackend, const char*> audioBackendNames = {
                    { Ship::AudioBackend::WASAPI, "Windows Audio Session API" },
                    { Ship::AudioBackend::SDL, "SDL" },
            };

            ImGui::Text("Audio API (Needs reload)");
            auto currentAudioBackend = Ship::Context::GetInstance()->GetAudio()->GetCurrentAudioBackend();

            if (Ship::Context::GetInstance()->GetAudio()->GetAvailableAudioBackends()->size() <= 1) {
                UIWidgets::DisableComponent(ImGui::GetStyle().Alpha * 0.5f);
            }
            if (ImGui::BeginCombo("##AApi", audioBackendNames[currentAudioBackend])) {
                for (uint8_t i = 0; i < Ship::Context::GetInstance()->GetAudio()->GetAvailableAudioBackends()->size(); i++) {
                    auto backend = Ship::Context::GetInstance()->GetAudio()->GetAvailableAudioBackends()->data()[i];
                    if (ImGui::Selectable(audioBackendNames[backend], backend == currentAudioBackend)) {
                        Ship::Context::GetInstance()->GetAudio()->SetCurrentAudioBackend(backend);
                    }
                }
                ImGui::EndCombo();
            }
            if (Ship::Context::GetInstance()->GetAudio()->GetAvailableAudioBackends()->size() <= 1) {
                UIWidgets::ReEnableComponent("");
            }
            
            UIWidgets::PaddedEnhancementCheckbox("Surround 5.1 (Needs reload)", "gAudioChannelsSetting", 1, 0);
            
            if (CVarGetInteger("gAudioChannelsSetting", 0) == 1) {
                // Subwoofer threshold
                UIWidgets::CVarSliderInt("Subwoofer threshold (Hz)", "gSubwooferThreshold", 10u, 1000u, 80u, {
                    .tooltip = "The threshold for the subwoofer to be activated. Any sound under this frequency will be played on the subwoofer.",
                    .format = "%d",
                });

                // Rear music volume slider
                UIWidgets::CVarSliderFloat("Rear music volume", "gVolumeRearMusic", 0.0f, 1.0f, 1.0f, {
                    .format = "%.0f%%",
                    .isPercentage = true,
                });

                // Configurable positioning of speakers
                DrawSpeakerPositionEditor();
            }

            ImGui::EndMenu();
        }
        
        if (!GameEngine::HasVersion(SF64_VER_JP) || GameEngine::HasVersion(SF64_VER_EU)) {
            UIWidgets::Spacer(0);
            if (UIWidgets::BeginMenu("Language")) {
                ImGui::Dummy(ImVec2(150, 0.0f));
                if (!GameEngine::HasVersion(SF64_VER_JP) && GameEngine::HasVersion(SF64_VER_EU)){
                    //UIWidgets::Spacer(0);
                    if (UIWidgets::CVarCombobox("Voices", "gVoiceLanguage", voiceLangs, 
                    {
                        .tooltip = "Changes the language of the voice acting in the game",
                        .defaultIndex = 0,
                    })) {
                        Audio_SetVoiceLanguage(CVarGetInteger("gVoiceLanguage", 0));
                    };
                } else {
                    if (UIWidgets::Button("Install JP/EU Audio")) {
                        if (GameEngine::GenAssetFile(false)){
                            GameEngine::ShowMessage("Success", "Audio assets installed. Changes will be applied on the next startup.", SDL_MESSAGEBOX_INFORMATION);
                        }
                        Ship::Context::GetInstance()->GetWindow()->Close();
                    }
                }
                ImGui::EndMenu();
            }
        }
        
        UIWidgets::Spacer(0);

        if (UIWidgets::BeginMenu("Controller")) {
            UIWidgets::WindowButton("Controller Mapping", "gInputEditorWindow", GameUI::mInputEditorWindow);

            UIWidgets::Spacer(0);

            UIWidgets::CVarCheckbox("Menubar Controller Navigation", "gControlNav", {
                .tooltip = "Allows controller navigation of the SOH menu bar (Settings, Enhancements,...)\nCAUTION: This will disable game inputs while the menubar is visible.\n\nD-pad to move between items, A to select, and X to grab focus on the menu bar"
            });

            UIWidgets::CVarCheckbox("Invert Y Axis", "gInvertYAxis",{
                .tooltip = "Inverts the Y axis for controlling vehicles"
            });

            ImGui::EndMenu();
        }

#ifdef __IOS__
        UIWidgets::Spacer(0);

        if (UIWidgets::BeginMenu("Mobile Controls")) {
            ImGui::Text("Touch Controls");
            UIWidgets::CVarCheckbox("Show Touch Controls", "gShowTouchControls", {
                .tooltip = "Shows or hides the on-screen touch controls",
                .defaultValue = true
            });

            UIWidgets::Spacer(0);
            ImGui::Separator();
            UIWidgets::Spacer(0);

            ImGui::Text("Gyro Controls");
            UIWidgets::CVarCheckbox("Enable Gyro Aiming", "gGyroEnabled", {
                .tooltip = "Enable or disable gyroscope aiming controls",
                .defaultValue = true
            });

            UIWidgets::CVarSliderFloat("Gyro Sensitivity", "gGyroSensitivity", 5.0f, 40.0f, 20.0f, {
                .tooltip = "Degrees of tilt needed for full stick deflection\n"
                          "Lower = more sensitive, Higher = less sensitive"
            });

            UIWidgets::CVarSliderFloat("Gyro Deadzone", "gGyroDeadzone", 0.0f, 5.0f, 0.5f, {
                .tooltip = "Deadzone in degrees - small movements below this threshold are ignored"
            });

            UIWidgets::CVarSliderFloat("Gyro Response Curve", "gGyroResponseCurve", 1.0f, 3.0f, 2.0f, {
                .tooltip = "Response curve for gyro controls\n"
                          "1.0 = Linear, 2.0 = Squared (progressive), 3.0 = Cubed"
            });

            UIWidgets::CVarCheckbox("Invert Gyro Pitch", "gGyroInvertPitch", {
                .tooltip = "Inverts the pitch (up/down) axis for gyro controls",
                .defaultValue = true
            });

            UIWidgets::CVarCheckbox("Invert Gyro Roll", "gGyroInvertRoll", {
                .tooltip = "Inverts the roll (left/right) axis for gyro controls",
                .defaultValue = true
            });

            if (UIWidgets::Button("Recalibrate Gyro")) {
                // TODO: Call recalibration function
                // [[MotionController sharedController] recalibrate];
            }
            UIWidgets::Tooltip("Sets your current device orientation as the neutral position");

            UIWidgets::Spacer(0);
            UIWidgets::CVarCheckbox("Show Gyro Debug Info", "gShowGyroDebug", {
                .tooltip = "Shows debug information for gyro controls",
                .defaultValue = false
            });

            ImGui::EndMenu();
        }
#endif

        ImGui::EndMenu();
    }

    ImGui::SetCursorPosY(0.0f);
    if (UIWidgets::BeginMenu("Graphics")) {
        UIWidgets::WindowButton("Resolution Editor", "gAdvancedResolutionEditorEnabled", GameUI::mAdvancedResolutionSettingsWindow);

        UIWidgets::Spacer(0);

        // Previously was running every frame, and nothing was setting it? Maybe a bad copy/paste?
        // Ship::Context::GetInstance()->GetWindow()->SetResolutionMultiplier(CVarGetFloat("gInternalResolution", 1));
        // UIWidgets::Tooltip("Multiplies your output resolution by the value inputted, as a more intensive but effective form of anti-aliasing");
#ifndef __WIIU__
        if (UIWidgets::CVarSliderInt("MSAA: %d", "gMSAAValue", 1, 8, 1, {
            .tooltip = "Activates multi-sample anti-aliasing when above 1x up to 8x for 8 samples for every pixel"
        })) {
            Ship::Context::GetInstance()->GetWindow()->SetMsaaLevel(CVarGetInteger("gMSAAValue", 1));
        }
#endif

        { // FPS Slider
            const int minFps = 30;
            static int maxFps = 360;
            int currentFps = 0;
        #ifdef __WIIU__
            UIWidgets::Spacer(0);
            // only support divisors of 60 on the Wii U
            if (currentFps > 60) {
                currentFps = 60;
            } else {
                currentFps = 60 / (60 / currentFps);
            }

            int fpsSlider = 1;
            if (currentFps == 30) {
                ImGui::Text("FPS: Original (30)");
            } else {
                ImGui::Text("FPS: %d", currentFps);
                if (currentFps == 30) {
                    fpsSlider = 2;
                } else { // currentFps == 60
                    fpsSlider = 3;
                }
            }
            if (CVarGetInteger("gMatchRefreshRate", 0)) {
                UIWidgets::DisableComponent(ImGui::GetStyle().Alpha * 0.5f);
            }

            if (ImGui::Button(" - ##WiiUFPS")) {
                fpsSlider--;
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 7.0f);

            UIWidgets::Spacer(0);

            ImGui::PushItemWidth(std::min((ImGui::GetContentRegionAvail().x - 60.0f), 260.0f));
            ImGui::SliderInt("##WiiUFPSSlider", &fpsSlider, 1, 3, "", ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 7.0f);
            if (ImGui::Button(" + ##WiiUFPS")) {
                fpsSlider++;
            }

            if (CVarGetInteger("gMatchRefreshRate", 0)) {
                UIWidgets::ReEnableComponent("");
            }
            if (fpsSlider > 3) {
                fpsSlider = 3;
            } else if (fpsSlider < 1) {
                fpsSlider = 1;
            }

            if (fpsSlider == 1) {
                currentFps = 20;
            } else if (fpsSlider == 2) {
                currentFps = 30;
            } else if (fpsSlider == 3) {
                currentFps = 60;
            }
            CVarSetInteger("gInterpolationFPS", currentFps);
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        #else
            bool matchingRefreshRate = CVarGetInteger("gMatchRefreshRate", 0);
            UIWidgets::CVarSliderInt((currentFps == 30) ? "FPS: Original (30)" : "FPS: %d", "gInterpolationFPS", minFps, maxFps, 60, {
                .disabled = matchingRefreshRate
            });
        #endif
            UIWidgets::Tooltip(
                "Uses Matrix Interpolation to create extra frames, resulting in smoother graphics. This is purely "
                "visual and does not impact game logic, execution of glitches etc.\n\n"
                "A higher target FPS than your monitor's refresh rate will waste resources, and might give a worse result."
            );
        } // END FPS Slider

        UIWidgets::PaddedEnhancementCheckbox("Match Refresh Rate", "gMatchRefreshRate", true, false);
        UIWidgets::Tooltip("Matches interpolation value to the refresh rate of your display.");

        if (Ship::Context::GetInstance()->GetWindow()->GetWindowBackend() == Ship::WindowBackend::FAST3D_DXGI_DX11) {
            UIWidgets::PaddedEnhancementCheckbox("Render parallelization","gRenderParallelization", true, false, {}, {}, {}, true);
            UIWidgets::Tooltip(
                "This setting allows the CPU to work on one frame while GPU works on the previous frame.\n"
                "Recommended if you can't reach the FPS you set, despite it being set below your refresh rate "
                "or if you notice other performance problems.\n"
                "Adds up to one frame of input lag under certain scenarios.");
        }
      
        UIWidgets::PaddedSeparator(true, true, 3.0f, 3.0f);

        static std::unordered_map<Ship::WindowBackend, const char*> windowBackendNames = {
                { Ship::WindowBackend::FAST3D_DXGI_DX11, "DirectX" },
                { Ship::WindowBackend::FAST3D_SDL_OPENGL, "OpenGL"},
                { Ship::WindowBackend::FAST3D_SDL_METAL, "Metal" }
        };

        ImGui::Text("Renderer API (Needs reload)");
        Ship::WindowBackend runningWindowBackend = Ship::Context::GetInstance()->GetWindow()->GetWindowBackend();
        Ship::WindowBackend configWindowBackend;
        int configWindowBackendId = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Backend.Id", -1);
        if (configWindowBackendId != -1 && configWindowBackendId < static_cast<int>(Ship::WindowBackend::WINDOW_BACKEND_COUNT)) {
            configWindowBackend = static_cast<Ship::WindowBackend>(configWindowBackendId);
        } else {
            configWindowBackend = runningWindowBackend;
        }

        if (Ship::Context::GetInstance()->GetWindow()->GetAvailableWindowBackends()->size() <= 1) {
            UIWidgets::DisableComponent(ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::BeginCombo("##RApi", windowBackendNames[configWindowBackend])) {
            for (size_t i = 0; i < Ship::Context::GetInstance()->GetWindow()->GetAvailableWindowBackends()->size(); i++) {
                auto backend = Ship::Context::GetInstance()->GetWindow()->GetAvailableWindowBackends()->data()[i];
                if (ImGui::Selectable(windowBackendNames[backend], backend == configWindowBackend)) {
                    Ship::Context::GetInstance()->GetConfig()->SetInt("Window.Backend.Id", static_cast<int>(backend));
                    Ship::Context::GetInstance()->GetConfig()->SetString("Window.Backend.Name",
                                                                        windowBackendNames[backend]);
                    Ship::Context::GetInstance()->GetConfig()->Save();
                }
            }
            ImGui::EndCombo();
        }
        if (Ship::Context::GetInstance()->GetWindow()->GetAvailableWindowBackends()->size() <= 1) {
            UIWidgets::ReEnableComponent("");
        }

        if (Ship::Context::GetInstance()->GetWindow()->CanDisableVerticalSync()) {
            UIWidgets::PaddedEnhancementCheckbox("Enable Vsync", "gVsyncEnabled", true, false, false, "", UIWidgets::CheckboxGraphics::Cross, true);
            UIWidgets::Tooltip("Removes tearing, but clamps your max FPS to your displays refresh rate.");
        }

        if (Ship::Context::GetInstance()->GetWindow()->SupportsWindowedFullscreen()) {
            UIWidgets::PaddedEnhancementCheckbox("Windowed fullscreen", "gSdlWindowedFullscreen", true, false);
        }

        if (Ship::Context::GetInstance()->GetWindow()->GetGui()->SupportsViewports()) {
            UIWidgets::PaddedEnhancementCheckbox("Allow multi-windows", "gEnableMultiViewports", true, false, false, "", UIWidgets::CheckboxGraphics::Cross, true);
            UIWidgets::Tooltip("Allows windows to be able to be dragged off of the main game window. Requires a reload to take effect.");
        }

        UIWidgets::PaddedEnhancementCheckbox("Enable Alternative Assets", "gEnhancements.Mods.AlternateAssets");
        // If more filters are added to LUS, make sure to add them to the filters list here
        ImGui::Text("Texture Filter (Needs reload)");
        UIWidgets::EnhancementCombobox("gTextureFilter", filters, 0);

        UIWidgets::PaddedEnhancementCheckbox("Apply Point Filtering to UI Elements", "gHUDPointFiltering", true, false, false, "", UIWidgets::CheckboxGraphics::Cross, true);
        UIWidgets::Spacer(0);

        Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGameOverlay()->DrawSettings();

        ImGui::EndMenu();
    }
}

void DrawMenuBarIcon() {
    static bool gameIconLoaded = false;
    if (!gameIconLoaded) {
        // Ship::Context::GetInstance()->GetWindow()->GetGui()->LoadGuiTexture("Game_Icon", "textures/icons/gIcon.png", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        gameIconLoaded = false;
    }

    if (Ship::Context::GetInstance()->GetWindow()->GetGui()->GetTextureByName("Game_Icon")) {
#ifdef __SWITCH__
        ImVec2 iconSize = ImVec2(20.0f, 20.0f);
        float posScale = 1.0f;
#elif defined(__WIIU__)
        ImVec2 iconSize = ImVec2(16.0f * 2, 16.0f * 2);
        float posScale = 2.0f;
#else
        ImVec2 iconSize = ImVec2(20.0f, 20.0f);
        float posScale = 1.5f;
#endif
        ImGui::SetCursorPos(ImVec2(5, 2.5f) * posScale);
        ImGui::Image(Ship::Context::GetInstance()->GetWindow()->GetGui()->GetTextureByName("Game_Icon"), iconSize);
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(25, 0) * posScale);
    }
}

void DrawGameMenu() {
    if (UIWidgets::BeginMenu("Starship")) {
        if (UIWidgets::MenuItem("Reset", "F4")) {
            gNextGameState = GSTATE_BOOT;
        }
#if !defined(__SWITCH__) && !defined(__WIIU__)

        if (UIWidgets::MenuItem("Toggle Fullscreen", "F11")) {
            Ship::Context::GetInstance()->GetWindow()->ToggleFullscreen();
        }
#endif

        if (UIWidgets::MenuItem("Quit")) {
            Ship::Context::GetInstance()->GetWindow()->Close();
        }
        ImGui::EndMenu();
    }
}

static const char* hudAspects[] = {
    "Expand", "Custom", "Original (4:3)", "Widescreen (16:9)", "Nintendo 3DS (5:3)", "16:10 (8:5)", "Ultrawide (21:9)"
};

void DrawEnhancementsMenu() {
    if (UIWidgets::BeginMenu("Enhancements")) {

        if (UIWidgets::BeginMenu("Gameplay")) {
            UIWidgets::CVarCheckbox("No Level of Detail (LOD)", "gDisableLOD", {
                .tooltip = "Disable Level of Detail (LOD) to avoid models using lower poly versions at a distance",
                .defaultValue = true
            });
            UIWidgets::CVarCheckbox("Character heads inside Arwings at all times", "gTeamFaces", {
                .tooltip = "Character heads are displayed inside Arwings in all cutscenes",
                .defaultValue = true
            });
            UIWidgets::CVarCheckbox("Use red radio backgrounds for enemies.", "gEnemyRedRadio");
            UIWidgets::CVarSliderInt("Cockpit Glass Opacity: %d", "gCockpitOpacity", 0, 255, 120);
            

            ImGui::EndMenu();
        }
        
        if (UIWidgets::BeginMenu("Fixes")) {
            UIWidgets::CVarCheckbox("Macbeth: Level ending cutscene camera fix", "gMaCameraFix", {
                .tooltip = "Fixes a camera bug found in the code of the game"
            });

            UIWidgets::CVarCheckbox("Sector Z: Spawn all actors", "gSzActorFix", {
                .tooltip = "Fixes a bug found in Sector Z, where only 10 of 12 available actors are spawned, this causes two 'Space Junk Boxes' to be missing from the level."
            });

            ImGui::EndMenu();
        }

        if (UIWidgets::BeginMenu("Restoration")) {
            UIWidgets::CVarCheckbox("Sector Z: Missile cutscene bug", "gSzMissileBug", {
                .tooltip = "Restores the missile cutscene bug present in JP 1.0"
            });

            UIWidgets::CVarCheckbox("Beta: Restore beta coin", "gRestoreBetaCoin", {
                .tooltip = "Restores the beta coin that got replaced with the gold ring"
            });

            UIWidgets::CVarCheckbox("Beta: Restore beta boost/brake gauge", "gRestoreBetaBoostGauge", {
                .tooltip = "Restores the beta boost gauge that was seen in some beta footage"
            });

            ImGui::EndMenu();
        }

        if (UIWidgets::BeginMenu("HUD")) {
            if (UIWidgets::CVarCombobox("HUD Aspect Ratio", "gHUDAspectRatio.Selection", hudAspects, 
            {
                .tooltip = "Which Aspect Ratio to use when drawing the HUD (Radar, gauges and radio messages)",
                .defaultIndex = 0,
            })) {
                CVarSetInteger("gHUDAspectRatio.Enabled", 1);
                switch (CVarGetInteger("gHUDAspectRatio.Selection", 0)) {
                    case 0:
                        CVarSetInteger("gHUDAspectRatio.Enabled", 0);
                        CVarSetInteger("gHUDAspectRatio.X", 0);
                        CVarSetInteger("gHUDAspectRatio.Y", 0);
                        break;
                    case 1:
                        if (CVarGetInteger("gHUDAspectRatio.X", 0) <= 0){
                            CVarSetInteger("gHUDAspectRatio.X", 1);
                        }
                        if (CVarGetInteger("gHUDAspectRatio.Y", 0) <= 0){
                            CVarSetInteger("gHUDAspectRatio.Y", 1);
                        }
                        break;
                    case 2:
                        CVarSetInteger("gHUDAspectRatio.X", 4);
                        CVarSetInteger("gHUDAspectRatio.Y", 3);
                        break;
                    case 3:
                        CVarSetInteger("gHUDAspectRatio.X", 16);
                        CVarSetInteger("gHUDAspectRatio.Y", 9);
                        break;
                    case 4:
                        CVarSetInteger("gHUDAspectRatio.X", 5);
                        CVarSetInteger("gHUDAspectRatio.Y", 3);
                        break;
                    case 5:
                        CVarSetInteger("gHUDAspectRatio.X", 8);
                        CVarSetInteger("gHUDAspectRatio.Y", 5);
                        break;
                    case 6:
                        CVarSetInteger("gHUDAspectRatio.X", 21);
                        CVarSetInteger("gHUDAspectRatio.Y", 9);
                        break;                    
                }
            }
            
            if (CVarGetInteger("gHUDAspectRatio.Selection", 0) == 1)
            {
                UIWidgets::CVarSliderInt("Horizontal: %d", "gHUDAspectRatio.X", 1, 100, 1);
                UIWidgets::CVarSliderInt("Vertical: %d", "gHUDAspectRatio.Y", 1, 100, 1);
            }

            ImGui::Dummy(ImVec2(ImGui::CalcTextSize("Nintendo 3DS (5:3)").x + 35, 0.0f));
            ImGui::EndMenu();
        }

        if (UIWidgets::BeginMenu("Accessibility")) { 
            UIWidgets::CVarCheckbox("Disable Gorgon (Area 6 boss) screen flashes", "gDisableGorgonFlash", {
                .tooltip = "Gorgon flashes the screen repeatedly when firing its beam or when teleporting, which causes eye pain for some players and may be harmful to those with photosensitivity.",
                .defaultValue = false
            });
            UIWidgets::CVarCheckbox("Add outline to Arwing and Wolfen in radar", "gFighterOutlines", {
                .tooltip = "Increases visibility of ships in the radar.",
                .defaultValue = false
            });
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

void DrawCheatsMenu() {
    if (UIWidgets::BeginMenu("Cheats")) {
        UIWidgets::CVarCheckbox("Infinite Lives", "gInfiniteLives");
        UIWidgets::CVarCheckbox("Invincible", "gInvincible");
        UIWidgets::CVarCheckbox("Unbreakable Wings", "gUnbreakableWings");
        UIWidgets::CVarCheckbox("Infinite Bombs", "gInfiniteBombs");
        UIWidgets::CVarCheckbox("Infinite Boost/Brake", "gInfiniteBoost");
        UIWidgets::CVarCheckbox("Hyper Laser", "gHyperLaser");
        UIWidgets::CVarSliderInt("Laser Range Multiplier: %d%%", "gLaserRangeMult", 15, 800, 100,
            { .tooltip = "Changes how far your lasers fly." });
        UIWidgets::CVarCheckbox("Rapid-fire mode", "gRapidFire", {
                .tooltip = "Hold A to keep firing. Release A to start charging a shot."
            });
            if (CVarGetInteger("gRapidFire", 0) == 1) {
                ImGui::Dummy(ImVec2(22.0f, 0.0f));
                ImGui::SameLine();
                UIWidgets::CVarCheckbox("Hold L to Charge", "gLtoCharge", {
                    .tooltip = "If you prefer to not have auto-charge."
                });
            }
        UIWidgets::CVarCheckbox("Self destruct button", "gHit64SelfDestruct", {
                .tooltip = "Press Down on the D-PAD to instantly self destruct."
            });
        UIWidgets::CVarCheckbox("Start with Falco dead", "gHit64FalcoDead", {
                .tooltip = "Start the level with with Falco dead."
            });
        UIWidgets::CVarCheckbox("Start with Slippy dead", "gHit64SlippyDead", {
                .tooltip = "Start the level with with Slippy dead."
            });
        UIWidgets::CVarCheckbox("Start with Peppy dead", "gHit64PeppyDead", {
                .tooltip = "Start the level with with Peppy dead."
            });
        
        UIWidgets::CVarCheckbox("Score Editor", "gScoreEditor", { .tooltip = "Enable the score editor" });

        if (CVarGetInteger("gScoreEditor", 0) == 1) {
            UIWidgets::CVarSliderInt("Score: %d", "gScoreEditValue", 0, 999, 0,
                { .tooltip = "Increase or decrease the current mission score number" });
        }

        ImGui::EndMenu();
    }
}

static const char* debugInfoPages[6] = {
    "Object",
    "Check Surface",
    "Map",
    "Stage",
    "Effect",
    "Enemy",
};

static const char* logLevels[] = {
    "trace", "debug", "info", "warn", "error", "critical", "off",
};

void DrawDebugMenu() {
    if (UIWidgets::BeginMenu("Developer")) {
        if (UIWidgets::CVarCombobox("Log Level", "gDeveloperTools.LogLevel", logLevels, {
            .tooltip = "The log level determines which messages are printed to the "
                        "console. This does not affect the log file output",
            .defaultIndex = 1,
        })) {
            Ship::Context::GetInstance()->GetLogger()->set_level((spdlog::level::level_enum)CVarGetInteger("gDeveloperTools.LogLevel", 1));
        }

#ifdef __SWITCH__
        if (UIWidgets::CVarCombobox("Switch CPU Profile", "gSwitchPerfMode", SWITCH_CPU_PROFILES, {
            .tooltip = "Switches the CPU profile to a different one",
            .defaultIndex = (int)Ship::SwitchProfiles::STOCK
        })) {
            SPDLOG_INFO("Profile:: %s", SWITCH_CPU_PROFILES[CVarGetInteger("gSwitchPerfMode", (int)Ship::SwitchProfiles::STOCK)]);
            Ship::Switch::ApplyOverclock();
        }
#endif

        UIWidgets::WindowButton("Gfx Debugger", "gGfxDebuggerEnabled", GameUI::mGfxDebuggerWindow, {
            .tooltip = "Enables the Gfx Debugger window, allowing you to input commands, type help for some examples"
        });

        // UIWidgets::CVarCheckbox("Debug mode", "gEnableDebugMode", {
        //     .tooltip = "TBD"
        // });

        UIWidgets::CVarCheckbox("Level Selector", "gLevelSelector", {
            .tooltip = "Allows you to select any level from the main menu"
        });

        UIWidgets::CVarCheckbox("Skip Briefing", "gSkipBriefing", {
            .tooltip = "Allows you to skip the briefing sequence in level select"
        });

        UIWidgets::CVarCheckbox("Enable Expert Mode", "gForceExpertMode", {
            .tooltip = "Allows you to force expert mode"
        });

        UIWidgets::CVarCheckbox("SFX Jukebox", "gSfxJukebox", {
            .tooltip = "Press L in the Expert Sound options to play sound effects from the game"
        });

        UIWidgets::CVarCheckbox("Disable Starfield interpolation", "gDisableStarsInterpolation", {
            .tooltip = "Disable starfield interpolation to increase performance on slower CPUs"
        });
        UIWidgets::CVarCheckbox("Disable Gamma Boost (Needs reload)", "gGraphics.GammaMode", {
            .tooltip = "Disables the game's Built-in Gamma Boost. Useful for modders",
            .defaultValue = false
        });

        UIWidgets::CVarCheckbox("Spawner Mod", "gSpawnerMod", {
            .tooltip = "Spawn Scenery, Actors, Bosses, Sprites, Items, Effects and even Event Actors.\n"
                       "\n"
                       "Controls:\n"
                       "D-Pad left and right to set the object Id.\n"
                       "C-Right to change between spawn modes.\n"
                       "Analog stick sets the spawn position.\n"
                       "L-Trigger to spawn the object.\n"
                       "D-Pad UP to kill all objects.\n"
                       "D-Pad DOWN to freeze/unfreeze the ship speed.\n"
                       "WARNING: Spawning an object that's not loaded in memory will likely result in a crash."
        });

        UIWidgets::CVarCheckbox("Jump To Map", "gDebugJumpToMap", {
            .tooltip = "Press Z + R + C-UP to get back to the map"
        });

        UIWidgets::CVarCheckbox("L To Warp Zone", "gDebugWarpZone", {
            .tooltip = "Press L to get into the Warp Zone"
        });

        UIWidgets::CVarCheckbox("L to Level Complete", "gDebugLevelComplete", {
            .tooltip = "Press L to Level Complete"
        });

        UIWidgets::CVarCheckbox("L to All-Range mode", "gDebugJumpToAllRange", {
            .tooltip = "Press L to switch to All-Range mode"
        });

        UIWidgets::CVarCheckbox("Disable Collision", "gDebugNoCollision", {
            .tooltip = "Disable vehicle collision"
        });
        
        UIWidgets::CVarCheckbox("Speed Control", "gDebugSpeedControl", {
            .tooltip = "Arwing speed control. Use D-PAD Left and Right to Increase/Decrease the Arwing Speed, D-PAD Down to stop movement."
        });

        UIWidgets::CVarCheckbox("Debug Ending", "gDebugEnding", {
            .tooltip = "Jump to credits at the main menu"
        });

        UIWidgets::CVarCheckbox("Debug Pause", "gLToDebugPause", {
            .tooltip = "Press L to toggle Debug Pause"
        });
        if (CVarGetInteger("gLToDebugPause", 0)) {
            ImGui::Dummy(ImVec2(22.0f, 0.0f));
            ImGui::SameLine();
            UIWidgets::CVarCheckbox("Frame Advance", "gLToFrameAdvance", {
            .tooltip = "Pressing L again advances one frame instead"
        });
        }

        if (CVarGetInteger(StringHelper::Sprintf("gCheckpoint.%d.Set", gCurrentLevel).c_str(), 0)) {
            if (UIWidgets::Button("Clear Checkpoint")) {
                CVarClear(StringHelper::Sprintf("gCheckpoint.%d.Set", gCurrentLevel).c_str());
                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
        } else if (gPlayer != NULL && gGameState == GSTATE_PLAY) {
            if (UIWidgets::Button("Set Checkpoint")) {
                CVarSetInteger(StringHelper::Sprintf("gCheckpoint.%d.Set", gCurrentLevel).c_str(), 1);
                CVarSetInteger(StringHelper::Sprintf("gCheckpoint.%d.gSavedPathProgress", gCurrentLevel).c_str(), gGroundSurface);
                CVarSetFloat(StringHelper::Sprintf("gCheckpoint.%d.gSavedPathProgress", gCurrentLevel).c_str(), (-gPlayer->pos.z) - 250.0f);
                CVarSetInteger(StringHelper::Sprintf("gCheckpoint.%d.gSavedObjectLoadIndex", gCurrentLevel).c_str(), gObjectLoadIndex);
                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
        }

        UIWidgets::Spacer(0);

        UIWidgets::WindowButton("Stats", "gStatsEnabled", GameUI::mStatsWindow, {
            .tooltip = "Shows the stats window, with your FPS and frametimes, and the OS you're playing on"
        });
        UIWidgets::WindowButton("Console", "gConsoleEnabled", GameUI::mConsoleWindow, {
            .tooltip = "Enables the console window, allowing you to input commands, type help for some examples"
        });

        ImGui::EndMenu();
    }
}

void GameMenuBar::DrawElement() {
#ifdef __IOS__
    // On iOS, use a touch-friendly window with tree nodes instead of menu bar
    // Menu bars are designed for desktop and don't work well with touch
    // Note: iOS_SetMenuOpen is called from GuiMenuBar::SetVisibility()
    // This method is only called when IsVisible() is true (see GuiMenuBar::Draw)

    ImGuiIO& io = ImGui::GetIO();

    // Center the window on screen
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.85f, io.DisplaySize.y * 0.8f), ImGuiCond_Always);

    // Set solid background color for iOS menu
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    // Use a regular window with scroll support - works better with touch than menu bars
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    // Ensure window receives input focus for touch interaction
    ImGui::SetNextWindowFocus();

    bool windowOpen = true;
    if (ImGui::Begin("iOS Settings Menu", &windowOpen, windowFlags)) {
        // CRITICAL: Force this window to be the focused window for input
        ImGui::SetWindowFocus();
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        // Track touch-down position for scroll gesture detection (independent of buttons)
        // Use manual state tracking instead of io.MouseClicked which may not be reliable
        bool isTouchingNow = io.MouseDown[0];
        bool touchJustStarted = isTouchingNow && !g_wasTouchingLastFrame;
        bool touchJustEnded = !isTouchingNow && g_wasTouchingLastFrame;

        if (touchJustStarted) {
            // Touch just started - record position for scroll detection
            g_scrollTouchDownPos = io.MousePos;
            SPDLOG_INFO("[iOS Scroll] Touch-down detected at ({:.0f}, {:.0f})", io.MousePos.x, io.MousePos.y);
        }
        if (touchJustEnded) {
            SPDLOG_INFO("[iOS Scroll] Touch-up detected at ({:.0f}, {:.0f}), down was ({:.0f}, {:.0f})",
                io.MousePos.x, io.MousePos.y, g_scrollTouchDownPos.x, g_scrollTouchDownPos.y);
        }
        g_wasTouchingLastFrame = isTouchingNow;

        // ========== VERTICAL SIDEBAR NAVIGATION ==========
        // Vertical tabs on left, content on right - optimal for landscape mode
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

        float windowWidth = ImGui::GetContentRegionAvail().x;
        float windowHeight = ImGui::GetContentRegionAvail().y;

        // Sidebar dimensions
        float sidebarWidth = 110.0f;
        float contentWidth = windowWidth - sidebarWidth - 10.0f;  // Gap between sidebar and content
        float tabHeight = 45.0f;

        const char* tabNames[] = { "Game", "Settings", "Graphics", "Cheats", "Dev", "Mods" };
        const int numTabs = 6;

        // Left sidebar with vertical tabs
        ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, windowHeight), false, ImGuiWindowFlags_NoScrollbar);

        for (int i = 0; i < numTabs; i++) {
            // Highlight selected tab
            bool isSelected = (mOpenSection == i);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
            }

            if (TouchFriendlyButton(tabNames[i], ImVec2(sidebarWidth - 8.0f, tabHeight))) {
                mOpenSection = i;
            }

            ImGui::PopStyleColor(2);
        }

        // Spacer to push close button to bottom
        float remainingHeight = windowHeight - (numTabs * (tabHeight + 4.0f)) - tabHeight - 8.0f;
        if (remainingHeight > 0) {
            ImGui::Dummy(ImVec2(0, remainingHeight));
        }

        // Close button at bottom of sidebar - styled red
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        if (TouchFriendlyButton("X Close", ImVec2(sidebarWidth - 8.0f, tabHeight))) {
            ToggleVisibility();
        }
        ImGui::PopStyleColor(2);

        ImGui::EndChild();

        // Content area on the right
        ImGui::SameLine(0, 10.0f);
        ImGui::BeginChild("TabContent", ImVec2(contentWidth, windowHeight), false, ImGuiWindowFlags_None);

        float buttonWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 buttonSize(buttonWidth, 50.0f);

        // Draw content based on selected tab
        switch (mOpenSection) {
            case 0:  // Game tab
                TouchFriendlySectionHeader("Game");

                if (TouchFriendlyButton("Reset Game", buttonSize)) {
                    gNextGameState = GSTATE_BOOT;
                    ToggleVisibility();  // Close menu after reset
                }
                ImGui::Spacing();

                if (TouchFriendlyButton("Quit Game", buttonSize)) {
                    Ship::Context::GetInstance()->GetWindow()->Close();
                }

                TouchFriendlySectionHeader("Gameplay");

                TouchFriendlyCheckbox("Invert Y Axis", "gInvertYAxis");
                break;

            case 1:  // Settings tab
                TouchFriendlySectionHeader("Audio");

                TouchFriendlySliderFloat("Master Volume", "gGameMasterVolume", 0.0f, 1.0f, 1.0f, 0.05f);
                TouchFriendlySliderFloat("Music Volume", "gMainMusicVolume", 0.0f, 1.0f, 1.0f, 0.05f);
                TouchFriendlySliderFloat("Voice Volume", "gVoiceVolume", 0.0f, 1.0f, 1.0f, 0.05f);
                TouchFriendlySliderFloat("SFX Volume", "gSFXMusicVolume", 0.0f, 1.0f, 1.0f, 0.05f);

                TouchFriendlySectionHeader("Touch Controls");

                TouchFriendlyCheckbox("Show Touch Controls", "gShowTouchControls", true);

                TouchFriendlySectionHeader("Gyro Controls");

                TouchFriendlyCheckbox("Enable Gyro Aiming", "gGyroEnabled", true);
                TouchFriendlySliderFloat("Gyro Sensitivity", "gGyroSensitivity", 5.0f, 40.0f, 20.0f, 2.5f);
                TouchFriendlySliderFloat("Gyro Deadzone", "gGyroDeadzone", 0.0f, 5.0f, 0.5f, 0.25f);
                TouchFriendlyCheckbox("Invert Gyro Pitch", "gGyroInvertPitch", true);
                TouchFriendlyCheckbox("Invert Gyro Roll", "gGyroInvertRoll", true);
                break;

            case 2:  // Graphics tab
                TouchFriendlySectionHeader("Performance");

                // FPS Target (30-120 for mobile, higher values waste battery)
                if (TouchFriendlySliderInt("FPS Target", "gInterpolationFPS", 30, 120, 60, 10)) {
                    // FPS changed - no additional action needed, engine reads this
                }
                TouchFriendlyCheckbox("Match Refresh Rate", "gMatchRefreshRate");

                // MSAA Anti-aliasing
                if (TouchFriendlySliderInt("Anti-Aliasing (MSAA)", "gMSAAValue", 1, 4, 1, 1)) {
                    Ship::Context::GetInstance()->GetWindow()->SetMsaaLevel(CVarGetInteger("gMSAAValue", 1));
                }

                TouchFriendlySectionHeader("Enhancements");

                TouchFriendlyCheckbox("No LOD (Better models)", "gDisableLOD", true);
                TouchFriendlyCheckbox("Show faces in Arwings", "gTeamFaces", true);
                TouchFriendlyCheckbox("Red radio for enemies", "gEnemyRedRadio");
                TouchFriendlySliderInt("Cockpit Opacity", "gCockpitOpacity", 0, 255, 120, 15);
                TouchFriendlyCheckbox("Alternative Assets", "gEnhancements.Mods.AlternateAssets");

                TouchFriendlySectionHeader("Fixes");

                TouchFriendlyCheckbox("Macbeth camera fix", "gMaCameraFix");
                TouchFriendlyCheckbox("Sector Z actor fix", "gSzActorFix");

                TouchFriendlySectionHeader("Restoration");

                TouchFriendlyCheckbox("Restore beta coin", "gRestoreBetaCoin");
                TouchFriendlyCheckbox("Restore beta boost gauge", "gRestoreBetaBoostGauge");
                TouchFriendlyCheckbox("Sector Z missile bug", "gSzMissileBug");

                TouchFriendlySectionHeader("Accessibility");

                TouchFriendlyCheckbox("Disable Gorgon flash", "gDisableGorgonFlash");
                TouchFriendlyCheckbox("Radar ship outlines", "gFighterOutlines");
                break;

            case 3:  // Cheats tab
                TouchFriendlySectionHeader("Player Cheats");

                TouchFriendlyCheckbox("Infinite Lives", "gInfiniteLives");
                TouchFriendlyCheckbox("Invincible", "gInvincible");
                TouchFriendlyCheckbox("Unbreakable Wings", "gUnbreakableWings");
                TouchFriendlyCheckbox("Infinite Bombs", "gInfiniteBombs");
                TouchFriendlyCheckbox("Infinite Boost/Brake", "gInfiniteBoost");

                TouchFriendlySectionHeader("Weapon Cheats");

                TouchFriendlyCheckbox("Hyper Laser", "gHyperLaser");
                TouchFriendlyCheckbox("Rapid-fire mode", "gRapidFire");
                if (CVarGetInteger("gRapidFire", 0) == 1) {
                    TouchFriendlyCheckbox("  Hold L to Charge", "gLtoCharge");
                }
                TouchFriendlySliderInt("Laser Range %", "gLaserRangeMult", 15, 800, 100, 25);

                TouchFriendlySectionHeader("Team");

                TouchFriendlyCheckbox("Start with Falco dead", "gHit64FalcoDead");
                TouchFriendlyCheckbox("Start with Slippy dead", "gHit64SlippyDead");
                TouchFriendlyCheckbox("Start with Peppy dead", "gHit64PeppyDead");

                TouchFriendlySectionHeader("Misc");

                TouchFriendlyCheckbox("Self destruct (D-Pad Down)", "gHit64SelfDestruct");
                break;

            case 4:  // Dev tab
                TouchFriendlySectionHeader("Display");

                // Stats window toggle
                {
                    bool statsEnabled = CVarGetInteger("gStatsEnabled", 0) != 0;
                    if (TouchFriendlyCheckbox("Show FPS/Stats", "gStatsEnabled")) {
                        // Toggle the stats window visibility
                        if (GameUI::mStatsWindow) {
                            if (CVarGetInteger("gStatsEnabled", 0)) {
                                GameUI::mStatsWindow->Show();
                            } else {
                                GameUI::mStatsWindow->Hide();
                            }
                        }
                    }
                }
                TouchFriendlyCheckbox("Show Gyro Debug", "gShowGyroDebug");

                TouchFriendlySectionHeader("Developer");

                TouchFriendlyCheckbox("Level Selector", "gLevelSelector");
                TouchFriendlyCheckbox("Skip Briefing", "gSkipBriefing");
                TouchFriendlyCheckbox("Force Expert Mode", "gForceExpertMode");
                TouchFriendlyCheckbox("SFX Jukebox", "gSfxJukebox");

                TouchFriendlySectionHeader("Debug Modes");

                TouchFriendlyCheckbox("Speed Control (D-Pad)", "gDebugSpeedControl");
                TouchFriendlyCheckbox("No Collision", "gDebugNoCollision");
                TouchFriendlyCheckbox("Debug Pause (L)", "gLToDebugPause");
                if (CVarGetInteger("gLToDebugPause", 0)) {
                    TouchFriendlyCheckbox("  Frame Advance", "gLToFrameAdvance");
                }

                TouchFriendlySectionHeader("Shortcuts");

                TouchFriendlyCheckbox("L to Warp Zone", "gDebugWarpZone");
                TouchFriendlyCheckbox("L to All-Range", "gDebugJumpToAllRange");
                TouchFriendlyCheckbox("L to Level Complete", "gDebugLevelComplete");
                TouchFriendlyCheckbox("Jump To Map (Z+R+C-Up)", "gDebugJumpToMap");

                TouchFriendlySectionHeader("Tools");

                TouchFriendlyCheckbox("Spawner Mod", "gSpawnerMod");
                TouchFriendlyCheckbox("Disable Star Interpolation", "gDisableStarsInterpolation");
                break;

            case 5:  // Mods tab
                TouchFriendlySectionHeader("Installed Mods");
                {
                    // List O2R/ZIP files in mods directory
                    std::string modsPath = Ship::Context::GetPathRelativeToAppDirectory("mods");

                    if (std::filesystem::exists(modsPath) && std::filesystem::is_directory(modsPath)) {
                        int modCount = 0;
                        for (const auto& entry : std::filesystem::directory_iterator(modsPath)) {
                            if (entry.is_regular_file()) {
                                std::string ext = entry.path().extension().string();
                                // Convert to lowercase for comparison
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                                if (ext == ".o2r" || ext == ".zip") {
                                    std::string modName = entry.path().filename().string();
                                    modCount++;

                                    // Display mod with file size
                                    auto fileSize = std::filesystem::file_size(entry.path());
                                    std::string sizeStr;
                                    if (fileSize < 1024) {
                                        sizeStr = std::to_string(fileSize) + " B";
                                    } else if (fileSize < 1024 * 1024) {
                                        sizeStr = std::to_string(fileSize / 1024) + " KB";
                                    } else {
                                        sizeStr = std::to_string(fileSize / (1024 * 1024)) + " MB";
                                    }

                                    // Display as a button row (read-only for now)
                                    char displayText[256];
                                    snprintf(displayText, sizeof(displayText), "%-25s %s", modName.c_str(), sizeStr.c_str());

                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.25f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.4f, 0.3f, 1.0f));
                                    TouchFriendlyButton(displayText, ImVec2(contentWidth - 20.0f, 50.0f));
                                    ImGui::PopStyleColor(2);
                                }
                            }
                        }

                        if (modCount == 0) {
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No mods found");
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Mods folder not found");
                    }
                }

                TouchFriendlySectionHeader("Add Mods");
                ImGui::TextWrapped("Place .o2r or .zip mod files in:");
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "Documents/mods/");
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Restart required after adding mods");
                break;

            default:
                mOpenSection = 0;  // Default to Game tab
                break;
        }

        // Touch-based scrolling for iOS - MUST be inside child region to affect its scroll
        // NOTE: SDL on iOS doesn't generate MOTION events during touch drag, only DOWN and UP.
        // So we calculate scroll delta at touch-up time based on the full gesture distance.
        float currentScrollY = ImGui::GetScrollY();
        float maxScrollY = ImGui::GetScrollMaxY();

        // Detect scroll gesture on touch release
        // A scroll gesture is when the finger moved more than TAP_TOLERANCE (50px)
        // Uses touchJustEnded computed at start of frame
        if (touchJustEnded && g_scrollTouchDownPos.x >= 0 && maxScrollY > 0) {
            float deltaY = g_scrollTouchDownPos.y - io.MousePos.y;  // Positive = finger moved up = scroll down
            float touchDistance = fabsf(deltaY);

            const float TAP_TOLERANCE = 50.0f;
            if (touchDistance >= TAP_TOLERANCE) {
                // This was a scroll gesture, not a tap
                // Apply the scroll delta (inverted: drag down = scroll up to show earlier content)
                float newScroll = currentScrollY + deltaY;
                newScroll = ImClamp(newScroll, 0.0f, maxScrollY);
                ImGui::SetScrollY(newScroll);
                g_scrollVelocity = deltaY * 0.3f;  // Initial momentum
                g_isDragging = true;
                SPDLOG_INFO("[iOS Scroll] Scroll gesture: deltaY={:.0f}, newScroll={:.0f}, maxScroll={:.0f}",
                    deltaY, newScroll, maxScrollY);
            }
        }

        // Apply momentum scrolling after gesture ends
        if (!io.MouseDown[0] && g_isDragging && fabsf(g_scrollVelocity) > 0.5f && maxScrollY > 0) {
            float newScroll = currentScrollY + g_scrollVelocity;
            newScroll = ImClamp(newScroll, 0.0f, maxScrollY);
            ImGui::SetScrollY(newScroll);
            g_scrollVelocity *= 0.92f;
        } else if (!io.MouseDown[0]) {
            g_scrollVelocity = 0.0f;
            g_isDragging = false;
        }

        ImGui::EndChild();  // End TabContent - scroll applies to this child

        // Reset scroll touch tracking on release (outside child since it's global state)
        if (touchJustEnded) {
            g_scrollTouchDownPos = ImVec2(-1, -1);
        }

        ImGui::PopStyleVar(2);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);

    // If window was closed via X button
    if (!windowOpen) {
        ToggleVisibility();
    }
#else
    // On desktop/other platforms, use the standard menu bar
    if(ImGui::BeginMenuBar()){
        DrawMenuBarIcon();

        DrawGameMenu();

        ImGui::SetCursorPosY(0.0f);

        DrawSettingsMenu();

        ImGui::SetCursorPosY(0.0f);

        DrawEnhancementsMenu();

        ImGui::SetCursorPosY(0.0f);

        DrawCheatsMenu();

        ImGui::SetCursorPosY(0.0f);

        ImGui::SetCursorPosY(0.0f);

        DrawDebugMenu();

        ImGui::EndMenuBar();
    }
#endif
}