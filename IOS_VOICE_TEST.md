# iOS Voice Chat - Hướng dẫn test

Branch `main` commit `25f69857` đã port voice chat sang iOS.

## Những gì đã sửa (`/tmp/globed2`):
- `geobuild.py:372` `GLOBED_VOICE_CAN_TALK` bật cho `is_apple()` (trước chỉ Windows)
- `geobuild.py:478` link `-framework AVFoundation` cho iOS
- `src/platform/ios/objc.mm` setup `AVAudioSessionCategoryPlayAndRecord` + `AllowBluetooth | DefaultToSpeaker | MixWithOthers`, xử lý interruption
- `src/audio/AudioManager.cpp` gọi `setupIosAudioSession()` ở `preInitialize()` và `ensureIosAudioSessionActive()` trước `recordStart`
- `src/platform/ios/VoiceTalkButton.{hpp,cpp}` nút hold-to-talk trong level (góc phải dưới, chỉ hiện khi đang trong level có Globed)
- `src/ui/settings/SettingsLayer.cpp` sửa text voice chat cho iOS

## Build qua GitHub Actions
Workflow `.github/workflows/build.yml` đã có matrix `iOS` (macos-latest + target iOS). Push lên `main` sẽ tự trigger.

**Nếu Actions chưa chạy (fork thường bị disable):**
1. Vào `https://github.com/datlun2k11-pixel/globed2/actions`
2. Bấm `Enable workflows` nếu bị tắt
3. Vào tab `Build Globed` -> `Run workflow` -> `main`

Artifact sẽ là `globed-build-output` chứa `dankmeme.globed2.geode` đã combine 5 platform (trong đó có iOS). Tải `.geode` về.

## Cài lên iPhone
1. Cài Geode iOS qua `https://github.com/geode-sdk/ios-launcher` (theo hướng dẫn INSTALL.md)
2. Copy `.geode` vào `Geode/mods` qua Filza / TrollStore
   Hoặc dùng `geode install` nếu có CLI.
3. **QUAN TRỌNG - Info.plist:** GD IPA phải có `NSMicrophoneUsageDescription` nếu không iOS sẽ crash khi xin mic. 
   - Nếu dùng `ios-launcher` bản mới, nó đã có sẵn key này. Nếu chưa, patch IPA bằng:
   ```bash
   plutil -insert NSMicrophoneUsageDescription -string "Voice chat for Globed multiplayer" Payload/GeometryDash.app/Info.plist
   # hoặc bằng Filza: mở Info.plist -> Add -> NSMicrophoneUsageDescription = "Globed voice chat"
   ```
   Sau khi patch, resign IPA lại (TrollStore tự resign).

## Test
1. Mở GD, Login, vào Globed menu -> Link Discord (bắt buộc cho voice)
2. Vào Settings -> Audio -> Bật `Voice Chat`, cho phép Microphone khi hệ thống hỏi
3. Vào level bất kỳ (join room), bạn sẽ thấy nút `TALK` màu trắng ở góc phải dưới. **Giữ** nút để nói, thả ra để tắt mic. Khi đang nói nút đỏ + chữ `ON`.
4. Test với bạn bè trên PC: họ phải nghe được bạn (Opus 24kHz), bạn vẫn nghe được họ như cũ.

## Debug nếu không nói được
- Check log Geode: `Geode/logs/latest.log` tìm `AVAudioSession configured` và `threadStartRecord`
- Nếu `FMOD error 51 (FMOD_ERR_RECORD)` -> AVAudioSession chưa PlayAndRecord -> kiểm tra `objc.mm` đã được compile (xem build log có `VoiceTalkButton.cpp` không)
- Nếu `permission denied` -> iOS Settings -> Privacy -> Microphone -> bật cho Geometry Dash
- Nếu nghe được nhưng bạn bè không nghe bạn -> kiểm tra Discord linked, firewall UDP

## Hạn chế hiện tại
- Chỉ hold-to-talk, chưa có voice activation (open mic)
- Chưa có nút deafen riêng cho mobile (dùng nút TALK + Settings)
- macOS cũng được bật nhưng chưa có AVAudioSession (chỉ iOS cần)

Bạn có thể feedback log để mình fix tiếp.
