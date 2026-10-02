# SHIS Stream — Emisor SysDVR para Nintendo Switch

Este repositorio contiene la parte de **Nintendo Switch** de SHIS Stream: **SysDVR SHIS Direct v0.6** para una consola con Atmosphère.

Su trabajo es capturar vídeo y audio del juego con SysDVR y enviarlos directamente al relay de SHIS Stream.

## Cómo funciona

```text
Nintendo Switch
  └─ SysDVR SHIS Direct
       ├─ vídeo H.264
       └─ audio PCM
          ↓ TCP
Relay nativo en Railway
          ↓ WebRTC
Cloudflare Realtime SFU
          ↓
Activity de SHIS Stream en Discord
```

La Switch no ejecuta Discord y no guarda credenciales de Cloudflare. Solo necesita la dirección TCP del relay, el puerto, el nombre lógico del stream y una clave privada.

La Activity, el backend y el relay están en [`cyannick056-spec/Discord`](https://github.com/cyannick056-spec/Discord).

## Contenido del repositorio

- `sysdvr-shis/TCPmode.c` — transporte SHIS integrado en SysDVR.
- `.github/workflows/sysdvr-shis.yml` — workflow que compila y empaqueta el módulo para la tarjeta SD.

El repositorio contiene únicamente lo necesario para el emisor SysDVR usado por SHIS Stream.

## Compilar

El workflow **Build SysDVR SHIS Direct** descarga la versión fijada de SysDVR, integra `sysdvr-shis/TCPmode.c` y genera el paquete para Atmosphère.

Puedes ejecutarlo desde GitHub Actions:

[`Build SysDVR SHIS Direct`](https://github.com/cyannick056-spec/Shis-Stream/actions/workflows/sysdvr-shis.yml)

El artefacto generado se llama `SysDVR-SHIS-Direct-v0.6`.

## Instalar en la Switch

1. Descarga el artefacto `SysDVR-SHIS-Direct-v0.6` de un workflow correcto.
2. Copia sus carpetas `atmosphere/` y `config/` a la raíz de la tarjeta SD.
3. Edita `/config/sysdvr/shis.ini`.
4. Reinicia Atmosphère o reinicia el sysmodule correspondiente.
5. Abre un juego compatible con la captura de SysDVR.

## Configuración

Ejemplo de `/config/sysdvr/shis.ini`:

```ini
relay_host=TU_HOST_TCP_DE_RAILWAY
relay_port=TU_PUERTO_TCP
stream=shis
stream_key=TU_CLAVE_PRIVADA
```

### `relay_host`

Host del **proxy TCP del relay nativo en Railway**. No es la URL HTTPS de la Activity.

### `relay_port`

Puerto público del proxy TCP del relay.

### `stream`

Nombre lógico de la transmisión. Debe coincidir con la configuración del sistema SHIS Stream.

### `stream_key`

Clave privada usada para autenticar la publicación de la Switch. Debe coincidir con `STREAM_KEY` en el relay.

## Datos enviados

SysDVR SHIS Direct entrega al relay:

- vídeo H.264;
- audio PCM;
- marcas de tiempo necesarias para mantener la reproducción estable.

El relay se encarga de convertir el audio a Opus y de publicar el stream hacia Cloudflare Realtime SFU. La Switch no necesita implementar WebRTC ni conocer las credenciales del SFU.

## Seguridad

- Mantén `stream_key` en privado.
- No guardes secretos de Discord o Cloudflare en la Switch.
- No publiques los endpoints y claves reales de producción en este repositorio.
- Las credenciales de servidor pertenecen a las variables de entorno de Railway.

## Repositorios de SHIS Stream

**Este repositorio:** captura y envío desde Nintendo Switch mediante SysDVR SHIS Direct.

**[`cyannick056-spec/Discord`](https://github.com/cyannick056-spec/Discord):** Activity de Discord, backend, escenas, editor, autorización, Cloudflare Realtime SFU y relay nativo de Railway.
