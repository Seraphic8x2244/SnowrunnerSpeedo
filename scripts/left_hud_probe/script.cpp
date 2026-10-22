#include "sdk/script_api.h"

// Stage-one, read-only SnowLoader integration probe.
// This checks script loading and HUD overlay rendering only.
// There is NO native HUD binding here; do not mistake this marker for attachment.

class LeftHudProbe final : public IScript {
public:
    const char* GetName() const override { return "left_hud_probe"; }

    bool Initialize(const ScriptContext& context) override {
        gameState_ = context.gameState;
        overlay_ = context.overlays;
        memory_ = context.memory;
        gameModule_ = context.gameModule;
        if (!gameModule_ || !memory_ || !gameState_) return false;
        if (overlay_) overlay_->ShowTransientMessage("Left HUD probe loaded (native transform NOT resolved)", 6000);
        return true;
    }

    void OnTick(float) override {
        // No address guessing, memory writes, game-file edits or transform simulation.
        // Native leftHudPanelContainer access must be verified before adding a marker.
        // Keeping this plugin a load/lifecycle probe avoids a misleading visual test.
        if (!gameState_ || !gameState_->IsInGame()) return;
    }

    void Shutdown() override {
        gameModule_ = nullptr;
        memory_ = nullptr;
        gameState_ = nullptr;
        overlay_ = nullptr;
    }
private:
    HMODULE gameModule_ = nullptr;
    IGameMemoryService* memory_ = nullptr;
    IGameStateService* gameState_ = nullptr;
    IOverlayService* overlay_ = nullptr;
};

SNOWLOADER_DEFINE_SCRIPT(LeftHudProbe)
