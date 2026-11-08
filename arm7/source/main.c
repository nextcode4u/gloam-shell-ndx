// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2005 Michael Noland (joat)
// Copyright (C) 2005 Jason Rogers (Dovoto)
// Copyright (C) 2005-2015 Dave Murphy (WinterMute)
// Copyright (C) 2023 Antonio Niño Díaz

// Default ARM7 core, modified for this frontend: enable DSi SD/IRQ/DMA access
// before storage initialization, matching the newer BlocksDS TMIO setup.
// SD write-command compatibility is implemented in sdmmc_compat.twl.c.

#include "hardware_protocol.h"
#include <nds.h>

#if defined(USE_MAXMOD) && defined(USE_LIBXM7)
#error "Only one audio library can be used"
#endif

#if defined(USE_DSWIFI)
#include <dswifi7.h>
#endif

#if defined(USE_MAXMOD)
#include <maxmod7.h>
#endif

#if defined(USE_LIBXM7)
#include <libxm7.h>

// Assign FIFO_USER_07 channel to LibXM7
#define FIFO_LIBXM7 FIFO_USER_07

void XM7_Value32Handler(u32 command, void *userdata)
{
    (void)userdata;

    XM7_ModuleManager_Type *module = (XM7_ModuleManager_Type *)command;

    if (module == NULL)
        XM7_StopModule();
    else
        XM7_PlayModule(module);
}
#endif

volatile bool exit_loop = false;

void power_button_callback(void)
{
    exit_loop = true;
}

void vblank_handler(void)
{
    inputGetAndSend();
#if defined(USE_DSWIFI)
    Wifi_Update();
#endif
}

static void frontend_hardware_request(u32 command, void *userdata)
{
    (void)userdata;
    if (command == FRONTEND_HW_WARMBOOT_CRC || command == FRONTEND_HW_AUTOLOAD_CRC) {
        bool autoload = command == FRONTEND_HW_AUTOLOAD_CRC;
        const void *record = (const void *)(autoload ? 0x02000800 : 0x02000300);
        u32 prefix = autoload ? FRONTEND_HW_AUTOLOAD_REPLY : FRONTEND_HW_WARMBOOT_REPLY;
        fifoSendValue32(FRONTEND_HW_FIFO, prefix | swiCRC16(0xFFFF, record, autoload ? 0x400 : 32));
    } else if (command == FRONTEND_HW_READ_BRIGHTNESS) {
        u32 level = 0;
        if (isDSiMode()) {
            u8 value = i2cReadRegister(I2C_PM, I2CREGPM_BACKLIGHT);
            if (value <= 4)
                level = value + 1;
        }
        fifoSendValue32(FRONTEND_HW_FIFO, FRONTEND_HW_BRIGHTNESS_REPLY | level);
    }
}

int main(void)
{
#if defined(DEBUG_LIBS)
    // The ARM9 exception handler can trap data aborts and undefined instruction
    // exceptions. However, the ARM7 handler can only trap undefined
    // instructions. When an exception happens, a message is sent to the ARM9 to
    // display the crash information.
    defaultExceptionHandler();
#endif

    // Initialize sound hardware
    // Required for some functions within libnds
    enableSound();

    // Read user information from the firmware (name, birthday, etc)
    readUserSettings();

    // Stop LED blinking
    ledBlink(0);

    // Using the calibration values read from the firmware with
    // readUserSettings(), calculate some internal values to convert raw
    // coordinates into screen coordinates.
    touchInit();

    if (isDSiMode() && (REG_SCFG_EXT & SCFG_EXT_SCFG_MBK_REG))
        REG_SCFG_EXT |= SCFG_EXT_SDMMC | SCFG_EXT_INTERRUPT | SCFG_EXT_AES | SCFG_EXT_DMA;

    irqInit();
    fifoInit();

#if defined(USE_DSWIFI)
    installWifiFIFO();
#endif

    // Required for some functions within libnds
    installSoundFIFO();

    fifoSetValue32Handler(FRONTEND_HW_FIFO, frontend_hardware_request, 0);
    installSystemFIFO(); // Sleep mode, storage, firmware...
    if (isDSiMode())
        installCameraFIFO();

#if defined(USE_MAXMOD)
    // Initialize Maxmod. It uses timer 0 internally.
    mmInstall(FIFO_MAXMOD);
#endif

#if defined(USE_LIBXM7)
    // Initialize LibXM7. It uses timer 0 internally.
    XM7_Initialize();
    // Setup the FIFO handler for LibXM7
    fifoSetValue32Handler(FIFO_LIBXM7, XM7_Value32Handler, 0);
#endif

    // This sets a callback that is called when the power button in a DSi
    // console is pressed. It has no effect in a DS.
    setPowerButtonCB(power_button_callback);

    // Read current date from the RTC and setup an interrupt to update the time
    // regularly. The interrupt simply adds one second every time, it doesn't
    // read the date. Reading the RTC is very slow, so it's a bad idea to do it
    // frequently.
    initClockIRQTimer(LIBNDS_DEFAULT_TIMER_RTC);

    // Now that the FIFO is setup we can start sending input data to the ARM9.
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);

    while (!exit_loop) {
        const uint16_t key_mask = KEY_SELECT | KEY_START | KEY_L | KEY_R;
        uint16_t keys_pressed = ~REG_KEYINPUT;

        if ((keys_pressed & key_mask) == key_mask)
            exit_loop = true;

        swiWaitForVBlank();
    }

    return 0;
}
