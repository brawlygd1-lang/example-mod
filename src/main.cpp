#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <vector>
#include <cmath>

using namespace geode::prelude;

struct Pending { bool down; int button; bool p1; float waited; bool notified; };

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        std::vector<Pending> queue;
        bool replaying = false;
        int ticks = 0;
        int lastNear = 0;
    };

    bool dangerAhead(PlayerObject* p) {
        if (!p || !m_objects) return false;
        auto mod = Mod::get();
        float ahead = (float)mod->getSettingValue<int64_t>("lookahead") * 10.f;
        float reach = (float)mod->getSettingValue<int64_t>("reach");
        CCRect r = p->getObjectRect();
        CCRect box(
            r.getMinX(),
            r.getMinY() - reach,
            r.size.width + ahead,
            r.size.height + reach * 2
        );
        float px = p->getPositionX();
        int near = 0;
        bool hit = false;
        for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
            if (obj->m_objectType != GameObjectType::Hazard) continue;
            if (std::fabs(obj->getPositionX() - px) > 1500.f) continue;
            near++;
            if (box.intersectsRect(obj->getObjectRect())) hit = true;
        }
        m_fields->lastNear = near;
        return hit;
    }

    void handleButton(bool down, int button, bool isP1) {
        auto mod = Mod::get();
        int maxDelay = (int)mod->getSettingValue<int64_t>("max-delay");
        if (!mod->getSettingValue<bool>("enabled") || maxDelay <= 0 || m_fields->replaying) {
            GJBaseGameLayer::handleButton(down, button, isP1);
            return;
        }
        m_fields->queue.push_back({down, button, isP1, 0.f, false});
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto mod = Mod::get();
        float maxSec = (float)mod->getSettingValue<int64_t>("max-delay") / 60.f;
        float step = dt > 0.f ? dt : (1.f / 240.f);
        PlayerObject* ps[2] = {m_player1, m_player2};

        m_fields->ticks++;
        if (m_fields->ticks % 120 == 0) {
            dangerAhead(m_player1);
            Notification::create(
                fmt::format("near={}", m_fields->lastNear),
                NotificationIcon::None, 0.8f
            )->show();
        }

        auto& q = m_fields->queue;
        m_fields->replaying = true;
        for (auto it = q.begin(); it != q.end();) {
            int idx = it->p1 ? 0 : 1;
            bool hold = it->waited < maxSec && dangerAhead(ps[idx]);
            if (hold) {
                if (!it->notified) {
                    it->notified = true;
                    Notification::create("HOLD", NotificationIcon::None, 0.4f)->show();
                }
                it->waited += step;
                ++it;
            } else {
                GJBaseGameLayer::handleButton(it->down, it->button, it->p1);
                it = q.erase(it);
            }
        }
        m_fields->replaying = false;
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
