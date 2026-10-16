# SHIS Direct Lumi — homebrew app prototype

This directory starts a separate Nintendo Switch Homebrew Menu application for SHIS Direct. It is intentionally isolated from the existing SysDVR sysmodule build.

## Current state

**Prototype only. It does not transmit audio/video yet.** The first milestone is an independently launchable NRO shell. Later milestones can add relay configuration and diagnostics, then connect a compatible capture/transport implementation.

## Safety boundary

- This app does not install, replace, or patch the official SysDVR sysmodule.
- The existing `sysdvr-shis/` transport and its Atmosphère package workflow remain untouched.
- A separate NRO icon/name does not, by itself, create a second streaming engine. Coexistence requires a separately identified sysmodule or another supported architecture; that must be verified before claiming streaming works alongside official SysDVR.

## Build

Requires devkitPro/devkita64 with libnx and the standard Switch build tools. Run `make` in this directory. The NRO and icon are build outputs, not yet a release package.

## Planned milestones

1. Launchable NRO and Lumi branding.
2. Read/write config under `sdmc:/config/shis-direct/` and safe connection diagnostics.
3. Decide and implement a coexistence-safe capture/transport architecture.
4. Only after build and device testing, package a ZIP with installation/rollback instructions.
