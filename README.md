# SHIS Stream — SysDVR sender for Nintendo Switch

The current stream sender is **SysDVR SHIS Direct v0.6** for a Nintendo Switch running Atmosphère. It sends game video and audio directly from the Switch to the native relay used by the [SHIS Discord Activity](https://github.com/cyannick056-spec/Discord). Android and the older standalone forwarder remain in this repository as previous approaches.

## How it works

```text
Switch (SysDVR SHIS Direct: H.264 video + PCM audio)
  -> Railway TCP proxy
  -> SHIS native relay (Discord/relay)
  -> LiveKit Cloud
  -> Discord Activity and viewers
```

The Switch sends frames over TCP after authenticating with its private `stream_key`. The relay publishes to LiveKit. The Activity receives the stream; the Switch does not run Discord or need LiveKit credentials.

## Build and install SysDVR SHIS Direct

The [Build SysDVR SHIS Direct](https://github.com/cyannick056-spec/Shis-Stream/actions/workflows/sysdvr-shis.yml) workflow builds the customized sysmodule from pinned SysDVR 6.3 source plus [`sysdvr-shis/TCPmode.c`](sysdvr-shis/TCPmode.c). It runs when that source or its workflow changes and can also be started manually. Download the `SysDVR-SHIS-Direct-v0.6` artifact from a successful run.

1. Back up the existing SysDVR files on your SD card. Copy the artifact's `atmosphere/` and `config/` directories to the root of the SD card. The package installs the customized `exefs.nsp`, `boot2.flag`, and `/config/sysdvr/tcp` marker.
2. Edit `/config/sysdvr/shis.ini` on the SD card using the format below. Use the **relay TCP proxy** host and port, not the Activity's HTTPS URL. Keep the real `stream_key` private.
3. Start the Switch with Atmosphère and launch a game that supports SysDVR capture. The customized TCP mode connects to the relay and reconnects after interruptions.

```ini
relay_host=YOUR_RELAY_TCP_HOST
relay_port=YOUR_RELAY_TCP_PORT
stream=cris
stream_key=YOUR_PRIVATE_STREAM_KEY
```

The configured `stream` must match the stream selected by Activity viewers (default: `cris`). The relay's `STREAM_KEY` environment variable must match `stream_key` exactly. There are no real hostnames, ports or secrets in this repository.

This build **replaces SysDVR's normal TCP mode** with SHIS output. Do not also run the older standalone SHIS Forwarder. To restore normal TCP mode, put the official SysDVR `exefs.nsp` back in place.

## Other source directories

- `sysdvr-shis/`: active direct SysDVR integration; the build workflow packages it for the SD card.
- `native/`: older separate Switch forwarder and its own configuration. It is not needed with SysDVR SHIS Direct.
- `app/`: older Android 10 / Switchroot sender using MediaProjection, audio playback capture and LiveKit. Its `android.yml` workflow builds an APK, but the current Switch stream uses SysDVR instead.

The Activity server and native relay are in the [Discord repository](https://github.com/cyannick056-spec/Discord). Keep LiveKit API credentials on those servers, never in the Switch configuration or this repository.
