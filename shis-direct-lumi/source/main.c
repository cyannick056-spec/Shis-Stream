#include <switch.h>
#include <stdio.h>
#include <string.h>

static void draw_header(void) {
    printf("\x1b[1;35m");
    printf("  ____  _   _ ___ ____   ____  _ _ ____\n");
    printf(" / ___|| | | |_ _/ ___| |  _ \\| | / ___|\n");
    printf(" \\___ \\| |_| || |\\___ \\ | | | | | \\___ \\n");
    printf("  ___) |  _  || | ___) || |_| | | |___) |\n");
    printf(" |____/|_| |_|___|____/ |____/|_|_|____/\n");
    printf("\x1b[0m");
    printf("\x1b[1;35m       SHIS DIRECT · LUMI EDITION\x1b[0m\n");
    printf("       Emisor experimental para SHIS Stream\n\n");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    consoleInit(NULL);
    socketInitializeDefault();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        consoleClear();
        draw_header();
        printf("  Estado: \x1b[1;33mPROTOTIPO DE INTERFAZ\x1b[0m\n\n");
        printf("  Esta app todavia NO transmite video ni audio.\n");
        printf("  Estamos preparando la app independiente sin\n");
        printf("  reemplazar el modulo oficial de SysDVR.\n\n");
        printf("  Objetivo de esta primera etapa:\n");
        printf("   [1] Configuracion del relay SHIS\n");
        printf("   [2] Diagnostico de conexion\n");
        printf("   [3] Estado del emisor y estadisticas\n\n");
        printf("  B = salir\n");
        printf("  A = actualizar pantalla\n\n");
        printf("  Importante: no instala ni modifica sysmodules.\n");

        consoleUpdate(NULL);
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_B) break;
        if (kDown & HidNpadButton_A) continue;
    }

    socketExit();
    consoleExit(NULL);
    return 0;
}
