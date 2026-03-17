#include "Processes.r"
#include "Menus.r"
#include "Dialogs.r"
#include "Windows.r"
#include "MacTypes.r"

type 'PICT' {
    hex string;
};

resource 'PICT' (128) {
    $$read("logo_stripped.pict")
};

resource 'MENU' (128) {
    128, textMenuProc;
    allEnabled, enabled;
    apple;
    {
        "About BigMaclock...", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Preferences...", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (129) {
    129, textMenuProc;
    allEnabled, enabled;
    "File";
    {
        "Quit", noIcon, "Q", noMark, plain;
    }
};

resource 'MBAR' (128) {
    { 128, 129 };
};

resource 'DLOG' (128) {
    { 40, 80, 260, 380 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    128,
    "Preferences",
    centerMainScreen
};

resource 'DITL' (128) {
    {
        { 190, 220, 210, 280 }, Button { enabled, "OK" };               /* 1 */
        { 190, 20, 210, 80 }, Button { enabled, "Cancel" };              /* 2 */
        { 8, 20, 24, 100 }, StaticText { disabled, "Alarm Time:" };      /* 3 */
        { 8, 105, 24, 135 }, EditText { enabled, "00" };                 /* 4 - HH */
        { 4, 140, 15, 160 }, Button { enabled, "+" };                    /* 5 - HH Up */
        { 17, 140, 28, 160 }, Button { enabled, "-" };                   /* 6 - HH Down */
        { 8, 165, 24, 175 }, StaticText { disabled, ":" };               /* 7 */
        { 8, 180, 24, 210 }, EditText { enabled, "00" };                 /* 8 - MM */
        { 4, 215, 15, 235 }, Button { enabled, "+" };                    /* 9 - MM Up */
        { 17, 215, 28, 235 }, Button { enabled, "-" };                   /* 10 - MM Down */
        { 34, 20, 48, 130 }, StaticText { disabled, "Enable Alarm:" };   /* 11 */
        { 34, 135, 48, 160 }, CheckBox { enabled, "" };                  /* 12 */
        { 52, 20, 66, 75 }, StaticText { disabled, "Sound:" };           /* 13 */
        { 50, 80, 70, 230 }, UserItem { enabled };                       /* 14 */
        { 0, 0, 0, 0 }, UserItem { disabled };                           /* 15 (placeholder) */
        { 0, 0, 0, 0 }, UserItem { disabled };                           /* 16 (placeholder) */
        { 74, 20, 88, 75 }, StaticText { disabled, "Format:" };          /* 17 */
        { 74, 80, 88, 160 }, RadioButton { enabled, "12 Hour" };         /* 18 */
        { 74, 160, 88, 240 }, RadioButton { enabled, "24 Hour" };        /* 19 */
        { 96, 20, 110, 130 }, StaticText { disabled, "Invert Display:" }; /* 20 */
        { 96, 135, 110, 160 }, CheckBox { enabled, "" };                  /* 21 */
        { 118, 20, 132, 110 }, StaticText { disabled, "Date Order:" };    /* 22 */
        { 118, 115, 132, 185 }, RadioButton { enabled, "DD.MM" };         /* 23 */
        { 118, 190, 132, 260 }, RadioButton { enabled, "MM.DD" };         /* 24 */
        { 140, 20, 154, 130 }, StaticText { disabled, "Gray Background:" }; /* 25 */
        { 140, 135, 154, 160 }, CheckBox { enabled, "" };                  /* 26 */
    }
};

resource 'DLOG' (129) {
    { 40, 80, 260, 360 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    129,
    "About BigMaclock",
    centerMainScreen
};

resource 'DITL' (129) {
    {
        { 180, 190, 200, 250 }, Button { enabled, "OK" };               /* 1 */
        { 10, 20, 26, 260 }, StaticText { disabled, "BigMaclock 1.0 - RetroTheory 2026" };   /* 2 */
        { 30, 20, 46, 260 }, StaticText { disabled, "A classic desk clock For Macs" }; /* 3 */
        { 50, 20, 66, 260 }, StaticText { disabled, "Press H for Happy Mac" }; /* 4 */
        { 70, 20, 86, 260 }, StaticText { disabled, "Press I for Invert" }; /* 5 */
        { 90, 20, 106, 260 }, StaticText { disabled, "Press G for Gray Background" }; /* 6 */
        { 110, 20, 126, 260 }, StaticText { disabled, "Press Q to Quit" }; /* 7 */
        { 130, 80, 146, 260 }, StaticText { disabled, "@retrotheory.bsky.social" }; /* 8 - adjust rect left instead of spaces for align */
    }
};


resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
	notHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    150 * 1024,
    150 * 1024
};

