/**
 * Big Clock for Macintosh Plus
 *
 * A full-screen clock application designed for early Macintosh systems.
 * Features:
 * - Large digitized digits for time.
 * - Date and Day of the week display.
 * - Double-buffered rendering to prevent flickering on the slow screen refresh.
 * - Customizable preferences (24h/12h, Inverted, Gray Background).
 * - Alarm functionality with multiple sound options.
 * - Dynamic Resolution Scaling (supports Compact Macs and 640x480 displays).
 * - Sticky menu bar reveal on hover/click.
 */

#include "bitmaps.h"
#include <Devices.h>
#include <Dialogs.h>
#include <Events.h>
#include <Files.h>
#include <Fonts.h>
#include <Menus.h>
#include <OSUtils.h>
#include <Quickdraw.h>
#include <Sound.h>
#include <TextEdit.h>
#include <TextUtils.h>
#include <ToolUtils.h>
#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Configuration ---

// --- Layout & Scaling Globals ---
// These variables are set dynamically in UpdateLayout() based on screen
// resolution.
int gPixelWidth = 8;
int gPixelHeight = 8;
#define DIGIT_WIDTH 11
#define DIGIT_HEIGHT 18
int gDigitPixelWidth;  // Calculated: DIGIT_WIDTH * gPixelWidth
int gDigitPixelHeight; // Calculated: DIGIT_HEIGHT * gPixelHeight
int gSpaceBetweenDigits = 24;

#define COLON_WIDTH 3
int gColonPixelWidth;

// Small Font Configuration (used for date, day, and alarm icon)
int gSmallPixelSize = 8;
#define SMALL_WIDTH 5
#define SMALL_HEIGHT 7
int gSmallPixelWidth;
int gSmallPixelHeight;
int gSmallSpacing = 8; // Horizontal spacing between characters

const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

// --- Forward Declarations ---
void BeginDraw();
void EndDraw();
void BlitToScreen();
void RedrawAll();
void UpdateLayout(Rect bounds);
void DrawSmallString(const char *s, int x, int y);

/**
 * Draws a block of pixels at a specific row/col within a bitmap.
 * Each "pixel" is scaled by w and h.
 * Insets the rectangle by 1px to create a subtle grid/border effect.
 */
void DrawPixelGroup(int col, int row, int startX, int startY, int w, int h) {
  Rect r;
  r.left = startX + (col * w);
  r.top = startY + (row * h);
  r.right = r.left + w;
  r.bottom = r.top + h;
  InsetRect(&r, 1, 1);
  PaintRect(&r);
}

/**
 * Draws a single large digit from the `digits` array.
 */
void DrawDigit(int n, int startX, int startY) {
  Rect r = {startY, startX, startY + gDigitPixelHeight,
            startX + gDigitPixelWidth};
  EraseRect(&r);
  for (int row = 0; row < 18; row++) {
    uint16_t rowData = digits[n][row];
    for (int col = 0; col < 11; col++) {
      if (rowData & (1 << (10 - col)))
        DrawPixelGroup(col, row, startX, startY, gPixelWidth, gPixelHeight);
    }
  }
}

/**
 * Draws the colon (:) separator between hours and minutes.
 */
void DrawColon(int startX, int startY, uint8_t visible) {
  Rect r = {startY, startX, startY + gDigitPixelHeight,
            startX + gColonPixelWidth};
  EraseRect(&r);
  if (visible) {
    for (int row = 0; row < 18; row++) {
      uint8_t rowData = colonBitmap[row];
      for (int col = 0; col < 3; col++) {
        if (rowData & (1 << (2 - col)))
          DrawPixelGroup(col, row, startX, startY, gPixelWidth, gPixelHeight);
      }
    }
  }
}

/**
 * Draws a single character using the small font bitmaps.
 */
void DrawSmallChar(char c, int x, int y) {
  const uint8_t *bitmap = NULL;
  if (c >= '0' && c <= '9')
    bitmap = smallDigits[c - '0'];
  else if (c >= 'A' && c <= 'Z')
    bitmap = smallLetters[c - 'A'];
  else if (c >= 'a' && c <= 'z')
    bitmap = smallLetters[c - 'a'];
  if (c == 'c' || c == 'C')
    bitmap = smallDegC;
  if (c == '.')
    bitmap = smallDot;
  if (c == '-')
    bitmap = smallDash;
  if (!bitmap)
    return;

  for (int row = 0; row < 7; row++) {
    uint8_t rowData = bitmap[row];
    for (int col = 0; col < 5; col++) {
      if (rowData & (1 << (4 - col)))
        DrawPixelGroup(col, row, x, y, gSmallPixelSize, gSmallPixelSize);
    }
  }
}

int GetSmallCharWidth(char c) {
  if (c == '.')
    return 2;
  if (c == ' ')
    return 3;
  if (c == '-')
    return 5;
  return 5;
}

void DrawSmallString(const char *s, int x, int y) {
  while (*s) {
    if (*s != ' ')
      DrawSmallChar(*s, x, y);
    x += (GetSmallCharWidth(*s) * gSmallPixelSize) + gSmallSpacing;
    s++;
  }
}

static void MyCopyCStringToPascal(const char *src, Str255 dst) {
  size_t len = strlen(src);
  if (len > 255)
    len = 255;
  dst[0] = (unsigned char)len;
  memcpy(&dst[1], src, len);
}

static void CopyPascalToCString(ConstStr255Param src, char *dst) {
  size_t len = src[0];
  memcpy(dst, &src[1], len);
  dst[len] = '\0';
}

// --- Preferences & State ---

/**
 * Record structure for saving preferences to disk.
 * Uses a signature and version for compatibility checks.
 */
typedef struct {
  char signature[4]; // 'BGCK'
  char version;      // Format version
  Boolean alarmEnabled;
  int alarmHour;
  int alarmMinute;
  int alarmSoundType; // Deprecated
  Boolean is24Hour;
  Boolean inverted;
  Boolean dateFormatUS;
  Boolean grayBackground;
  Str255 alarmSoundName; // v4+
} PrefsRecord;

const char kPrefsSignature[4] = {'B', 'G', 'C', 'K'};
const char kPrefsVersion = 4;

// --- Globals ---

// Alarm State
Boolean gAlarmEnabled = false;
int gAlarmHour = 0;
int gAlarmMinute = 0;
Boolean gAlarmTriggered = false;
int gAlarmSoundType = 0; // Legacy fallback
Str255 gAlarmSoundName;
Boolean gIs24Hour = true;
Boolean gIsPM = false;

// Display State
Boolean gInverted = false;
Boolean gDateFormatUS = false; // false = DD.MM (EU), true = MM.DD (US)
Boolean gShowingFace = false;
Boolean gGrayBackground = false;

// Menu State
unsigned long gLastMenuTime = 0;
unsigned long gLastMouseMoveTime = 0;
Boolean gMenuVisible = false;
Boolean gCursorHidden = false;
Boolean gMenuSticky = false; // If true, menu stays visible until clicked again
short gOrigMBarHeight = 20;  // Original height to restore

// Drawing Globals
int g_startX, g_startY;
int g_h1_x, g_h2_x, g_col_x, g_m1_x, g_m2_x;
int g_prev_h1 = -1, g_prev_h2 = -1, g_prev_m1 = -1, g_prev_m2 = -1,
    g_prev_blink = -1;
int g_prev_day = -1, g_prev_month = -1, g_prev_dow = -1;
WindowPtr gWindow;

// Double Buffer Globals
// We manually manage an offscreen GrafPort for the Macintosh Plus
// as it doesn't support the newer GWorld architecture.
GrafPort gOffPort;
BitMap gOffBits;
Ptr gOffBaseAddr = NULL;
int gOffRowBytes = 0;
Boolean gHasOffscreen = false;

/**
 * Saves application preferences to a file named "BigMaclock Prefs"
 * in the system folder (or current volume).
 */
void SavePrefs() {
  short vRefNum;
  long dirID;
  OSErr err;

  err = GetVol(NULL, &vRefNum);
  if (err != noErr)
    return;
  dirID = 0;

  FSSpec spec;
  err = FSMakeFSSpec(vRefNum, dirID, "\pBigMaclock Prefs", &spec);

  short refNum;
  if (err == fnfErr) {
    err = FSpCreate(&spec, 'BClk', 'pref', smSystemScript);
    if (err != noErr)
      return;
  } else if (err == noErr) {
    FSpDelete(&spec);
    err = FSpCreate(&spec, 'BClk', 'pref', smSystemScript);
    if (err != noErr)
      return;
  } else {
    return;
  }

  err = FSpOpenDF(&spec, fsRdWrPerm, &refNum);
  if (err != noErr)
    return;

  PrefsRecord prefs;
  prefs.signature[0] = kPrefsSignature[0];
  prefs.signature[1] = kPrefsSignature[1];
  prefs.signature[2] = kPrefsSignature[2];
  prefs.signature[3] = kPrefsSignature[3];
  prefs.version = kPrefsVersion;
  prefs.alarmEnabled = gAlarmEnabled;
  prefs.alarmHour = gAlarmHour;
  prefs.alarmMinute = gAlarmMinute;
  prefs.alarmSoundType = gAlarmSoundType;
  prefs.is24Hour = gIs24Hour;
  prefs.inverted = gInverted;
  prefs.dateFormatUS = gDateFormatUS;

  prefs.grayBackground = gGrayBackground;
  memcpy(prefs.alarmSoundName, gAlarmSoundName, 256);

  long count = sizeof(PrefsRecord);
  err = FSWrite(refNum, &count, &prefs);

  FSClose(refNum);
}

/**
 * Loads application preferences from disk.
 * Performs validation on the signature and version.
 */
void LoadPrefs() {
  short vRefNum;
  long dirID;
  OSErr err;

  err = GetVol(NULL, &vRefNum);
  if (err != noErr)
    return;
  dirID = 0;

  FSSpec spec;
  err = FSMakeFSSpec(vRefNum, dirID, "\pBigMaclock Prefs", &spec);
  if (err != noErr)
    return;

  short refNum;
  err = FSpOpenDF(&spec, fsRdPerm, &refNum);
  if (err != noErr)
    return;

  PrefsRecord prefs;
  long count = sizeof(PrefsRecord);
  err = FSRead(refNum, &count, &prefs);
  FSClose(refNum);

  if (err != noErr)
    return;

  if (prefs.signature[0] != kPrefsSignature[0] ||
      prefs.signature[1] != kPrefsSignature[1] ||
      prefs.signature[2] != kPrefsSignature[2] ||
      prefs.signature[3] != kPrefsSignature[3])
    return;

  if (prefs.version < 1 || prefs.version > kPrefsVersion)
    return;

  gAlarmEnabled = prefs.alarmEnabled;
  gAlarmHour = prefs.alarmHour;
  gAlarmMinute = prefs.alarmMinute;
  gAlarmSoundType = prefs.alarmSoundType;
  gIs24Hour = prefs.is24Hour;
  gInverted = prefs.inverted;
  gDateFormatUS = prefs.dateFormatUS;

  if (prefs.version >= 3) {
    gGrayBackground = prefs.grayBackground;
  }

  if (prefs.version >= 4) {
    memcpy(gAlarmSoundName, prefs.alarmSoundName, 256);
  } else {
    if (gAlarmSoundType == 1)
      MyCopyCStringToPascal("Staccato", gAlarmSoundName);
    else if (gAlarmSoundType == 2)
      MyCopyCStringToPascal("Pulsar", gAlarmSoundName);
    else
      MyCopyCStringToPascal("Simple Beep", gAlarmSoundName);
  }
}

/**
 * Initializes the offscreen buffer.
 * Allocates memory for the bitmap and opens a custom GrafPort.
 */
void InitOffscreen(Rect bounds) {
  long screenSize = bounds.right * bounds.bottom / 8;

  gOffRowBytes = (bounds.right + 15) & ~15;
  gOffRowBytes >>= 3;

  gOffBaseAddr = NewPtr(screenSize);
  if (gOffBaseAddr == NULL) {
    gHasOffscreen = false;
    return;
  }

  gOffBits.baseAddr = gOffBaseAddr;
  gOffBits.rowBytes = gOffRowBytes;
  gOffBits.bounds = bounds;

  OpenPort(&gOffPort);
  SetPortBits(&gOffBits);
  PortSize(bounds.right, bounds.bottom);

  gHasOffscreen = true;
}

void DisposeOffscreen() {
  if (gOffBaseAddr) {
    DisposePtr(gOffBaseAddr);
    gOffBaseAddr = NULL;
  }
  ClosePort(&gOffPort);
  gHasOffscreen = false;
}

/**
 * Prepares the offscreen buffer for drawing.
 * Sets the current port to the offscreen buffer and sets the
 * background pattern based on the current theme (Black, Gray, or White).
 */
void BeginDraw() {
  if (gHasOffscreen) {
    SetPort(&gOffPort);
    if (gInverted) {
      BackPat(&qd.black);
      PenPat(&qd.white);
    } else if (gGrayBackground) {
      BackPat(&qd.gray);
      PenPat(&qd.black);
    } else {
      BackPat(&qd.white);
      PenPat(&qd.black);
    }
  }
}

/**
 * Restores the current port to the main window.
 */
void EndDraw() {
  if (gHasOffscreen) {
    SetPort(gWindow);
  }
}

/**
 * Copies the contents of the offscreen buffer to the main window.
 * This is where the actual pixel data is pushed to the hardware.
 */
void BlitToScreen() {
  if (gHasOffscreen && gOffBaseAddr) {
    Rect bounds = gWindow->portRect;
    gOffBits.bounds = bounds;
    CopyBits(&gOffBits, &gWindow->portBits, &bounds, &bounds, srcCopy, NULL);
  }
}

void DrawHappyMac() {
  int facePixelSize = 12; // Larger pixels for the face
  int facePixelWidth = FACE_SIZE * facePixelSize;
  int facePixelHeight = FACE_SIZE * facePixelSize;

  int startX =
      (gWindow->portRect.right - gWindow->portRect.left - facePixelWidth) / 2;
  int startY =
      (gWindow->portRect.bottom - gWindow->portRect.top - facePixelHeight) / 2;

  EraseRect(&gWindow->portRect);

  for (int row = 0; row < FACE_SIZE; row++) {
    uint32_t rowData = faceBitmap[row];
    for (int col = 0; col < FACE_SIZE; col++) {
      if (rowData & (1L << (FACE_SIZE - 1 - col)))
        DrawPixelGroup(col, row, startX, startY, facePixelSize, facePixelSize);
    }
  }
}

void DrawAlarmIcon() {
  int bellX = g_m2_x + gDigitPixelWidth - (5 * gSmallPixelSize);
  int bellY = 34;
  Rect r = {bellY, bellX, bellY + 7 * gSmallPixelSize,
            bellX + 5 * gSmallPixelSize};
  EraseRect(&r);
  if (gAlarmEnabled) {
    for (int row = 0; row < 7; row++) {
      uint8_t rowData = smallBell[row];
      for (int col = 0; col < 5; col++) {
        if (rowData & (1 << (4 - col)))
          DrawPixelGroup(col, row, bellX, bellY, gSmallPixelSize,
                         gSmallPixelSize);
      }
    }
  }
}

/**
 * Calculates the horizontal and vertical positions of all digits
 * based on the current window size and 12h/24h setting.
 *
 * Dynamic Resolution Support:
 * - Dectects if the screen is 640x480 (typical for Mac II/LC) or
 *   the standard 512x342 (compact Macs).
 * - Adjusts gPixelWidth/Height and gSmallPixelSize to scale the clock.
 * - Recalculates all dependent pixel dimensions.
 */
void UpdateLayout(Rect bounds) {
  int screenWidth = bounds.right - bounds.left;
  int screenHeight = bounds.bottom - bounds.top;

  // Dynamic Scaling Detection
  if (screenWidth >= 640 && screenHeight >= 480) {
    gPixelWidth = 10;
    gPixelHeight = 10;
    gSpaceBetweenDigits = 32;
    gSmallPixelSize = 10;
    gSmallSpacing = 10;
  } else {
    // Default (Mac Plus / 512x342)
    gPixelWidth = 8;
    gPixelHeight = 8;
    gSpaceBetweenDigits = 24;
    gSmallPixelSize = 8;
    gSmallSpacing = 8;
  }

  // Recalculate derived sizes
  gDigitPixelWidth = DIGIT_WIDTH * gPixelWidth;
  gDigitPixelHeight = DIGIT_HEIGHT * gPixelHeight;
  gColonPixelWidth = COLON_WIDTH * gPixelWidth;
  gSmallPixelWidth = SMALL_WIDTH * gSmallPixelSize;
  gSmallPixelHeight = SMALL_HEIGHT * gSmallPixelSize;

  int digitsWidth =
      (4 * gDigitPixelWidth) + gColonPixelWidth + (4 * gSpaceBetweenDigits);
  int ampmWidth = (5 * gSmallPixelSize);
  int ampmSpacing = -(gSmallPixelSize * 4); // Adjusted spacing
  int totalWidth = digitsWidth;
  if (!gIs24Hour)
    totalWidth += ampmWidth + ampmSpacing;

  g_startX = (screenWidth - totalWidth) / 2;
  int digitsStart = g_startX;
  if (!gIs24Hour)
    digitsStart += ampmWidth + ampmSpacing;

  g_h1_x = digitsStart;
  g_h2_x = g_h1_x + gDigitPixelWidth + gSpaceBetweenDigits;
  g_col_x = g_h2_x + gDigitPixelWidth + gSpaceBetweenDigits;
  g_m1_x = g_col_x + gColonPixelWidth + gSpaceBetweenDigits;
  g_m2_x = g_m1_x + gDigitPixelWidth + gSpaceBetweenDigits;

  g_startY = ((screenHeight - gDigitPixelHeight) / 2) + 7;
}

/**
 * Renders the AM/PM indicator for 12-hour mode.
 */
void DrawAMPM(Boolean isPM, int x, int y) {
  Rect r = {y, x, y + 2 * gSmallPixelHeight + 8, x + 5 * gSmallPixelSize};
  EraseRect(&r);
  DrawSmallChar(isPM ? 'P' : 'A', x, y);
  DrawSmallChar('M', x, y + gSmallPixelHeight + 8);
}

/**
 * Draws decorative black corners on the full-screen display.
 */
void DrawCorners() {
  static const short cornerData[8] = {8, 6, 4, 3, 2, 2, 1, 1};
  short screenWidth = gWindow->portRect.right;
  for (short row = 0; row < 8; row++) {
    Rect pixel = {row, 0, row + 1, cornerData[row]};
    FillRect(&pixel, &qd.black);
    Rect pixelR = {row, screenWidth - cornerData[row], row + 1, screenWidth};
    FillRect(&pixelR, &qd.black);
  }
}

void FormatDate(DateTimeRec *date, char *buf) {
  int p = 0;
  int first = gDateFormatUS ? date->month : date->day;
  int second = gDateFormatUS ? date->day : date->month;
  if (first >= 10)
    buf[p++] = (first / 10) + '0';
  buf[p++] = (first % 10) + '0';
  buf[p++] = '.';
  if (second >= 10)
    buf[p++] = (second / 10) + '0';
  buf[p++] = (second % 10) + '0';
  buf[p] = '\0';
}

int GetDateStringWidth(const char *buf) {
  int w = 0;
  for (int i = 0; buf[i]; i++)
    w += (GetSmallCharWidth(buf[i]) * gSmallPixelSize) + gSmallSpacing;
  return w - gSmallSpacing;
}

/**
 * The main rendering entry point.
 * Clears the screen and draws all elements: hours, minutes, colon,
 * date, day of week, and alarm icon.
 * Uses double-buffering (BeginDraw/EndDraw/BlitToScreen).
 */
void RedrawAll() {
  BeginDraw();

  EraseRect(&gWindow->portRect);
  DrawCorners();

  DrawSmallString("20.5C", 20, 34);

  if (gShowingFace) {
    DrawHappyMac();
    EndDraw();
    BlitToScreen();
    return;
  }

  g_prev_h1 = g_prev_h2 = g_prev_m1 = g_prev_m2 = g_prev_blink = -1;
  g_prev_day = g_prev_month = g_prev_dow = -1;
  Rect bounds = gWindow->portRect;

  DrawAlarmIcon();

  unsigned long now;
  GetDateTime(&now);
  unsigned long secsSinceMidnight = now % 86400;
  int rawHour = secsSinceMidnight / 3600;
  int displayHour = rawHour;
  if (!gIs24Hour) {
    displayHour = rawHour % 12;
    if (displayHour == 0)
      displayHour = 12;
  }
  int h1 = displayHour / 10;
  int h2 = displayHour % 10;
  int minute = (secsSinceMidnight % 3600) / 60;
  int m1 = minute / 10;
  int m2 = minute % 10;
  int blink = now % 2;

  if (!gIs24Hour && h1 == 0) {
    // Skip drawing h1
  } else {
    DrawDigit(h1, g_h1_x, g_startY);
  }

  if (!gIs24Hour) {
    DrawAMPM(gIsPM, g_startX,
             g_startY + (gDigitPixelHeight - (2 * gSmallPixelHeight + 8)) / 2);
  }

  DrawDigit(h2, g_h2_x, g_startY);
  DrawColon(g_col_x, g_startY, blink);
  DrawDigit(m1, g_m1_x, g_startY);
  DrawDigit(m2, g_m2_x, g_startY);

  g_prev_h1 = h1;
  g_prev_h2 = h2;
  g_prev_m1 = m1;
  g_prev_m2 = m2;
  g_prev_blink = blink;

  // Draw date/day
  DateTimeRec date;
  SecondsToDate(now, &date);
  DrawSmallString(days[date.dayOfWeek - 1], 20,
                  bounds.bottom - gSmallPixelHeight - 20);

  char dateBuf[16];
  FormatDate(&date, dateBuf);
  int br_width = GetDateStringWidth(dateBuf);
  DrawSmallString(dateBuf, bounds.right - br_width - 20,
                  bounds.bottom - gSmallPixelHeight - 20);

  g_prev_day = date.day;
  g_prev_month = date.month;
  g_prev_dow = date.dayOfWeek;

  EndDraw();
  BlitToScreen();
}

/**
 * Reveals the menu bar.
 * If sticky is true, it remains until a manual hide action.
 * Otherwise, it hides automatically after mouse movement.
 */
void ShowMenu(Boolean sticky) {
  if (!gMenuVisible) {
    *(short *)0x0BAA = gOrigMBarHeight;
    DrawMenuBar();
    gMenuVisible = true;
  }
  gLastMenuTime = TickCount();
  if (sticky)
    gMenuSticky = true;
}

/**
 * Custom function to paint the menu bar area with the current theme
 * when the system draws it, to maintain a seamless look.
 */
void UpdateMenuBarBackground() {
  GrafPtr oldPort;
  GrafPtr wmPort;
  GetPort(&oldPort);
  GetWMgrPort(&wmPort);
  SetPort(wmPort);

  if (gInverted) {
    BackPat(&qd.black);
  } else if (gGrayBackground) {
    BackPat(&qd.gray);
  } else {
    BackPat(&qd.white);
  }

  Rect r = {0, 0, gOrigMBarHeight, qd.screenBits.bounds.right};
  EraseRect(&r);

  int cornerData[8] = {8, 6, 4, 3, 2, 2, 1, 1};

  for (int row = 0; row < 8; row++) {
    Rect pixel = {row, 0, row + 1, cornerData[row]};
    FillRect(&pixel, &qd.black);
  }

  int screenWidth = qd.screenBits.bounds.right;
  for (int row = 0; row < 8; row++) {
    Rect pixel = {row, screenWidth - cornerData[row], row + 1, screenWidth};
    FillRect(&pixel, &qd.black);
  }

  SetPort(oldPort);
}

/**
 * Hides the menu bar and restores the full-screen clock look.
 */
void HideMenu() {
  if (gMenuVisible) {
    *(short *)0x0BAA = 0;
    DrawMenuBar();
    gMenuVisible = false;
    gMenuSticky = false;

    UpdateMenuBarBackground();
    RedrawAll();
  }
}

pascal Boolean AlarmFilter(DialogPtr d, EventRecord *event, short *itemHit) {
  if (event->what == keyDown || event->what == autoKey) {
    char key = event->message & charCodeMask;
    if (key == 30 || key == 31) { // Up or Down arrow
      short field = ((DialogPeek)d)->editField + 1;
      if (field == 4 || field == 8) { // HH or MM field
        short itemType;
        Handle itemH;
        Rect itemR;
        Str255 macStr;
        char buf[8];
        int val;

        GetDialogItem(d, field, &itemType, &itemH, &itemR);
        GetDialogItemText(itemH, macStr);
        CopyPascalToCString(macStr, buf);
        val = atoi(buf);

        if (key == 30)
          val++;
        else
          val--;

        if (field == 4) { // Hour
          if (val > 23)
            val = 0;
          if (val < 0)
            val = 23;
        } else { // Minute
          if (val > 59)
            val = 0;
          if (val < 0)
            val = 59;
        }

        sprintf(buf, "%02d", val);
        MyCopyCStringToPascal(buf, macStr);
        SetDialogItemText(itemH, macStr);
        SelectDialogItemText(d, field, 0, 32767); // Select all
        *itemHit = 0;                             // Avoid garbage hit
        return true;                              // Handled
      }
    }
  }
  return false;
}

pascal void DrawSoundDropdown(WindowPtr theWindow, short itemNo) {
  short itemType;
  Handle itemH;
  Rect itemR;
  GetDialogItem((DialogPtr)theWindow, itemNo, &itemType, &itemH, &itemR);

  EraseRect(&itemR);

  Rect boxR = itemR;
  boxR.right -= 1;
  boxR.bottom -= 1;
  FrameRect(&boxR);

  MoveTo(boxR.left + 2, boxR.bottom);
  LineTo(boxR.right, boxR.bottom);
  MoveTo(boxR.right, boxR.top + 2);
  LineTo(boxR.right, boxR.bottom - 1);

  MoveTo(boxR.left + 5, boxR.bottom - 5);
  TextFont(0);
  DrawString(gAlarmSoundName);

  int triX = boxR.right - 14;
  int triY = boxR.top + (boxR.bottom - boxR.top) / 2 - 1;
  MoveTo(triX, triY);
  LineTo(triX + 8, triY);
  MoveTo(triX + 1, triY + 1);
  LineTo(triX + 7, triY + 1);
  MoveTo(triX + 2, triY + 2);
  LineTo(triX + 6, triY + 2);
  MoveTo(triX + 3, triY + 3);
  LineTo(triX + 5, triY + 3);
  MoveTo(triX + 4, triY + 4);
  LineTo(triX + 4, triY + 4);
}

/**
 * Displays the preferences dialog and handles user input.
 * Updates global state and saves preferences on "OK".
 */
void DoPreferences() {
  if (gCursorHidden) {
    ShowCursor();
    gCursorHidden = false;
  }
  DialogPtr d = GetNewDialog(128, NULL, (WindowPtr)-1);
  short itemHit = 0;
  char buf[32];
  Str255 macStr;

  // Set initial values
  sprintf(buf, "%02d", gAlarmHour);
  MyCopyCStringToPascal(buf, macStr);
  short itemType;
  Handle itemH;
  Rect itemR;
  GetDialogItem(d, 4, &itemType, &itemH, &itemR);
  SetDialogItemText(itemH, macStr);

  sprintf(buf, "%02d", gAlarmMinute);
  MyCopyCStringToPascal(buf, macStr);
  GetDialogItem(d, 8, &itemType, &itemH, &itemR);
  SetDialogItemText(itemH, macStr);

  GetDialogItem(d, 12, &itemType, &itemH, &itemR);
  SetControlValue((ControlHandle)itemH, gAlarmEnabled);

  // Set sound selection UserItem
  GetDialogItem(d, 14, &itemType, &itemH, &itemR);
  SetDialogItem(d, 14, itemType, (Handle)NewUserItemUPP(DrawSoundDropdown),
                &itemR);

  // Set format selection
  GetDialogItem(d, 18, &itemType, &itemH, &itemR);
  SetControlValue((ControlHandle)itemH, !gIs24Hour);
  GetDialogItem(d, 19, &itemType, &itemH, &itemR);
  SetControlValue((ControlHandle)itemH, gIs24Hour);

  // Set invert selection
  GetDialogItem(d, 21, &itemType, &itemH, &itemR);
  SetControlValue((ControlHandle)itemH, gInverted);

  // Set date order selection
  GetDialogItem(d, 23, &itemType, &itemH, &itemR);
  SetControlValue((ControlHandle)itemH, !gDateFormatUS);
  GetDialogItem(d, 24, &itemType, &itemH, &itemR);
  SetControlValue((ControlHandle)itemH, gDateFormatUS);

  // Set gray background selection
  GetDialogItem(d, 26, &itemType, &itemH, &itemR);
  SetControlValue((ControlHandle)itemH, gGrayBackground);

  while (itemHit != 1 && itemHit != 2) {
    ModalDialog(AlarmFilter, &itemHit);
    if (itemHit == 12 || itemHit == 21 || itemHit == 26) { // Toggle checkboxes
      GetDialogItem(d, itemHit, &itemType, &itemH, &itemR);
      SetControlValue((ControlHandle)itemH,
                      !GetControlValue((ControlHandle)itemH));
    } else if (itemHit == 14) { // Sound Dropdown Button
      MenuHandle sndMenu = NewMenu(200, "\pSound");
      int count = CountResources('snd ');
      int menuIdx = 1;
      for (int i = 1; i <= count; ++i) {
        Handle h = GetIndResource('snd ', i);
        if (h) {
          short id;
          ResType type;
          Str255 name;
          GetResInfo(h, &id, &type, name);
          if (name[0] > 0) {
            AppendMenu(sndMenu, "\p ");
            SetMenuItemText(sndMenu, menuIdx, name);
            if (EqualString(name, gAlarmSoundName, false, false)) {
              CheckItem(sndMenu, menuIdx, true);
            }
            menuIdx++;
          }
        }
      }
      InsertMenu(sndMenu, -1);

      GetDialogItem(d, 14, &itemType, &itemH, &itemR);
      Point pt = {itemR.top, itemR.left};
      LocalToGlobal(&pt);

      long result = PopUpMenuSelect(sndMenu, pt.v, pt.h, 0);
      short menuItem = LoWord(result);
      if (menuItem > 0) {
        GetMenuItemText(sndMenu, menuItem, gAlarmSoundName);
        GrafPtr oldPort;
        GetPort(&oldPort);
        SetPort((GrafPtr)d);
        DrawSoundDropdown(d, 14);
        SetPort(oldPort);

        // Preview sound
        Handle h = GetNamedResource('snd ', gAlarmSoundName);
        if (h) {
          LoadResource(h);
          SndPlay(NULL, h, false);
        }
      }

      DeleteMenu(200);
      DisposeMenu(sndMenu);
    } else if (itemHit == 5 || itemHit == 6 || itemHit == 9 || itemHit == 10) {
      // Arrow buttons
      int field = (itemHit <= 6) ? 4 : 8;
      int delta = (itemHit == 5 || itemHit == 9) ? 1 : -1;

      GetDialogItem(d, field, &itemType, &itemH, &itemR);
      GetDialogItemText(itemH, macStr);
      CopyPascalToCString(macStr, buf);
      int val = atoi(buf);
      val += delta;

      if (field == 4) { // Hour
        if (val > 23)
          val = 0;
        if (val < 0)
          val = 23;
      } else { // Minute
        if (val > 59)
          val = 0;
        if (val < 0)
          val = 59;
      }

      sprintf(buf, "%02d", val);
      MyCopyCStringToPascal(buf, macStr);
      SetDialogItemText(itemH, macStr);
      SelectDialogItemText(d, field, 0, 32767);
    } else if (itemHit == 18 || itemHit == 19) { // Format radio buttons
      for (int i = 0; i < 2; i++) {
        GetDialogItem(d, 18 + i, &itemType, &itemH, &itemR);
        SetControlValue((ControlHandle)itemH, (18 + i == itemHit));
      }
    } else if (itemHit == 23 || itemHit == 24) { // Date order radio buttons
      for (int i = 0; i < 2; i++) {
        GetDialogItem(d, 23 + i, &itemType, &itemH, &itemR);
        SetControlValue((ControlHandle)itemH, (23 + i == itemHit));
      }
    }
  }

  if (itemHit == 1) { // OK
    GetDialogItem(d, 4, &itemType, &itemH, &itemR);
    GetDialogItemText(itemH, macStr);
    CopyPascalToCString(macStr, buf);
    gAlarmHour = atoi(buf);

    GetDialogItem(d, 8, &itemType, &itemH, &itemR);
    GetDialogItemText(itemH, macStr);
    CopyPascalToCString(macStr, buf);
    gAlarmMinute = atoi(buf);

    GetDialogItem(d, 12, &itemType, &itemH, &itemR);
    gAlarmEnabled = GetControlValue((ControlHandle)itemH);

    GetDialogItem(d, 18, &itemType, &itemH, &itemR);
    gIs24Hour = !GetControlValue((ControlHandle)itemH);

    GetDialogItem(d, 21, &itemType, &itemH, &itemR);
    gInverted = GetControlValue((ControlHandle)itemH);

    GetDialogItem(d, 24, &itemType, &itemH, &itemR);
    gDateFormatUS = GetControlValue((ControlHandle)itemH);

    GetDialogItem(d, 26, &itemType, &itemH, &itemR);
    gGrayBackground = GetControlValue((ControlHandle)itemH);

    UpdateLayout(gWindow->portRect);
    RedrawAll();
    SavePrefs();
  }

  DisposeDialog(d);
  ShowCursor();
  gCursorHidden = false;
  gLastMouseMoveTime = TickCount();
  HideMenu();
}

void DoAbout() {
  if (gCursorHidden) {
    ShowCursor();
    gCursorHidden = false;
  }
  DialogPtr d = GetNewDialog(129, NULL, (WindowPtr)-1);
  short itemHit = 0;

  while (itemHit != 1) {
    ModalDialog(NULL, &itemHit);
  }

  DisposeDialog(d);
  ShowCursor();
  gCursorHidden = false;
  gLastMouseMoveTime = TickCount();
}

void HandleMenu(long menuResult) {
  short menuID = HiWord(menuResult);
  short menuItem = LoWord(menuResult);

  if (menuID == 128) { // Apple Menu
    if (menuItem == 1) {
      DoAbout();
    } else if (menuItem == 3) {
      DoPreferences();
    }
  } else if (menuID == 129) { // File Menu
    if (menuItem == 1) {
      exit(0);
    }
  }
  HiliteMenu(0);
}

void ShowStartupScreen() {
  PicHandle logo = GetPicture(128);
  if (logo == NULL)
    return;

  Rect r = (**logo).picFrame;
  int w = r.right - r.left;
  int h = r.bottom - r.top;

  Rect destR;
  destR.left = (gWindow->portRect.right - w) / 2;
  destR.top = (gWindow->portRect.bottom - h) / 2;
  destR.right = destR.left + w;
  destR.bottom = destR.top + h;

  BeginDraw();
  EraseRect(&gWindow->portRect);
  DrawPicture(logo, &destR);
  EndDraw();
  BlitToScreen();

  unsigned long start = TickCount();
  while (TickCount() - start < 180) { // 3 seconds
    EventRecord ev;
    if (WaitNextEvent(mDownMask | keyDownMask, &ev, 1, NULL))
      break;
  }

  ReleaseResource((Handle)logo);
}

/**
 * Entry point. Initializes Macintosh managers, sets up menus,
 * creates the full-screen window, and runs the main event loop.
 */
int main(int argc, char **argv) {
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(NULL);

  LoadPrefs();

  // Setup Menus
  MenuHandle appleMenu = GetMenu(128);
  AppendResMenu(appleMenu, 'DRVR');
  InsertMenu(appleMenu, 0);
  InsertMenu(GetMenu(129), 0);
  DrawMenuBar();

  // Save original menu bar height and hide it
  gOrigMBarHeight = *(short *)0x0BAA;
  *(short *)0x0BAA = 0;
  DrawMenuBar();

  gMenuVisible = false;

  UpdateMenuBarBackground();

  Rect bounds = qd.screenBits.bounds;
  gWindow = NewWindow(NULL, &bounds, "\pBigMaclock", true, plainDBox,
                      (WindowPtr)-1, true, 0);
  SetPort(gWindow);
  InitCursor();
  ShowCursor();

  InitOffscreen(bounds);

  UpdateLayout(gWindow->portRect);

  MyCopyCStringToPascal("Simple Beep", gAlarmSoundName);

  unsigned long now;
  DateTimeRec date;
  Boolean done = false;
  EventRecord event;
  Point lastMouse;
  unsigned long lastSecond = 0;
  GetMouse(&lastMouse);
  gLastMouseMoveTime = TickCount();

  ShowStartupScreen();

  RedrawAll();

  while (!done) {
    if (WaitNextEvent(everyEvent, &event, 1, NULL)) {
      switch (event.what) {
      case mouseDown: {
        WindowPtr whichWindow;
        short part = FindWindow(event.where, &whichWindow);
        if (part == inMenuBar) {
          HandleMenu(MenuSelect(event.where));
          gLastMenuTime = TickCount(); // Update after selection
        } else {
          if (gAlarmTriggered) {
            gAlarmTriggered = false;
          }
          // Toggle sticky menu on click anywhere else
          if (gMenuVisible)
            HideMenu();
          else
            ShowMenu(true); // Sticky reveal
        }
        break;
      }
      case keyDown:
      case autoKey: {
        char key = event.message & charCodeMask;
        if (key == 'q' || key == 'Q') {
          done = true;
        }
        if (key == 'h' || key == 'H') {
          gShowingFace = !gShowingFace;
          RedrawAll();
        }
        if (key == 'i' || key == 'I') {
          gInverted = !gInverted;
          UpdateLayout(gWindow->portRect);
          RedrawAll();
          if (gMenuVisible) {
            HideMenu();
          } else {
            gMenuVisible = true;
            HideMenu();
          }
          SavePrefs();
        }
        if (key == 'g' || key == 'G') {
          gGrayBackground = !gGrayBackground;
          UpdateLayout(gWindow->portRect);
          RedrawAll();
          if (gMenuVisible) {
            HideMenu();
          } else {
            gMenuVisible = true;
            HideMenu();
          }
          SavePrefs();
        }
        break;
      }
      case updateEvt:
        if ((WindowPtr)event.message == gWindow) {
          BeginUpdate(gWindow);
          RedrawAll();
          EndUpdate(gWindow);
        }
        break;
      }
    }

    // Mouse Movement / Menu Bar handling
    Point currentMouse;
    GetMouse(&currentMouse);
    if (currentMouse.h != lastMouse.h || currentMouse.v != lastMouse.v) {
      lastMouse = currentMouse;
      gLastMouseMoveTime = TickCount();
      if (gCursorHidden) {
        ShowCursor();
        gCursorHidden = false;
      }
      if (currentMouse.v < 20) {
        ShowMenu(false);
      } else if (gMenuVisible && !gMenuSticky) {
        HideMenu();
      }
    }

    // Auto-hide cursor after 2 seconds when menu is hidden and mouse idle
    if (!gMenuVisible && !gCursorHidden &&
        (TickCount() - gLastMouseMoveTime > 120)) {
      HideCursor();
      gCursorHidden = true;
    }

    // Auto-hide menu bar after 4 seconds of idle (sticky or not)
    if (gMenuVisible && (TickCount() - gLastMenuTime > 240)) {
      HideMenu();
    }

    GetDateTime(&now);

    if (now != lastSecond) {
      lastSecond = now;

      unsigned long secsSinceMidnight = now % 86400;
      int rawHour = secsSinceMidnight / 3600;
      int displayHour = rawHour;
      Boolean isPM = (rawHour >= 12);

      if (!gIs24Hour) {
        displayHour = rawHour % 12;
        if (displayHour == 0)
          displayHour = 12;
      }

      if (!gIs24Hour && isPM != gIsPM) {
        gIsPM = isPM;
        RedrawAll();
      } else {
        gIsPM = isPM;
      }

      int minute = (secsSinceMidnight % 3600) / 60, blink = (now % 2 == 0);
      int h1 = displayHour / 10, h2 = displayHour % 10, m1 = minute / 10,
          m2 = minute % 10;

      // Alarm Check
      if (gAlarmEnabled && !gAlarmTriggered) {
        if (rawHour == gAlarmHour && minute == gAlarmMinute &&
            (now % 60) == 0) {
          gAlarmTriggered = true;
        }
      }

      if (gAlarmTriggered) {
        static unsigned long lastAlarmTick = 0;
        if (TickCount() - lastAlarmTick > 60) {
          lastAlarmTick = TickCount();
          Handle h = GetNamedResource('snd ', gAlarmSoundName);
          if (h) {
            LoadResource(h);
            SndPlay(NULL, h, false);
          } else {
            SysBeep(30);
          }
        }
      }

      if (gShowingFace) {
        continue;
      }

      BeginDraw();

      if (h1 != g_prev_h1) {
        if (!gIs24Hour && h1 == 0) {
          Rect r1 = {g_startY, g_h1_x, g_startY + gDigitPixelHeight,
                     g_h1_x + gDigitPixelWidth};
          EraseRect(&r1);
        } else {
          DrawDigit(h1, g_h1_x, g_startY);
        }
        g_prev_h1 = h1;
        if (!gIs24Hour) {
          DrawAMPM(gIsPM, g_startX,
                   g_startY +
                       (gDigitPixelHeight - (2 * gSmallPixelHeight + 8)) / 2);
        }
      }
      if (h2 != g_prev_h2) {
        DrawDigit(h2, g_h2_x, g_startY);
        g_prev_h2 = h2;
      }
      if (blink != g_prev_blink) {
        DrawColon(g_col_x, g_startY, blink);
        g_prev_blink = blink;
      }
      if (m1 != g_prev_m1) {
        DrawDigit(m1, g_m1_x, g_startY);
        g_prev_m1 = m1;
      }
      if (m2 != g_prev_m2) {
        DrawDigit(m2, g_m2_x, g_startY);
        g_prev_m2 = m2;
      }

      if (now % 60 == 0 || g_prev_day == -1) {
        SecondsToDate(now, &date);
        if (date.day != g_prev_day || date.month != g_prev_month ||
            date.dayOfWeek != g_prev_dow) {
          Rect bl = {bounds.bottom - gSmallPixelHeight - 20, 20,
                     bounds.bottom - 20, 150};
          EraseRect(&bl);
          DrawSmallString(days[date.dayOfWeek - 1], 20,
                          bounds.bottom - gSmallPixelHeight - 20);

          char dateBuf[16];
          FormatDate(&date, dateBuf);
          int br_width = GetDateStringWidth(dateBuf);
          Rect br = {bounds.bottom - gSmallPixelHeight - 20,
                     bounds.right - br_width - 20, bounds.bottom - 20,
                     bounds.right - 20};
          EraseRect(&br);
          DrawSmallString(dateBuf, bounds.right - br_width - 20,
                          bounds.bottom - gSmallPixelHeight - 20);

          g_prev_day = date.day;
          g_prev_month = date.month;
          g_prev_dow = date.dayOfWeek;
        }
      }

      EndDraw();
      BlitToScreen();
    }
  }

  DisposeOffscreen();
  DisposeWindow(gWindow);
  return 0;
}
