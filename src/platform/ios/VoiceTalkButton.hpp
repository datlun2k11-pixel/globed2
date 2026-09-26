#pragma once
#include <Geode/Geode.hpp>
#ifdef GEODE_IS_IOS
#include <core/hooks/GJBaseGameLayer.hpp>

namespace globed {

// Simple hold-to-talk button for iOS
// Shows mic icon, turns red when transmitting
class VoiceTalkButton : public cocos2d::CCLayer {
public:
    static VoiceTalkButton* create();
    bool init() override;
    void updateVisibility();
private:
    cocos2d::CCSprite* m_bg = nullptr;
    cocos2d::CCSprite* m_icon = nullptr;
    cocos2d::CCLabelBMFont* m_label = nullptr;
    bool m_talking = false;
    bool m_touching = false;

    void setTalking(bool talking);
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void registerWithTouchDispatcher() override;
};

}
#endif
