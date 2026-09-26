#include <globed/config.hpp>
#include <globed/core/RoomManager.hpp>
#include <globed/core/PopupManager.hpp>
#include <globed/core/EmoteManager.hpp>
#include <globed/audio/AudioManager.hpp>
#include <core/net/NetworkManagerImpl.hpp>
#include <core/game/SettingCache.hpp>
#include <core/hooks/GJBaseGameLayer.hpp>
#include <ui/misc/HoldableButton.hpp>
#include <ui/misc/CancellableMenu.hpp>
#include <modules/ui/UIModule.hpp>
#include <modules/ui/popups/UserListPopup.hpp>
#include <modules/ui/popups/EmoteListPopup.hpp>
#ifdef GEODE_IS_IOS
#include <Geode/utils/permission.hpp>
#endif

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <UIBuilder.hpp>

using namespace geode::prelude;

namespace globed {

#ifdef GEODE_IS_IOS
// Persistent toggle state for iOS pause menu voice (open mic)
static bool s_iosVoiceToggleOn = false;
#endif

struct GLOBED_MODIFY_ATTR UIHookedPauseLayer : Modify<UIHookedPauseLayer, PauseLayer> {
    static void onModify(auto& self) {
        (void) self.setHookPriority("PauseLayer::customSetup", 10);
        (void) self.setHookPriority("PauseLayer::onQuit", -10000);
        (void) self.setHookPriority("PauseLayer::goEdit", -999999999);
        (void) self.setHookPriority("PauseLayer::onRestart", -99999);
        (void) self.setHookPriority("PauseLayer::onRestartFull", -99999);

        GLOBED_CLAIM_HOOKS(UIModule::get(), self,
            "PauseLayer::customSetup",
            "PauseLayer::onQuit",
            "PauseLayer::onEdit",
            "PauseLayer::goEdit",
            "PauseLayer::onResume",
            "PauseLayer::onNormalMode",
            "PauseLayer::onPracticeMode",
            "PauseLayer::onRestart",
            "PauseLayer::onRestartFull",
        );
    }

    struct Fields {
        CCMenu* m_rightMenu = nullptr;
        CCMenu* m_quickEmotePopup = nullptr;

        ~Fields() {
            CachedSettings::get().reload();
        }
    };

    $override
    void customSetup() {
        PauseLayer::customSetup();

        auto gpl = GlobedGJBGL::get();
        if (!gpl || !gpl->active()) return;

        // prevent some keybinds from being active in pause menu
#ifdef GEODE_IS_IOS
        // iOS toggle mode: don't auto-mute, keep toggle state
        if (!s_iosVoiceToggleOn) {
            gpl->pauseVoiceRecording();
        }
#else
        gpl->pauseVoiceRecording();
#endif

        auto& fields = *m_fields.self();

        auto winSize = CCDirector::get()->getWinSize();

        auto menu = Build<CancellableMenu>::create()
            .id("playerlist-menu"_spr)
            .parent(this)
            .pos(winSize.width - 52.f, 24.f)
            .anchorPoint(0.5f, 0.f)
            .contentSize(48.f, winSize.height - 48.f)
            .layout(ColumnLayout::create()->setAutoScale(false)->setAxisAlignment(AxisAlignment::Start)->setGap(0.f))
            .collect();
        fields.m_rightMenu = menu;
        menu->setTouchCallback([this](bool within) {
            return this->maybeDismissEmotePopup();
        });

        Build<CCSprite>::create("icon-players.png"_spr)
            .scale(0.9f)
            .intoMenuItem(+[] {
                UserListPopup::create()->show();
            })
            .scaleMult(1.2f)
            .id("btn-open-playerlist"_spr)
            .parent(menu);

        if (globed::setting<bool>("core.player.quick-chat-enabled")) {
            auto spr = Build<CCSprite>::create("icon-emotes.png"_spr)
                .scale(0.9f)
                .collect();

            auto btn = HoldableButton::create(spr, [](auto) {
                EmoteListPopup::create()->show();
            }, [this](auto) {
                this->showQuickEmotePopup();
            });

#ifdef GEODE_IS_DESKTOP
            btn->setHoldThreshold(0.f); // disable on pc
#endif

            btn->setID("btn-open-emotelist"_spr);
            menu->addChild(btn);
        }

#ifdef GEODE_IS_IOS
        // iOS voice toggle (pause menu) - replaces hold-to-talk
        if (globed::setting<bool>("core.audio.voice-chat-enabled")) {
            auto onSpr = Build<CCSprite>::create("deafen-icon-off.png"_spr).scale(0.85f).collect();
            auto offSpr = Build<CCSprite>::create("deafen-icon-on.png"_spr).scale(0.85f).collect();
            // Fallback if sprites missing
            if (!onSpr) onSpr = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
            if (!offSpr) offSpr = CCSprite::createWithSpriteFrameName("GJ_button_02.png");

            auto toggler = CCMenuItemExt::createToggler(
                onSpr, offSpr,
                [this](CCMenuItemToggler* t) {
                    // CCMenuItemToggler already toggled before callback, isToggled() is new state
                    s_iosVoiceToggleOn = t->isToggled();

                    auto gpl = GlobedGJBGL::get();
                    if (!gpl) return;

                    if (s_iosVoiceToggleOn) {
                        // request mic permission if needed
                        if (!geode::utils::permission::getPermissionStatus(geode::utils::permission::Permission::RecordAudio)) {
                            geode::utils::permission::requestPermission(geode::utils::permission::Permission::RecordAudio, [t](bool granted){
                                geode::queueInMainThread([t, granted]{
                                    if (!granted) {
                                        FLAlertLayer::create("Microphone", "Microphone permission denied. Enable in Settings > Privacy > Microphone.", "OK")->show();
                                        s_iosVoiceToggleOn = false;
                                        t->toggle(false);
                                        extern void setIosVoiceActive(bool);
                                        setIosVoiceActive(false);
                                        return;
                                    }
                                    extern void ensureIosAudioSessionActive();
                                    ensureIosAudioSessionActive();
                                    if (auto g = GlobedGJBGL::get()) g->resumeVoiceRecording();
                                });
                            });
                            // revert toggle until permission granted
                            if (!geode::utils::permission::getPermissionStatus(geode::utils::permission::Permission::RecordAudio)) {
                                // will be handled in callback
                            }
                        } else {
                            extern void ensureIosAudioSessionActive();
                            ensureIosAudioSessionActive();
                            gpl->resumeVoiceRecording();
                        }
                    } else {
                        gpl->pauseVoiceRecording();
                        extern void setIosVoiceActive(bool);
                        setIosVoiceActive(false);
                    }
                }
            );
            toggler->toggle(s_iosVoiceToggleOn);
            toggler->setID("btn-voice-toggle"_spr);
            // scale up a bit for mobile
            toggler->setScale(1.05f);
            menu->addChild(toggler);

            // If toggle was ON before pausing, ensure voice stays ON while in pause menu
            if (s_iosVoiceToggleOn) {
                // ensure session active and resume
                extern void ensureIosAudioSessionActive();
                ensureIosAudioSessionActive();
                gpl->resumeVoiceRecording();
            }
        }
#endif

        menu->updateLayout();

        this->schedule(schedule_selector(UIHookedPauseLayer::selUpdate), 0.f);
    }

    void selUpdate(float dt) {
        if (auto pl = GlobedGJBGL::get()) {
            pl->pausedUpdate(dt);
        }
    }

    void showQuickEmotePopup() {
        float emoteSize = 28.f;

        auto grid = CCMenu::create();
        grid->setID("quick-emote-popup"_spr);
        grid->setLayout(RowLayout::create()
            ->setGap(5.f)
            ->setGrowCrossAxis(true)
            ->setAutoScale(false)
        );
        grid->setZOrder(99);

        // fit 2 rows, 4 columns
        grid->setContentSize({emoteSize * 2.f + 5.f, 0.f});

        auto& em = EmoteManager::get();

        for (size_t i = 0; i < 8; i++) {
            auto spr = em.createFavoriteEmote(i);
            if (!spr) {
                // default plus btn (emote 0)
                spr = em.createEmote(0);
            }

            Build(spr)
                .with([&](auto spr) { cue::rescaleToMatch(spr, emoteSize); })
                .intoMenuItem([this, i](auto self) {
                    auto& em = EmoteManager::get();
                    auto emoteId = em.getFavoriteEmote(i);

                    if (emoteId == 0) return; // no emote assigned

                    if (GlobedGJBGL::get()->playSelfEmote(emoteId)) {
                        this->onResume(this);
                    } else {
                        // tint the emoji to red and then back, also shake the button
                        auto tint = CCSequence::create(
                            CCTintTo::create(0.1f, 255, 50, 50),
                            CCTintTo::create(0.1f, 255, 255, 255),
                            nullptr
                        );

                        auto shake = CCRepeat::create(
                            CCSequence::create(
                                CCRotateBy::create(0.05f, 15.f),
                                CCRotateBy::create(0.05f, -30.f),
                                CCRotateBy::create(0.05f, 15.f),
                                nullptr
                            ),
                            2
                        );

                        self->runAction(tint);
                        self->runAction(shake);
                    }
                })
                .scaleMult(1.15f)
                .parent(grid);
        }

        grid->updateLayout();

        cue::attachBackground(grid, cue::BackgroundOptions {
            .opacity = 255,
            .sidePadding = 8.f,
            .verticalPadding = 8.f,
            .texture = "GJ_square06.png",
        });

        grid->setPosition(m_fields->m_rightMenu->getPosition() + CCSize{0.f, 50.f});
        grid->setAnchorPoint({0.5f, 0.f});

        grid->setScale(0.01f);
        grid->runAction(
            CCEaseBackOut::create(
                CCScaleTo::create(0.2f, 1.f)
            )
        );

        this->addChild(grid);
        m_fields->m_quickEmotePopup = grid;
    }

    bool maybeDismissEmotePopup() {
        auto& p = m_fields->m_quickEmotePopup;
        if (!p) return false;

        // this is terribly hacky and i'm so sorry,
        // but if the user pressed anywhere that's not an emote button, close it and consume touch
        if (!p->m_pSelectedItem) {
            cue::resetNode(p);
            return true;
        }
        return false;
    }

    $override
    void onQuit(CCObject* sender) {
        if (this->hasPopup()) return;

        PauseLayer::onQuit(sender);
    }

    $override
    void goEdit() {
        if (auto pl = GlobedGJBGL::get()) {
            pl->onQuit();
        }

        PauseLayer::goEdit();
    }

    bool hasPopup() {
        // Due to rob's confirm exit bug, pauselayer functions may be invoked on an invalid pauselayer and this can crash
        // we check if a PauseLayer exists in the scene and is equal to `this`, and avoid doing anything otherwise
        auto scene = CCScene::get();
        if (!scene) return false;
        auto curPause = scene->getChildByType<PauseLayer>(0);
        if (curPause != this) return false;

        auto parent = this->getParent();
        if (!parent) return false;

        return parent->getChildByType<UserListPopup>(0);
    }

#define REPLACE(method) \
    void method(CCObject* s) {\
        if (!this->hasPopup()) { \
            PauseLayer::method(s); \
        } \
    }

    void onResume(CCObject* s) {
        if (this->hasPopup()) return;
#ifdef GEODE_IS_IOS
        // Keep iOS voice toggle state after resuming, and restore audio session
        if (s_iosVoiceToggleOn) {
            if (auto g = GlobedGJBGL::get()) {
                extern void ensureIosAudioSessionActive();
                ensureIosAudioSessionActive();
                g->resumeVoiceRecording();
            }
        } else {
            if (auto g = GlobedGJBGL::get()) g->pauseVoiceRecording();
            extern void setIosVoiceActive(bool);
            setIosVoiceActive(false);
        }
#endif
        PauseLayer::onResume(s);
    }
    REPLACE(onNormalMode);
    REPLACE(onPracticeMode);

    $override
    REPLACE(onEdit);

    $override
    void onRestart(CCObject* s) {
        if (this->hasPopup()) return;

        auto& fields = *GlobedGJBGL::get()->m_fields.self();

        fields.m_manualReset = true;
        PauseLayer::onRestart(s);
        fields.m_manualReset = false;
    }

    $override
    void onRestartFull(CCObject* s) {
        if (this->hasPopup()) return;

        auto& fields = *GlobedGJBGL::get()->m_fields.self();

        fields.m_manualReset = true;
        PauseLayer::onRestartFull(s);
        fields.m_manualReset = false;
    }
};

}
