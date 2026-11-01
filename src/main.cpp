#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <vector>
#include <cmath>

using namespace geode::prelude;

struct Pending { bool down; int button; bool p1; float waited; bool notified; };

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        std::vector<Pending> queue;
        CCPoint last[2] = {{0, 0}, {0, 0}};
        CCPoint velPerSec[2] = {{0, 0}, {0, 0}};
        bool replaying = false;
    };

    bool dangerAhead(PlayerObject* p, int idx) {
        if (!p || !m_objects) return false;
        auto mod = Mod::get();
        float lookSec = (float)mod->getSettingValue<int64_t>("lookahead") / 60.f;
        float reach = (float)mod->getSettingValue<int64_t>("reach");
        auto v = m_fields->velPerSec[idx];
        CCRect base = p->getObjectRect();
        CCRect f = base;
        float dx = v.x * lookSec;
        float dy = v.y * lookSec;
        f.origin.x += std::min(0.f, dx);
        f.origin.y += std::min(0.f, dy) - reach;
        f.size.width += std::fabs(dx);
        f.size.height += std::fabs(dy) + reach * 2;
        float px = p->getPositionX();
        for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
            if (obj->m_objectType != GameObjectType::Hazard) continue;
            if (std::fabs(obj->getPositionX() - px) > 800.f) continue;
            if (f.intersectsRect(obj->getObjectRect())) return true;
        }
        return false;
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
        PlayerObject* ps[2] = {m_player1, m_player2};
        for (int i = 0; i < 2; i++) {
            if (!ps[i] || dt <= 0.f) continue;
            auto pos = ps[i]->getPosition();
            m_fields->velPerSec[i] = (pos - m_fields->last[i]) / dt;
            m_fields->last[i] = pos;
        }
        auto& q = m_fields->queue;
        m_fields->replaying = true;
        for (auto it = q.begin(); it != q.end();) {
            int idx = it->p1 ? 0 : 1;
            bool hold = it->waited < maxSec && dangerAhead(ps[idx], idx);
            if (hold) {
                if (!it->notified) {
                    it->notified = true;
                    Notification::create("HOLD", NotificationIcon::None, 0.3f)->show();
                }
                it->waited += dt;
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
