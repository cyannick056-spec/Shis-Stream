# SHIS Stream — SysDVR sender for Nintendo Switch

This repository contains the Switch-side sender used by SHIS Stream. The active implementation is **SysDVR SHIS Direct v0.6** for a Nintendo Switch running Atmosphère.

It captures game video and audio with SysDVR and sends them directly to the SHIS relay. The current production path is:

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

## Build and install SysDVR SHIS Direct

The [Build SysDVR SHIS Direct](https://github.com/cyannick056-spec/Shis-Stream/actions/workflows/sysdvr-shis.yml) workflow builds the customized sysmodule from pinned SysDVR source plus [`sysdvr-shis/TCPmode.c`](sysdvr-shis/TCPmode.c).

The current package is **SysDVR SHIS Direct v0.6**.

1. Back up the existing SysDVR files on the SD card.
2. Download the `SysDVR-SHIS-Direct-v0.6` artifact from a successful workflow run.
3. Copy its `atmosphere/` and `config/` directories to the root of the SD card.
4. Edit `/config/sysdvr/shis.ini` with the relay TCP proxy host, port, stream name and private stream key.
5. Boot Atmosphère and start a game compatible with SysDVR capture.

Example configuration:

```ini
relay_host=YOUR_RELAY_TCP_HOST
relay_port=YOUR_RELAY_TCP_PORT
stream=shis
stream_key=YOUR_PRIVATE_STREAM_KEY
```

`relay_host` and `relay_port` must point to the **Railway TCP proxy for the native relay**, not the Discord Activity HTTPS URL.

`stream_key` must match the relay's server-side `STREAM_KEY`. Never commit the real value.

## Active source

- `sysdvr-shis/` — current direct SysDVR integration.
- `.github/workflows/sysdvr-shis.yml` — build/package workflow for the Switch SD-card artifact.

This customized build replaces SysDVR's normal TCP output path with the SHIS relay protocol. To restore ordinary SysDVR TCP mode, restore the official SysDVR `exefs.nsp`.

## Legacy source

The following directories are older experiments retained only for reference/rollback and are **not required by the current SHIS Stream setup**:

- `native/` — older standalone Switch forwarder.
- `app/` — older Android/Switchroot sender using MediaProjection.
- Android/native build workflows related only to those older senders.

Do not run the legacy standalone forwarder together with SysDVR SHIS Direct.

## Security

- Keep `stream_key` private.
- Do not place Discord, Cloudflare or deployment credentials on the Switch.
- Real production hostnames, ports and secrets are intentionally not stored in this public repository.
- Server-side credentials belong in Railway environment variables.

## Current project split

### This repository

Switch capture and SysDVR SHIS Direct packaging.

### [`cyannick056-spec/Discord`](https://github.com/cyannick056-spec/Discord)

Discord Activity, authorization, shared scene/editor state, Cloudflare signaling and the native Railway relay.

Public documentation reflects the current supported setup; older implementation history is intentionally kept out of the main README.
