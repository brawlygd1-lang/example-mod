#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(DiagLayer, GJBaseGameLayer) {
    struct Fields {
        int frames = 0;
    };

    void handleButton(bool down, int button, bool isP1) {
        Notification::create(
            fmt::format("CLICK down={} btn={}", down, button),
            NotificationIcon::None, 0.4f
        )->show();
        GJBaseGameLayer::handleButton(down, button, isP1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        m_fields->frames++;
        if (m_fields->frames % 120 == 0 && m_objects) {
            int total = 0;
            int hazards = 0;
            for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                total++;
                if (obj->m_objectType == GameObjectType::Hazard) hazards++;
            }
            Notification::create(
                fmt::format("objs={} hazards={}", total, hazards),
                NotificationIcon::None, 1.0f
            )->show();
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
