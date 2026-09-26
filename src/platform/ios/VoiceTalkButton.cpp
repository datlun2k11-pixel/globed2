#ifdef GEODE_IS_IOS
#include "VoiceTalkButton.hpp"
#include <globed/audio/AudioManager.hpp>
#include <globed/core/SettingsManager.hpp>
#include <Geode/utils/permission.hpp>

using namespace geode::prelude;

namespace globed {

VoiceTalkButton* VoiceTalkButton::create() {
    auto ret = new VoiceTalkButton();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool VoiceTalkButton::init() {
    if (!CCNode::init()) return false;

    this->setContentSize({64.f, 64.f});
    this->setAnchorPoint({0.5f, 0.5f});

    // BG circle (use GJ square as fallback)
    m_bg = CCSprite::create("GJ_button_01.png");
    if (!m_bg) m_bg = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
    if (m_bg) {
        m_bg->setScale(0.7f);
        m_bg->setPosition(getContentSize() / 2);
        this->addChild(m_bg);
    }

    m_icon = CCSprite::create("deafen-icon-off.png"_spr);
    if (!m_icon) {
        // fallback to text if sprite missing
        m_icon = nullptr;
    } else {
        m_icon->setScale(0.8f);
        m_icon->setPosition(getContentSize() / 2 + CCPoint{0, 8});
        this->addChild(m_icon);
    }

    m_label = CCLabelBMFont::create("TALK", "bigFont.fnt");
    if (m_label) {
        m_label->setScale(0.35f);
        m_label->setPosition(getContentSize() / 2 + CCPoint{0, -18});
        this->addChild(m_label);
    }

    this->setTouchEnabled(true);
    this->schedule(schedule_selector(VoiceTalkButton::updateVisibility), 0.3f);
    this->updateVisibility();
    this->setTalking(false);

    return true;
}

void VoiceTalkButton::updateVisibility() {
    bool shouldShow = globed::setting<bool>("core.audio.voice-chat-enabled");
    // Only show in levels where Globed is active, but we check GlobedGJBGL existence
    auto gjbgl = GlobedGJBGL::get();
    bool active = gjbgl && gjbgl->m_fields->m_active;
    this->setVisible(shouldShow && active);
    this->setTouchEnabled(shouldShow && active);
}

void VoiceTalkButton::setTalking(bool talking) {
    if (m_talking == talking) return;
    m_talking = talking;

    if (talking) {
        // request mic permission on first talk
        if (!geode::utils::permission::getPermissionStatus(geode::utils::permission::Permission::RecordAudio)) {
            geode::utils::permission::requestPermission(geode::utils::permission::Permission::RecordAudio, [this](bool granted){
                if (!granted) {
                    geode::queueInMainThread([]{
                        FLAlertLayer::create("Microphone", "Microphone permission denied. Enable it in Settings > Privacy > Microphone.", "OK")->show();
                    });
                    this->setTalking(false);
                    return;
                }
                // ensure session active after permission granted
                extern void ensureIosAudioSessionActive();
                ensureIosAudioSessionActive();
                geode::queueInMainThread([this]{
                    if (auto gjbgl = GlobedGJBGL::get()) gjbgl->resumeVoiceRecording();
                    if (m_bg) m_bg->setColor({255, 80, 80});
                    if (m_label) m_label->setString("ON");
                });
            });
            return;
        }

        extern void ensureIosAudioSessionActive();
        ensureIosAudioSessionActive();
        if (auto gjbgl = GlobedGJBGL::get()) gjbgl->resumeVoiceRecording();
        if (m_bg) m_bg->setColor({255, 80, 80});
        if (m_label) m_label->setString("ON");
        if (m_icon) m_icon->setColor({255,255,255});
    } else {
        if (auto gjbgl = GlobedGJBGL::get()) gjbgl->pauseVoiceRecording();
        if (m_bg) m_bg->setColor({255,255,255});
        if (m_label) m_label->setString("TALK");
    }
}

bool VoiceTalkButton::ccTouchBegan(CCTouch* touch, CEvent* event) {
    if (!this->isVisible()) return false;
    auto pos = this->convertTouchToNodeSpace(touch);
    auto size = this->getContentSize();
    cocos2d::CCRect rect{0,0, size.width, size.height};
    if (!rect.containsPoint(pos)) return false;

    m_touching = true;
    this->setTalking(true);
    // animate scale
    this->stopAllActions();
    this->runAction(CCScaleTo::create(0.05f, 1.1f));
    return true;
}

void VoiceTalkButton::ccTouchEnded(CCTouch* touch, CEvent* event) {
    if (!m_touching) return;
    m_touching = false;
    this->setTalking(false);
    this->stopAllActions();
    this->runAction(CCScaleTo::create(0.05f, 1.0f));
}

void VoiceTalkButton::ccTouchCancelled(CCTouch* touch, CEvent* event) {
    this->ccTouchEnded(touch, event);
}

void VoiceTalkButton::registerWithTouchDispatcher() {
    CCTouchDispatcher::get()->addTargetedDelegate(this, -500, true);
}

}
#endif
