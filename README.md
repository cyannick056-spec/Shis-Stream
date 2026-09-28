# SHIS Stream — Android sender

Lightweight Android 10 sender for a Nintendo Switch running Switchroot. It captures the emulator screen and Android playback audio and publishes both to LiveKit for the companion Discord Activity.

## Goal

`Switch Android -> SHIS Stream APK -> LiveKit -> Discord Activity`

The APK does **not** run Discord. It requests Android's MediaProjection permission and `RECORD_AUDIO`, then uses `AudioPlaybackCapture` through LiveKit's `ScreenAudioCapturer`. Before enabling LiveKit's audio transport it disables physical microphone recording via `JavaAudioDeviceModule.setAudioRecordEnabled(false)`.

## Build

Every push to `main` runs `.github/workflows/android.yml`. Download the `shis-stream-debug` artifact from the GitHub Actions run and install `app-debug.apk` on Android 10.

Local builds require JDK 17 and Gradle 8.9:

```bash
gradle :app:assembleDebug
```

## App configuration

The app remembers three values:

- Backend URL: HTTPS host running the companion `Discord` repository.
- Stream name: defaults to `cris`.
- Stream key: private publisher password configured on the backend as `STREAM_KEY`.

Do not put LiveKit API secrets in this Android repository or in the APK.

## Current MVP

- Android 10 / API 29 minimum.
- Full-screen MediaProjection capture.
- Playback audio capture intended for emulators/games.
- Physical microphone recording explicitly disabled before the LiveKit audio track is enabled.
- Foreground service so capture can continue while the emulator is in front.
- LiveKit Android SDK 2.29.0.

The first hardware test should be done before optimizing bitrate/resolution so we can confirm Switchroot's MediaProjection + AudioPlaybackCapture path is stable.
