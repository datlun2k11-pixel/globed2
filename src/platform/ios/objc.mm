#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <core/preload/PreloadManager.hpp>
#include <Geode/utils/permission.hpp>

extern "C" {

void* SecTaskCreateFromSelf(CFAllocatorRef allocator);
NSString* SecTaskCopyTeamIdentifier(void* task, NSError** error);
CFTypeRef SecTaskCopyValueForEntitlement(void* task, CFStringRef key, CFErrorRef* error);

}

using namespace geode::prelude;

namespace globed {

const char* getIosTeamId() {
    static NSString* ans = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        void* taskSelf = SecTaskCreateFromSelf(NULL);
        CFErrorRef error = NULL;
        CFTypeRef cfans = SecTaskCopyValueForEntitlement(taskSelf, CFSTR("com.apple.developer.team-identifier"), &error);
        if(CFGetTypeID(cfans) == CFStringGetTypeID()) {
            ans = (__bridge NSString*)cfans;
        }
        CFRelease(taskSelf);
        if (!ans)
            ans = [[[NSBundle mainBundle].bundleIdentifier componentsSeparatedByString:@"."] lastObject];
    });
    return ans.UTF8String;
}

// CCFileUtils::isFileExist on MacOS will always return false if the path is a
// relative path that does not contain any slashes, so we reimplement it properly.
// Thanks cocos!
bool isFileExistImpl(geode::ZStringView path) {
    if (path.empty()) return false;

    auto view = path.view();

    if (view[0] == '/') {
        // absolute path
        return [[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:path.c_str()]];
    }

    StringBuffer<512> pathBuf;
    StringBuffer<512> fileBuf;
    size_t pos = view.find_last_of('/');
    if (pos != std::string::npos) {
        fileBuf.append(view.substr(pos + 1));
        pathBuf.append(view.substr(0, pos));
    } else {
        fileBuf.append(view);
    }

    NSString* nspath = pathBuf.size() == 0 ? nil : [NSString stringWithUTF8String:pathBuf.c_str()];
    NSString* nsfile = [NSString stringWithUTF8String:fileBuf.c_str()];
    NSString* fullpath = [[NSBundle mainBundle] pathForResource:nsfile ofType:nil inDirectory:nspath];

    return fullpath != nil;
}

std::string getPathForDirAndFilenameImpl(geode::ZStringView directory, geode::ZStringView filename) {
    if (!directory.empty() && directory.view().front() == '/') {
        // absolute path
        geode::utils::StringBuffer<1024> fullBuf;
        fullBuf.append("{}{}", directory, filename);
        if ([[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:fullBuf.c_str()]]) {
            return fullBuf.str();
        }
    } else {
        // relative path
        NSString* fp = [[NSBundle mainBundle]
            pathForResource:[NSString stringWithUTF8String:filename.c_str()]
            ofType:nil
            inDirectory:[NSString stringWithUTF8String:directory.c_str()]
        ];
        if (fp) {
            return std::string([fp UTF8String]);
        }
    }
    return std::string{};
}

std::unique_ptr<unsigned char[]> getFileDataImpl(geode::ZStringView path, unsigned long* outSize) {
    NSString* nsp = [NSString stringWithUTF8String:path.c_str()];
    NSError* error = nil;
    NSData* data = [NSData dataWithContentsOfFile:nsp options:NSDataReadingMappedIfSafe error:&error];

    if (data) {
        if (outSize) *outSize = [data length];
        auto buffer = std::make_unique<unsigned char[]>([data length]);
        [data getBytes:buffer.get() length:[data length]];
        return buffer;
    }

    if (outSize) *outSize = 0;
    log::warn("Failed to read path '{}': {}", path, [[error localizedDescription] UTF8String]);
    return nullptr;
}

// Voice chat iOS audio session setup - required for FMOD recording on iOS
// Must use PlayAndRecord category, otherwise System::recordStart returns FMOD_ERR_RECORD
void setupIosAudioSession() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSError* error = nil;
        AVAudioSession* session = [AVAudioSession sharedInstance];

        // PlayAndRecord is mandatory for simultaneous playback + mic capture via FMOD
        // AllowBluetooth enables AirPods / BT headset, DefaultToSpeaker uses loudspeaker (not earpiece)
        // MixWithOthers allows GD music to keep playing
        BOOL ok = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                          withOptions:AVAudioSessionCategoryOptionAllowBluetooth
                                     | AVAudioSessionCategoryOptionAllowBluetoothA2DP
                                     | AVAudioSessionCategoryOptionDefaultToSpeaker
                                     | AVAudioSessionCategoryOptionMixWithOthers
                                error:&error];
        if (!ok) {
            log::warn("Failed to set AVAudioSession category: {}", [[error localizedDescription] UTF8String]);
        }

        // Set preferred sample rate to match Globed voice (48000 or 24000)
        [session setPreferredSampleRate:48000 error:nil];
        [session setPreferredIOBufferDuration:0.01 error:nil];

        ok = [session setActive:YES error:&error];
        if (!ok) {
            log::warn("Failed to activate AVAudioSession: {}", [[error localizedDescription] UTF8String]);
        } else {
            log::info("iOS AVAudioSession configured for voice chat (PlayAndRecord)");
        }

        // Listen for interruptions (phone call, Siri) to reactivate session
        [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionInterruptionNotification
            object:session queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* note) {
                NSInteger type = [note.userInfo[AVAudioSessionInterruptionTypeKey] integerValue];
                if (type == AVAudioSessionInterruptionTypeEnded) {
                    NSError* e = nil;
                    [session setActive:YES error:&e];
                    log::info("AVAudioSession reactivated after interruption");
                }
            }];
    });
}

// Ensure session is active before each recordStart - idempotent
void ensureIosAudioSessionActive() {
    AVAudioSession* session = [AVAudioSession sharedInstance];
    if (session.category != AVAudioSessionCategoryPlayAndRecord) {
        setupIosAudioSession();
    } else {
        NSError* error = nil;
        [session setActive:YES error:&error];
    }
}

} // namespace globed

// Hook early via mod load - called from AudioManager::preInitialize on iOS
namespace {
struct IosAudioSessionInit {
    IosAudioSessionInit() {
        // delay until main thread, ensure called once
        geode::queueInMainThread([]{
            globed::setupIosAudioSession();
        });
    }
};
static IosAudioSessionInit _iosAudioInit;
}
