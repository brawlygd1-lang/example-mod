#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <vector>

using namespace geode::prelude;

struct Pending { bool down; int button; bool p1; int waited; };

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        std::vector<Pending> queue;
        CCPoint last[2] = {{0, 0}, {0, 0}};
        CCPoint vel[2] = {{0, 0}, {0, 0}};
        bool replaying = false;
    };

    bool dangerAhead(PlayerObject* p, int idx) {
        if (!p || !m_objects) return false;
        auto mod = Mod::get();
        int steps = (int)mod->getSettingValue<int64_t>("lookahead");
        float reach = (float)mod->getSettingValue<int64_t>("reach");
        auto v = m_fields->vel[idx];
        CCRect base = p->getObjectRect();
        for (int k = 1; k <= steps; k++) {
            CCRect f = base;
            f.origin.x += v.x * k;
            f.origin.y += v.y * k - reach;
            f.size.height += reach * 2;
            for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                if (obj->m_objectType != GameObjectType::Hazard) continue;
                if (f.intersectsRect(obj->getObjectRect())) return true;
            }
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
        m_fields->queue.push_back({down, button, isP1, 0});
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto mod = Mod::get();
        int maxDelay = (int)mod->getSettingValue<int64_t>("max-delay");
        PlayerObject* ps[2] = {m_player1, m_player2};
        for (int i = 0; i < 2; i++) {
            if (!ps[i]) continue;
            auto pos = ps[i]->getPosition();
            m_fields->vel[i] = pos - m_fields->last[i];
            m_fields->last[i] = pos;
        }
        auto& q = m_fields->queue;
        m_fields->replaying = true;
        for (auto it = q.begin(); it != q.end();) {
            int idx = it->p1 ? 0 : 1;
            bool hold = it->waited < maxDelay && dangerAhead(ps[idx], idx);
            if (hold) {
                it->waited++;
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
