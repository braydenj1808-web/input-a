#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// Buffers jump presses by delaying the *release*.
// After you let go, the game keeps seeing the button as held for `window-ms`
// longer, so a tap made slightly before you can act (landing, orb, pad)
// still fires the moment it becomes valid. Hold-based modes (ship, wave, swing) are skipped because
// a delayed release would hurt them.

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        float time = 0.f;                       // accumulated game time (s)
        float pressTime[2] = {-1000.f, -1000.f};
        float releaseAt[2] = {0.f, 0.f};
        bool pending[2] = {false, false};       // a release is being delayed
    };

    bool bufferEnabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    float windowSeconds() {
        return static_cast<float>(Mod::get()->getSettingValue<int64_t>("window-ms")) / 1000.f;
    }

    bool modeAllowed(bool isPlayer1) {
        auto pl = isPlayer1 ? m_player1 : m_player2;
        if (!pl) return false;
        return !(pl->m_isShip || pl->m_isDart || pl->m_isSwing);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        // Only touch the jump button (1); leave left/right (platformer) alone.
        if (button != 1 || !bufferEnabled() || !modeAllowed(isPlayer1)) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto f = m_fields.self();
        int i = isPlayer1 ? 0 : 1;

        if (down) {
            // A new press while a release is still pending: finish the old
            // release first so the game sees a clean release -> press.
            if (f->pending[i]) {
                f->pending[i] = false;
                GJBaseGameLayer::handleButton(false, 1, isPlayer1);
            }
            f->pressTime[i] = f->time;
            GJBaseGameLayer::handleButton(true, 1, isPlayer1);
            return;
        }

        // Release: don't let go right away. Keep the button held for `window`
        // longer, so a press made a little too early is still active when the
        // player can finally act (landing, orb, pad).
        float window = windowSeconds();
        if (window > 0.f) {
            f->pending[i] = true;
            f->releaseAt[i] = f->time + window;
            return;
        }

        GJBaseGameLayer::handleButton(false, 1, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto f = m_fields.self();
        f->time += dt;

        for (int i = 0; i < 2; i++) {
            if (!f->pending[i]) continue;

            auto pl = i == 0 ? m_player1 : m_player2;
            bool dead = pl && pl->m_isDead;

            if (dead || f->time >= f->releaseAt[i]) {
                f->pending[i] = false;
                GJBaseGameLayer::handleButton(false, 1, i == 0);
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
