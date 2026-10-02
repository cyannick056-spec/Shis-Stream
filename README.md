# SHIS Stream — SysDVR sender for Nintendo Switch

This repository contains the Switch-side sender used by SHIS Stream. The supported implementation is **SysDVR SHIS Direct v0.6** for a Nintendo Switch running Atmosphère.

It captures game video and audio with SysDVR and sends them directly to the SHIS relay:

```text
Nintendo Switch
  └─ SysDVR SHIS Direct
       ├─ H.264 video
       └─ PCM audio
          ↓ TCP
Railway SHIS relay
          ↓
Cloudflare Realtime SFU
          ↓
SHIS Discord Activity
          ↓
Viewers in Discord
```

The Switch does not run Discord and does not contain Cloudflare credentials. It only needs the relay TCP endpoint, a logical stream name and a private stream key.

The Activity/backend lives in [`cyannick056-spec/Discord`](https://github.com/cyannick056-spec/Discord).

## Build and install

The [Build SysDVR SHIS Direct](https://github.com/cyannick056-spec/Shis-Stream/actions/workflows/sysdvr-shis.yml) workflow builds the customized sysmodule from pinned SysDVR source plus [`sysdvr-shis/TCPmode.c`](sysdvr-shis/TCPmode.c).

1. Back up the existing SysDVR files on the SD card.
2. Download the `SysDVR-SHIS-Direct-v0.6` artifact from a successful workflow run.
3. Copy its `atmosphere/` and `config/` directories to the root of the SD card.
4. Edit `/config/sysdvr/shis.ini` with the relay TCP proxy host, port, stream name and private stream key.
5. Boot Atmosphère and start a game compatible with SysDVR capture.

Example:

```ini
relay_host=YOUR_RELAY_TCP_HOST
relay_port=YOUR_RELAY_TCP_PORT
stream=shis
stream_key=YOUR_PRIVATE_STREAM_KEY
```

`relay_host` and `relay_port` point to the **Railway TCP proxy for the native relay**, not the Discord Activity HTTPS URL. `stream_key` must match the relay's server-side `STREAM_KEY`; never commit the real value.

## Repository contents

- `sysdvr-shis/TCPmode.c` — SHIS direct transport integrated into SysDVR.
- `.github/workflows/sysdvr-shis.yml` — build and package workflow for the SD-card artifact.

The retired Android/Switchroot sender and standalone Switch forwarder have been removed from the active repository. Their history remains recoverable through Git history, but they are not part of the supported SHIS Stream architecture.

This customized build replaces SysDVR's normal TCP output path with the SHIS relay protocol. To restore ordinary SysDVR TCP mode, restore the official SysDVR `exefs.nsp`.

## Security

- Keep `stream_key` private.
- Do not place Discord, Cloudflare or deployment credentials on the Switch.
- Real production hostnames, ports and secrets are intentionally not stored in this public repository.
- Server-side credentials belong in Railway environment variables.

## Project split

**This repository:** Switch capture and SysDVR SHIS Direct packaging.

**[`cyannick056-spec/Discord`](https://github.com/cyannick056-spec/Discord):** Discord Activity, authorization, shared scene/editor state, Cloudflare signaling and the native Railway relay.
