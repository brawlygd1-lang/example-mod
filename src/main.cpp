#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <vector>

using namespace geode::prelude;

static bool g_sim = false;
static bool g_died = false;

class $modify(SimPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (g_sim) {
            g_died = true;
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }
};

struct Pending { bool down; int button; bool p1; int ticksLeft; bool decided; };

class $modify(DelayLayer, GJBaseGameLayer) {
    struct Fields {
        std::vector<Pending> queue;
        bool replaying = false;
        int offset[2] = {0, 0};
    };

    bool survives(PlayLayer* pl, CheckpointObject* cp, int delay, int horizon, int hold,
                  bool p1, int button, float dt, bool half, bool last) {
        pl->loadFromCheckpoint(cp);
        g_died = false;
        for (int t = 0; t < horizon && !g_died; t++) {
            if (t == delay) GJBaseGameLayer::handleButton(true, button, p1);
            if (t == delay + hold) GJBaseGameLayer::handleButton(false, button, p1);
            GJBaseGameLayer::processCommands(dt, half, last);
        }
        return !g_died;
    }

    int decideDelay(bool p1, int button, float dt, bool half, bool last) {
        auto mod = Mod::get();
        int maxD = (int)mod->getSettingValue<int64_t>("max-delay");
        int horizon = (int)mod->getSettingValue<int64_t>("horizon");
        int hold = (int)mod->getSettingValue<int64_t>("hold-ticks");

        auto pl = PlayLayer::get();
        if (!pl) return 0;
        if (static_cast<GJBaseGameLayer*>(pl) != static_cast<GJBaseGameLayer*>(this)) return 0;
        if (!m_player1 || m_player1->m_isDead) return 0;

        Ref<CheckpointObject> cp = pl->createCheckpoint();
        if (!cp) return 0;

        int result = 0;
        bool found = false;
        g_sim = true;
        for (int d = 0; d <= maxD; d++) {
            if (survives(pl, cp.data(), d, horizon + d, hold, p1, button, dt, half, last)) {
                result = d;
                found = true;
                break;
            }
        }
        g_sim = false;
        g_died = false;
        pl->loadFromCheckpoint(cp.data());
        return found ? result : 0;
    }

    void handleButton(bool down, int button, bool isP1) {
        auto mod = Mod::get();
        int maxD = (int)mod->getSettingValue<int64_t>("max-delay");
        if (!mod->getSettingValue<bool>("enabled") || maxD <= 0 || m_fields->replaying || g_sim) {
            GJBaseGameLayer::handleButton(down, button, isP1);
            return;
        }
        m_fields->queue.push_back({down, button, isP1, 0, false});
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto mod = Mod::get();
        auto& q = m_fields->queue;
        float step = dt > 0.f ? dt : (1.f / 240.f);

        if (m_player1 && m_player1->m_isDead) q.clear();

        m_fields->replaying = true;

        for (auto& c : q) {
            if (c.decided) continue;
            int idx = c.p1 ? 0 : 1;
            if (c.down) {
                c.ticksLeft = decideDelay(c.p1, c.button, step, isHalfTick, isLastTick);
                m_fields->offset[idx] = c.ticksLeft;
                if (c.ticksLeft > 0 && mod->getSettingValue<bool>("debug")) {
                    Notification::create(
                        fmt::format("Retrasado {} pasos", c.ticksLeft),
                        NotificationIcon::None, 0.5f
                    )->show();
                }
            } else {
                c.ticksLeft = m_fields->offset[idx];
            }
            c.decided = true;
        }

        for (auto it = q.begin(); it != q.end();) {
            if (it->ticksLeft <= 0) {
                GJBaseGameLayer::handleButton(it->down, it->button, it->p1);
                it = q.erase(it);
            } else {
                it->ticksLeft--;
                ++it;
            }
        }

        m_fields->replaying = false;
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
