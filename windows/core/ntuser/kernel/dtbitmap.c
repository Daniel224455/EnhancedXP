/****************************** Module Header ******************************\
* Module Name: dtbitmap.c
*
* Copyright (c) 1985 - 1999, Microsoft Corporation
*
* Desktop Wallpaper Routines.
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Local Constants.
 */
#define MAXPAL         256
#define MAXSTATIC       20
#define TILE_XMINSIZE    2
#define TILE_YMINSIZE    4

/*
 * The version strings are stored in a contiguous-buffer.  Each string
 * is of size MAXVERSIONSTRING.
 */

// Size of each string buffer.
#define MAXVERSIONSTRING  128

// Offset into verbuffer of build-string.
#define OFFSET_BLDSTRING  0

// Offset into verbuffer of CSD string.
#define OFFSET_CSDSTRING  OFFSET_BLDSTRING + MAXVERSIONSTRING

// Max size of buffer (contains all 3 strings).
#define MAXVERSIONBUFFER  OFFSET_CSDSTRING + MAXVERSIONSTRING

WCHAR          wszSafeMode[MAX_PATH + 3 * MAXVERSIONSTRING];
WCHAR          SafeModeStr[64];
int            SafeModeStrLen;
WCHAR          wszProductName[MAXVERSIONSTRING];
WCHAR          wszProductBuild[2 * MAXVERSIONSTRING];


__inline PWND _GetShellWindow(
    PDESKTOP pdesk)
{
    if (pdesk == NULL) {
        return NULL;
    } else {
        return pdesk->pDeskInfo->spwndShell;
    }
}

/***************************************************************************\
* GetVersionInfo
*
* Outputs a string on the desktop indicating debug-version.
*
* History:
\***************************************************************************/
VOID
GetVersionInfo(
    BOOL Verbose)
{
    WCHAR          NameBuffer[MAXVERSIONBUFFER];
    WCHAR          Title1[MAXVERSIONSTRING];
    WCHAR          Title2[MAXVERSIONSTRING];
    WCHAR          wszPID[MAXVERSIONSTRING];
    WCHAR          wszProduct[MAXVERSIONSTRING];
    WCHAR          wszPBuild[MAXVERSIONSTRING];
    WCHAR          wszEvaluation[MAXVERSIONSTRING];
    UNICODE_STRING UserBuildString;
    UNICODE_STRING UserCSDString;
    NTSTATUS       Status;
    UINT           uProductStrId;

    /*
     * Temporary code name handling.  Used internally and turned off by ""
     * for release.  The matching string in strid.mc doesn't have a space
     * separator between code name and the rest of the title, so this
     * space must be included at end of this code name string.
     */
    WCHAR          wszCodeName[] = L"";

    RTL_QUERY_REGISTRY_TABLE BaseServerRegistryConfigurationTable[] = {

        {NULL,
         RTL_QUERY_REGISTRY_DIRECT,
         L"BuildLab",
         &UserBuildString,
         REG_NONE,
         NULL,
         0},

        {NULL,
         RTL_QUERY_REGISTRY_DIRECT,
         L"CSDVersion",
         &UserCSDString,
         REG_NONE,
         NULL,
         0},

        {NULL,
         0,
         NULL,
         NULL,
         REG_NONE,
         NULL,
         0}
    };

    UserBuildString.Buffer          = &NameBuffer[OFFSET_BLDSTRING];
    UserBuildString.Length          = 0;
    UserBuildString.MaximumLength   = MAXVERSIONSTRING * sizeof(WCHAR);

    UserCSDString.Buffer            = &NameBuffer[OFFSET_CSDSTRING];
    UserCSDString.Length            = 0;
    UserCSDString.MaximumLength     = MAXVERSIONSTRING * sizeof(WCHAR);

    Status = RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT,
                                    L"",
                                    BaseServerRegistryConfigurationTable,
                                    NULL,
                                    NULL);

    if (!NT_SUCCESS(Status)) {
        RIPMSG1(RIP_WARNING, "GetVersionInfo failed with status %x", Status);
        return;
    }

    ServerLoadString(hModuleWin, STR_DTBS_PRODUCTID, wszPID, ARRAY_SIZE(wszPID));
    ServerLoadString(hModuleWin, STR_DTBS_PRODUCTBUILD, wszPBuild, ARRAY_SIZE(wszPBuild));

    /*
     * Write out Debugging Version message.
     */

    /*
     * Bug 280256 - joejo
     * Create new desktop build information strings
     */
    if (USER_SHARED_DATA->SuiteMask & (1 << EmbeddedNT)) {
        uProductStrId = STR_DTBS_PRODUCTEMB;
    } else if (USER_SHARED_DATA->NtProductType == NtProductWinNt) {
#ifdef _WIN64
        uProductStrId = STR_DTBS_PRODUCTWKS64;
#else
        if (USER_SHARED_DATA->SuiteMask & (1 << Personal)) {
            uProductStrId = STR_DTBS_PRODUCTPER;
        } else {
            uProductStrId = STR_DTBS_PRODUCTPRO;
        }
#endif
    } else {
#ifdef _WIN64
        if (USER_SHARED_DATA->SuiteMask & (1 << DataCenter)) {
            uProductStrId = STR_DTBS_PRODUCTDTC64;
        } else if (USER_SHARED_DATA->SuiteMask & (1 << Enterprise)) {
            uProductStrId = STR_DTBS_PRODUCTADV64;
        } else {
            uProductStrId = STR_DTBS_PRODUCTSRV64;
        }
#else
        if (USER_SHARED_DATA->SuiteMask & (1 << DataCenter)) {
            uProductStrId = STR_DTBS_PRODUCTDTC;
        } else if (USER_SHARED_DATA->SuiteMask & (1 << Enterprise)) {
            uProductStrId = STR_DTBS_PRODUCTADV;
        } else if (USER_SHARED_DATA->SuiteMask & (1 << Blade)) {
            uProductStrId = STR_DTBS_PRODUCTBLA;
        } else if(USER_SHARED_DATA->SuiteMask & (1 << SmallBusinessRestricted)) {
            uProductStrId = STR_DTBS_PRODUCTSBS;
        } else {
            uProductStrId = STR_DTBS_PRODUCTSRV;
        }
#endif /* _WIN64 */
    }

    ServerLoadString(hModuleWin, uProductStrId, wszProduct, ARRAY_SIZE(wszProduct));

    swprintf(
        wszProductName,
        wszPID,
        wszCodeName,
        wszProduct);

    if (gfUnsignedDrivers) {
        /* This takes precedence */
        ServerLoadString(hModuleWin, STR_TESTINGONLY, wszEvaluation, ARRAY_SIZE(wszEvaluation));
    } else if (USER_SHARED_DATA->SystemExpirationDate.QuadPart) {
        ServerLoadString(hModuleWin, STR_DTBS_EVALUATION, wszEvaluation, ARRAY_SIZE(wszEvaluation));
    } else {
        wszEvaluation[0] = '\0';
    }

    swprintf(
        wszProductBuild,
        wszPBuild,
        wszEvaluation,
        UserBuildString.Buffer
        );

    if (Verbose) {
        ServerLoadString(hModuleWin, STR_SAFEMODE_TITLE1, Title1, ARRAY_SIZE(Title1));
        ServerLoadString(hModuleWin, STR_SAFEMODE_TITLE2, Title2, ARRAY_SIZE(Title2));

        swprintf(
            wszSafeMode,
            UserCSDString.Length == 0 ? Title1 : Title2,
            wszCodeName,
            UserBuildString.Buffer,
            UserCSDString.Buffer,
            USER_SHARED_DATA->NtSystemRoot
            );
    } else {
        ServerLoadString(hModuleWin, STR_SAFEMODE_TITLE3, Title1, ARRAY_SIZE(Title1));
        ServerLoadString(hModuleWin, STR_SAFEMODE_TITLE4, Title2, ARRAY_SIZE(Title2));

        swprintf(
            wszSafeMode,
            UserCSDString.Length == 0 ? Title1 : Title2,
            wszCodeName,
            UserBuildString.Buffer,
            UserCSDString.Buffer);
    }
}

/***************************************************************************\
* GetDefaultWallpaperName
*
* Get initial bitmap name
*
* History:
* 21-Feb-1995 JimA      Created.
* 06-Mar-1996 ChrisWil  Moved to kernel to facilite ChangeDisplaySettings.
\***************************************************************************/
VOID
GetDefaultWallpaperName(
    LPWSTR  lpszWallpaper)
{
    /*
     * Set the initial global wallpaper bitmap name for (Default)
     * The global name is an at most 8 character name with no
     * extension.  It is "winnt" for workstation or "lanmannt"
     * for server or server upgrade.  It is followed by 256 it
     * is for 256 color devices.
     */
    if (USER_SHARED_DATA->NtProductType == NtProductWinNt) {
        wcsncpycch(lpszWallpaper, L"winnt", 8);
    } else {
        wcsncpycch(lpszWallpaper, L"lanmannt", 8);
    }

    lpszWallpaper[8] = (WCHAR)0;

    if (gpsi->BitsPixel * gpsi->Planes > 4) {
        int iStart = wcslen(lpszWallpaper);
        iStart = min(iStart, 5);

        lpszWallpaper[iStart] = (WCHAR)0;
        wcscat(lpszWallpaper, L"256");
    }
}

/***************************************************************************\
* GetDeskWallpaperName
*
* History:
* 19-Dec-1994 JimA          Created.
* 29-Sep-1995 ChrisWil      ReWrote to return filename.
\***************************************************************************/
#define GDWPN_KEYSIZE   40
#define GDWPN_BITSIZE  256

LPWSTR GetDeskWallpaperName(PUNICODE_STRING pProfileUserName,
        LPWSTR       lpszFile
        )
{
    WCHAR  wszKey[GDWPN_KEYSIZE];
    WCHAR  wszNone[GDWPN_KEYSIZE];
    LPWSTR lpszBitmap = NULL;

    /*
     * Load the none-string.  This will be used for comparisons later.
     */
    ServerLoadString(hModuleWin, STR_NONE, wszNone, ARRAY_SIZE(wszNone));

    if ((lpszFile == NULL)                 ||
        (lpszFile == SETWALLPAPER_DEFAULT) ||
        (lpszFile == SETWALLPAPER_METRICS)) {

        /*
         * Allocate a buffer for the wallpaper.  We will assume
         * a default-size in this case.
         */
        lpszBitmap = UserAllocPool(GDWPN_BITSIZE * sizeof(WCHAR), TAG_SYSTEM);
        if (lpszBitmap == NULL)
            return NULL;

        /*
         * Get the "Wallpaper" string from WIN.INI's [Desktop] section.  The
         * section name is not localized, so hard code it.  If the string
         * returned is Empty, then set it up for a none-wallpaper.
         *
         * Unlike the rest of per user settings that got updated in
         * xxxUpdatePerUserSystemParameters, wallpaper is being updated via a
         * direct call to SystemParametersInfo from UpdatePerUserSystemParameters.
         * Force remote settings check in this case.
         */
        if (!FastGetProfileStringFromIDW(pProfileUserName,
                                         PMAP_DESKTOP,
                                         STR_DTBITMAP,
                                         wszNone,
                                         lpszBitmap,
                                         GDWPN_BITSIZE,
                                         POLICY_REMOTE
                                         )) {
            wcscpy(lpszBitmap, wszNone);
        }

    } else {

        UINT uLen;

        uLen = wcslen(lpszFile) + 1;
        uLen = max(uLen, GDWPN_BITSIZE);

        /*
         * Allocate enough space to store the name passed in.  Returning
         * NULL will allow the wallpaper to redraw.  As well, if we're
         * out of memory, then no need to load a wallpaper anyway.
         */
        lpszBitmap = UserAllocPool(uLen * sizeof(WCHAR), TAG_SYSTEM);
        if (lpszBitmap == NULL)
            return NULL;

        wcscpy(lpszBitmap, lpszFile);
    }

    /*
     * No bitmap if NULL passed in or if (NONE) in win.ini entry.  We
     * return NULL to force the redraw of the wallpaper in the kernel.
     */
    if ((*lpszBitmap == (WCHAR)0) || (_wcsicmp(lpszBitmap, wszNone) == 0)) {
        UserFreePool(lpszBitmap);
        return NULL;
    }

    /*
     * If bitmap name set to (DEFAULT) then set it to the system bitmap.
     */
    ServerLoadString(hModuleWin, STR_DEFAULT, wszKey, ARRAY_SIZE(wszKey));

    if (_wcsicmp(lpszBitmap, wszKey) == 0) {
        GetDefaultWallpaperName(lpszBitmap);
    }


    return lpszBitmap;
}

/***************************************************************************\
* TestVGAColors
*
* Tests whether the log-palette is just a standard 20 palette.
*
* History:
* 29-Sep-1995 ChrisWil  Created.
\***************************************************************************/

BOOL TestVGAColors(
    LPLOGPALETTE ppal)
{
    int      i;
    int      n;
    int      size;
    COLORREF clr;

    static CONST DWORD StupidColors[] = {
         0x00000000,        //   0 Sys Black
         0x00000080,        //   1 Sys Dk Red
         0x00008000,        //   2 Sys Dk Green
         0x00008080,        //   3 Sys Dk Yellow
         0x00800000,        //   4 Sys Dk Blue
         0x00800080,        //   5 Sys Dk Violet
         0x00808000,        //   6 Sys Dk Cyan
         0x00c0c0c0,        //   7 Sys Lt Grey
         0x00808080,        // 248 Sys Lt Gray
         0x000000ff,        // 249 Sys Red
         0x0000ff00,        // 250 Sys Green
         0x0000ffff,        // 251 Sys Yellow
         0x00ff0000,        // 252 Sys Blue
         0x00ff00ff,        // 253 Sys Violet
         0x00ffff00,        // 254 Sys Cyan
         0x00ffffff,        // 255 Sys White

         0x000000BF,        //   1 Sys Dk Red again
         0x0000BF00,        //   2 Sys Dk Green again
         0x0000BFBF,        //   3 Sys Dk Yellow again
         0x00BF0000,        //   4 Sys Dk Blue again
         0x00BF00BF,        //   5 Sys Dk Violet again
         0x00BFBF00,        //   6 Sys Dk Cyan  again

         0x000000C0,        //   1 Sys Dk Red again
         0x0000C000,        //   2 Sys Dk Green again
         0x0000C0C0,        //   3 Sys Dk Yellow again
         0x00C00000,        //   4 Sys Dk Blue again
         0x00C000C0,        //   5 Sys Dk Violet again
         0x00C0C000,        //   6 Sys Dk Cyan  again
    };

    size = (sizeof(StupidColors) / sizeof(StupidColors[0]));

    for (i = 0; i < (int)ppal->palNumEntries; i++) {

        clr = ((LPDWORD)ppal->palPalEntry)[i];

        for (n = 0; n < size; n++) {

            if (StupidColors[n] == clr)
                break;
        }

        if (n == size)
            return FALSE;
    }

    return TRUE;
}

/***************************************************************************\
* DoHTColorAdjustment
*
* The default HT-Gamma adjustment was 2.0 on 3.5 (internal to gdi).  For
* 3.51 this value was decreased to 1.0 to accomdate printing.  For our
* desktop-wallpaper we are going to darken it slightly to that the image
* doesn't appear to light.  For the Shell-Release we will provid a UI to
* allow users to change this for themselves.
*
*
* History:
* 11-May-1995 ChrisWil  Created.
\***************************************************************************/

#define FIXED_GAMMA (WORD)13000

VOID DoHTColorAdjust(
    HDC hdc)
{
    COLORADJUSTMENT ca;

    if (GreGetColorAdjustment(hdc, &ca)) {

        ca.caRedGamma   =
        ca.caGreenGamma =
        ca.caBlueGamma  = FIXED_GAMMA;

        GreSetColorAdjustment(hdc, &ca);
    }
}

/***************************************************************************\
* ConvertToDDB
*
* Converts a DIBSection to a DDB.  We do this to speed up drawings so that
* bitmap-colors don't have to go through a palette-translation match.  This
* will also stretch/expand the image if the syle is set.
*
* If the new image requires a halftone-palette, the we will create one and
* set it as the new wallpaper-palette.
*
* History:
* 26-Oct-1995 ChrisWil  Ported.
* 30-Oct-1995 ChrisWil  Added halftoning.  Rewote the stretch/expand stuff.
\***************************************************************************/

HBITMAP ConvertToDDB(
    HDC      hdc,
    HBITMAP  hbmOld,
    HPALETTE hpal)
{
    BITMAP  bm;
    HBITMAP hbmNew;

    /*
     * This object must be a REALDIB type bitmap.
     */
    GreExtGetObjectW(hbmOld, sizeof(bm), &bm);

    /*
     * Create the new wallpaper-surface.
     */
    if (hbmNew = GreCreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight)) {

        HPALETTE hpalDst;
        HPALETTE hpalSrc;
        HBITMAP  hbmDst;
        HBITMAP  hbmSrc;
        UINT     bpp;
        BOOL     fHalftone = FALSE;

        /*
         * Select in the surfaces.
         */
        hbmDst = GreSelectBitmap(ghdcMem2, hbmNew);
        hbmSrc = GreSelectBitmap(ghdcMem, hbmOld);

        /*
         * Determine image bits/pixel.
         */
        bpp = (bm.bmPlanes * bm.bmBitsPixel);

        /*
         * Use the palette if given.  If the image is of a greater
         * resolution than the device, then we're going to go through
         * a halftone-palette to get better colors.
         */
        if (hpal) {

            hpalDst = _SelectPalette(ghdcMem2, hpal, FALSE);
            hpalSrc = _SelectPalette(ghdcMem, hpal, FALSE);

            xxxRealizePalette(ghdcMem2);

            /*
             * Set the halftoning for the destination.  This is done
             * for images of greater resolution than the device.
             */
            if (bpp > gpsi->BitCount) {
                fHalftone = TRUE;
                DoHTColorAdjust(ghdcMem2);
            }
        }

        /*
         * Set the stretchbltmode.  This is more necessary when doing
         * halftoning.  Since otherwise, the colors won't translate
         * correctly.
         */
        SetBestStretchMode(ghdcMem2, bpp, fHalftone);

        /*
         * Set the new surface bits.  Use StretchBlt() so the SBMode
         * will be used in color-translation.
         */
        GreStretchBlt(ghdcMem2,
                      0,
                      0,
                      bm.bmWidth,
                      bm.bmHeight,
                      ghdcMem,
                      0,
                      0,
                      bm.bmWidth,
                      bm.bmHeight,
                      SRCCOPY,
                      0);

        /*
         * Restore palettes.
         */
        if (hpal) {
            _SelectPalette(ghdcMem2, hpalDst, FALSE);
            _SelectPalette(ghdcMem, hpalSrc, FALSE);
        }

        /*
         * Restore the surfaces.
         */
        GreSelectBitmap(ghdcMem2, hbmDst);
        GreSelectBitmap(ghdcMem, hbmSrc);
        GreDeleteObject(hbmOld);

        GreSetBitmapOwner(hbmNew, OBJECT_OWNER_PUBLIC);

    } else {
        hbmNew = hbmOld;
    }

    return hbmNew;
}

/***************************************************************************\
* CreatePaletteFromBitmap
*
* Take in a REAL dib handle and create a palette from it.  This will not
* work for bitmaps created by any other means than CreateDIBSection or
* CreateDIBitmap(CBM_CREATEDIB).  This is due to the fact that these are
* the only two formats who have palettes stored with their object.
*
* History:
* 29-Sep-1995 ChrisWil  Created.
\***************************************************************************/

HPALETTE CreatePaletteFromBitmap(
    HBITMAP hbm)
{
    HPALETTE     hpal;
    LPLOGPALETTE ppal;
    HBITMAP      hbmT;
    DWORD        size;
    int          i;

    /*
     * Make room for temp logical palette of max size.
     */
    size = sizeof(LOGPALETTE) + (MAXPAL * sizeof(PALETTEENTRY));

    ppal = (LPLOGPALETTE)UserAllocPool(size, TAG_SYSTEM);
    if (!ppal)
        return NULL;

    /*
     * Retrieve the palette from the DIB(Section).  The method of calling
     * GreGetDIBColorTable() can only be done on sections or REAL-Dibs.
     */
    hbmT = GreSelectBitmap(ghdcMem, hbm);
    ppal->palVersion    = 0x300;
    ppal->palNumEntries = (WORD)GreGetDIBColorTable(ghdcMem,
                                              0,
                                              MAXPAL,
                                              (LPRGBQUAD)ppal->palPalEntry);
    GreSelectBitmap(ghdcMem, hbmT);

    /*
     * Create a halftone-palette if their are no entries.  Otherwise,
     * swap the RGB values to be palentry-compatible and create us a
     * palette.
     */
    if (ppal->palNumEntries == 0) {
        hpal = GreCreateHalftonePalette(gpDispInfo->hdcScreen);
    } else {

        BYTE tmpR;

        /*
         * Swap red/blue because a RGBQUAD and PALETTEENTRY dont get along.
         */
        for (i=0; i < (int)ppal->palNumEntries; i++) {
            tmpR                         = ppal->palPalEntry[i].peRed;
            ppal->palPalEntry[i].peRed   = ppal->palPalEntry[i].peBlue;
            ppal->palPalEntry[i].peBlue  = tmpR;
            ppal->palPalEntry[i].peFlags = 0;
        }

        /*
         * If the Bitmap only has VGA colors in it we dont want to
         * use a palette.  It just causes unessesary palette flashes.
         */
        hpal = TestVGAColors(ppal) ? NULL : GreCreatePalette(ppal);
    }

    UserFreePool(ppal);

    /*
     * Make this palette public.
     */
    if (hpal)
        GreSetPaletteOwner(hpal, OBJECT_OWNER_PUBLIC);

    return hpal;
}

/***************************************************************************\
* TileWallpaper
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/

BOOL
TileWallpaper(HDC hdc, LPCRECT lprc, BOOL fOffset)
{
    int     xO;
    int     yO;
    int     x;
    int     y;
    BITMAP  bm;
    HBITMAP hbmT = NULL;
    POINT   ptOffset;

    if (fOffset) {
        ptOffset.x = gsrcWallpaper.x;
        ptOffset.y = gsrcWallpaper.y;
    } else {
        ptOffset.x = 0;
        ptOffset.y = 0;
    }

    /*
     * We need to get the dimensions of the bitmap here rather than rely on
     * the dimensions in srcWallpaper because this function may
     * be called as part of ExpandBitmap, before srcWallpaper is
     * set.
     */
    if (GreExtGetObjectW(ghbmWallpaper, sizeof(BITMAP), (PBITMAP)&bm)) {
        xO = lprc->left - (lprc->left % bm.bmWidth) + (ptOffset.x % bm.bmWidth);
        if (xO > lprc->left) {
            xO -= bm.bmWidth;
        }

        yO = lprc->top - (lprc->top % bm.bmHeight) + (ptOffset.y % bm.bmHeight);
        if (yO > lprc->top) {
            yO -= bm.bmHeight;
        }

        /*
         *  Tile the bitmap to the surface.
         */
        if (hbmT = GreSelectBitmap(ghdcMem, ghbmWallpaper)) {
            for (y = yO; y < lprc->bottom; y += bm.bmHeight) {
                for (x = xO; x < lprc->right; x += bm.bmWidth) {
                    GreBitBlt(hdc,
                              x,
                              y,
                              bm.bmWidth,
                              bm.bmHeight,
                              ghdcMem,
                              0,
                              0,
                              SRCCOPY,
                              0);
                }
            }

            GreSelectBitmap(ghdcMem, hbmT);
        }
    }

    return (hbmT != NULL);
}

/***************************************************************************\
* GetWallpaperCenterRect
*
* Returns the rect of centered wallpaper on a particular monitor.
*
* History:
* 26-Sep-1996 adams     Created.
\***************************************************************************/

BOOL
GetWallpaperCenterRect(LPRECT lprc, LPPOINT lppt, LPCRECT lprcMonitor)
{
    RECT rc;


    if (gsrcWallpaper.x != 0 || gsrcWallpaper.y != 0) {
        rc.left = lprcMonitor->left + gsrcWallpaper.x;
        rc.top = lprcMonitor->top + gsrcWallpaper.y;
    } else {
        rc.left = (lprcMonitor->left + lprcMonitor->right - gsrcWallpaper.cx) / 2;
        rc.top = (lprcMonitor->top + lprcMonitor->bottom - gsrcWallpaper.cy) / 2;
    }

    rc.right  = rc.left + gsrcWallpaper.cx;
    rc.bottom = rc.top + gsrcWallpaper.cy;

    lppt->x = max(0, lprcMonitor->left - rc.left);
    lppt->y = max(0, lprcMonitor->top - rc.top);

    return IntersectRect(lprc, &rc, lprcMonitor);
}



/***************************************************************************\
* CenterOrStretchWallpaper
*
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/

BOOL
CenterOrStretchWallpaper(HDC hdc, LPCRECT lprcMonitor)
{
    BOOL    fStretchToEachMonitor = FALSE;
    RECT    rc;
    HBITMAP hbmT;
    BOOL    f = FALSE;
    HRGN    hrgn;
    POINT   pt;
    BITMAP  bm;
    int     oldStretchMode;

    /*
     * This used to call TileWallpaper, but this really slowed up the system
     * for small dimension bitmaps. We really only need to blt it once for
     * centered bitmaps.
     */
    if (hbmT = GreSelectBitmap(ghdcMem, ghbmWallpaper)) {
        if (fStretchToEachMonitor && (gwWPStyle & DTF_STRETCH)) {
            if (GreExtGetObjectW(ghbmWallpaper, sizeof(BITMAP), (PBITMAP)&bm)) {
                oldStretchMode = GreSetStretchBltMode(hdc, COLORONCOLOR);
                f = GreStretchBlt(hdc,
                                  lprcMonitor->left,
                                  lprcMonitor->top,
                                  lprcMonitor->right - lprcMonitor->left,
                                  lprcMonitor->bottom - lprcMonitor->top,
                                  ghdcMem,
                                  0,
                                  0,
                                  bm.bmWidth,
                                  bm.bmHeight,
                                  SRCCOPY,
                                  0);
                GreSetStretchBltMode(hdc, oldStretchMode);
            }
        } else {
            if (GetWallpaperCenterRect(&rc, &pt, lprcMonitor)) {
                f = GreBitBlt(hdc,
                              rc.left,
                              rc.top,
                              rc.right - rc.left,
                              rc.bottom - rc.top,
                              ghdcMem,
                              pt.x,
                              pt.y,
                              SRCCOPY,
                              0);

                /*
                 * Fill the bacground (excluding the bitmap) with the desktop
                 * brush.  Save the DC with the cliprect.
                 */
                if (f && NULL != (hrgn = CreateEmptyRgn())) {
                    if (GreGetRandomRgn(hdc, hrgn, 1) != -1) {
                        GreExcludeClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
                        FillRect(hdc, lprcMonitor, SYSHBR(DESKTOP));
                        GreExtSelectClipRgn(hdc, hrgn, RGN_COPY);
                    }

                    GreDeleteObject(hrgn);
                }
            }
        }

        GreSelectBitmap(ghdcMem, hbmT);
    }

    /*
     *  As a last-ditch effort, if something failed, just clear the desktop.
     */
    if (!f) {
        FillRect(hdc, lprcMonitor, SYSHBR(DESKTOP));
    }

    return f;
}

/***************************************************************************\
* xxxDrawWallpaper
*
* Performs the drawing of the wallpaper.  This can be either tiled or
* centered.  This routine provides the common things like palette-handling.
* If the (fPaint) is false, then we only to palette realization and no
* drawing.
*
* History:
* 01-Oct-1995 ChrisWil  Ported.
\***************************************************************************/

BOOL xxxDrawWallpaper(
    PWND        pwnd,
    HDC         hdc,
    PMONITOR    pMonitorPaint,
    LPCRECT     lprc)
{
    BOOL        f;
    HPALETTE    hpalT;
    int         i;

    CheckLock(pwnd);
    CheckLock(pMonitorPaint);
    UserAssert(ghbmWallpaper != NULL);
    UserAssert(lprc);

    /*
     * Select in the palette if one exists.  As a wallpaper, we should only
     * be able to do background-realizations.
     */
    if (ghpalWallpaper && pMonitorPaint->dwMONFlags & MONF_PALETTEDISPLAY) {
        hpalT = _SelectPalette(hdc, ghpalWallpaper, FALSE);
        i = xxxRealizePalette(hdc);
    } else {
        hpalT = NULL;
    }

    if (gwWPStyle & DTF_TILE) {
        f = TileWallpaper(hdc, lprc, pwnd != NULL);
    } else {
        f = CenterOrStretchWallpaper(hdc, &pMonitorPaint->rcMonitor);
    }

    if (hpalT) {
        _SelectPalette(hdc, hpalT, FALSE);
    }

    return f;
}

/***************************************************************************\
* xxxExpandBitmap
*
* Expand this bitmap to fit the screen.  This is used for tiled images
* only.
*
* History:
* 29-Sep-1995 ChrisWil  Ported from Chicago.
\***************************************************************************/

HBITMAP xxxExpandBitmap(
    HBITMAP hbm)
{
    int         nx;
    int         ny;
    BITMAP      bm;
    HBITMAP     hbmNew;
    HBITMAP     hbmD;
    LPRECT      lprc;
    RECT        rc;
    PMONITOR    pMonitor;
    TL          tlpMonitor;


    /*
     * Get the dimensions of the screen and bitmap we'll be dealing with.
     * We'll adjust the xScreen/yScreen to reflect the new surface size.
     * The default adjustment is to stretch the image to fit the screen.
     */
    GreExtGetObjectW(hbm, sizeof(bm), (PBITMAP)&bm);

    pMonitor = GetPrimaryMonitor();
    lprc = &pMonitor->rcMonitor;
    nx = (lprc->right / TILE_XMINSIZE) / bm.bmWidth;
    ny = (lprc->bottom / TILE_YMINSIZE) / bm.bmHeight;

    if (nx == 0)
        nx++;

    if (ny == 0)
        ny++;

    if ((nx + ny) <= 2)
        return hbm;


    /*
     * Create the surface for the new-bitmap.
     */
    rc.left = rc.top = 0;
    rc.right = nx * bm.bmWidth;
    rc.bottom = ny * bm.bmHeight;
    hbmD = GreSelectBitmap(ghdcMem, hbm);
    hbmNew = GreCreateCompatibleBitmap(ghdcMem, rc.right, rc.bottom);
    GreSelectBitmap(ghdcMem, hbmD);

    if (hbmNew == NULL)
        return hbm;

    if (hbmD = GreSelectBitmap(ghdcMem2, hbmNew)) {
        /*
         * Expand the bitmap to the new surface.
         */
        ThreadLockAlways(pMonitor, &tlpMonitor);
        xxxDrawWallpaper(NULL, ghdcMem2, pMonitor, &rc);
        ThreadUnlock(&tlpMonitor);
        GreSelectBitmap(ghdcMem2, hbmD);
    }

    GreDeleteObject(hbm);

    GreSetBitmapOwner(hbmNew, OBJECT_OWNER_PUBLIC);

    return hbmNew;
}

/***************************************************************************\
* xxxLoadDesktopWallpaper
*
* Load the dib (section) from the client-side.  We make this callback to
* utilize code in USER32 for loading/creating a dib or section.  Since,
* the wallpaper-code can be called from any-process, we can't use DIBSECTIONS
* for a wallpaper.  Luckily we can use Real-DIBs for this.  That way we
* can extract out a palette from the bitmap.  We couldn't do this if the
* bitmap was created "compatible".
*
* History:
* 29-Sep-1995 ChrisWil  Created.
\***************************************************************************/

BOOL xxxLoadDesktopWallpaper(
    LPWSTR lpszFile)
{
    UINT           LR_flags;
    int            dxDesired;
    int            dyDesired;
    UNICODE_STRING strName;


    /*
     * If the bitmap is somewhat large (big bpp), then we'll deal
     * with it as a real-dib.  We'll also do this for 8bpp since it
     * can utilize a palette.  Chicago uses DIBSECTIONS since it can
     * count on the one-process handling the drawing.  Since, NT can
     * have different processes doing the drawing, we can't use sections.
     */
    LR_flags = LR_LOADFROMFILE;

    if (gpDispInfo->fAnyPalette || gpsi->BitCount >= 8) {
        LR_flags |= LR_CREATEREALDIB;
    }

    /*
     * If we are going to be stretching the bitmap, go ahead and pre-stretch
     * the bitmap to the size of the primary monitor.  This makes blitting
     * to the primary monitor quicker (because it doesn't have to stretch),
     * while other monitors will be a little slower.
     */
    if (gwWPStyle & DTF_STRETCH) {
        PMONITOR pMonitor = GetPrimaryMonitor();
        dxDesired = pMonitor->rcMonitor.right - pMonitor->rcMonitor.left;
        dyDesired = pMonitor->rcMonitor.bottom - pMonitor->rcMonitor.top;
    } else {
        dxDesired = dyDesired = 0;
    }

    /*
     * Make a callback to the client to perform the loading.
     * Saves us some code.
     */
    RtlInitUnicodeString(&strName, lpszFile);

    ghbmWallpaper = xxxClientLoadImage(
            &strName,
            0,
            IMAGE_BITMAP,
            dxDesired,
            dyDesired,
            LR_flags,
            TRUE);

    if (ghbmWallpaper == NULL)
        return FALSE;

    /*
     * If it's a palette-display, then we will derive the global
     * wallpaper palette from the bitmap.
     */
    if (gpDispInfo->fAnyPalette) {
        ghpalWallpaper = CreatePaletteFromBitmap(ghbmWallpaper);
    }

    /*
     * Always try to convert the bitmap to a DDB.  On single monitor
     * systems this will improve performance.  On multiple-monitor
     * systems, GDI will refuse to create the DDB and just leave it
     * as a DIB at the color format of the primary monitor.
     */
    ghbmWallpaper = ConvertToDDB(gpDispInfo->hdcScreen, ghbmWallpaper, ghpalWallpaper);

    /*
     * Mark the bitmap as public, so any process can party with it.
     */
    GreSetBitmapOwner(ghbmWallpaper, OBJECT_OWNER_PUBLIC);

    /*
     * Expand bitmap if we are going to tile it. This creates a larger
     * bitmap that contains an even multiple of the source bitmap.  This
     * larger bitmap can then be tiled more quickly than the smaller bitmap.
     */
    if (gwWPStyle & DTF_TILE) {
        ghbmWallpaper = xxxExpandBitmap(ghbmWallpaper);
    }

    return TRUE;
}

/***************************************************************************\
* xxxSetDeskWallpaper
*
* Sets the desktop-wallpaper.  This deletes the old handles in the process.
*
* History:
* 29-Jul-1991 MikeKe    From win31.
* 01-Oct-1995 ChrisWil  Rewrote for LoadImage().
\***************************************************************************/

BOOL xxxSetDeskWallpaper(PUNICODE_STRING pProfileUserName,
    LPWSTR lpszFile)
{
    BITMAP       bm;
    UINT         WallpaperStyle2;
    PWND         pwndShell;
    TL           tl;
    PTHREADINFO  ptiCurrent = PtiCurrent();
    PDESKTOP     pdesk;
    BOOL         fRet = FALSE;
    HBITMAP      hbmOld;

    PROFINTINFO  apsi[] = {
        {PMAP_DESKTOP, (LPWSTR)STR_TILEWALL , 0, &gwWPStyle    },
        {PMAP_DESKTOP, (LPWSTR)STR_DTSTYLE  , 0, &WallpaperStyle2   },
        {PMAP_DESKTOP, (LPWSTR)STR_DTORIGINX, 0, &gsrcWallpaper.x },
        {PMAP_DESKTOP, (LPWSTR)STR_DTORIGINY, 0, &gsrcWallpaper.y },
        {0,            NULL,                  0, NULL               }
    };

    pdesk = ptiCurrent->rpdesk;
    hbmOld = ghbmWallpaper;

    if ((lpszFile == SETWALLPAPER_METRICS) && !(gwWPStyle & DTF_STRETCH)) {

        gsrcWallpaper.x = 0;
        gsrcWallpaper.y = 0;

        if (ghbmWallpaper)
            goto CreateNewWallpaper;

        goto Metric_Change;
    }

CreateNewWallpaper:

    /*
     * Delete the old wallpaper and palette if the exist.
     */
    if (ghpalWallpaper) {
        GreDeleteObject(ghpalWallpaper);
        ghpalWallpaper = NULL;
    }

    if (ghbmWallpaper) {
        GreDeleteObject(ghbmWallpaper);
        ghbmWallpaper = NULL;
    }

    /*
     * Kill any SPBs no matter what. This works if we're switching from/to
     * palettized wallpaper. Fixes a lot of problems because palette doesn't
     * change, shell paints funny on desktop, etc.
     */
    BEGINATOMICCHECK();
    FreeAllSpbs();
    ENDATOMICCHECK();

    /*
     * If this is a metric-change (and stretched), then we need to
     * reload it.  However, since we are called from the winlogon process
     * during a desktop-switch, we would be mapped to the wrong Luid
     * when we attempt to grab the name from GetDeskWallpaperName.  This
     * would use the Luid from the DEFAULT user rather than the current
     * logged on user.  In order to avoid this, we cache the wallpaer
     * name so that on METRIC-CHANGES we use the current-user's wallpaper.
     *
     * NOTE: we assume that prior to any METRIC change, we have already
     * setup the ghbmWallpaper and lpszCached.  This is usually done
     * either on logon or during user desktop-changes through conrol-Panel.
     */
    if (lpszFile == SETWALLPAPER_METRICS) {

        UserAssert(gpszWall != NULL);

        goto LoadWallpaper;
    }

    /*
     * Free the cached handle.
     */
    if (gpszWall) {
        UserFreePool(gpszWall);
        gpszWall = NULL;
    }

    /*
     * Load the wallpaper-name.  If this returns FALSE, then
     * the user specified (None).  We will return true to force
     * the repainting of the desktop.
     */
    gpszWall = GetDeskWallpaperName(pProfileUserName,lpszFile);
    if (!gpszWall) {
        fRet = TRUE;
        goto SDW_Exit;
    }

    /*
     * Retrieve the default settings from the registry.
     *
     * If tile is indicated, then normalize style to not include
     * FIT/STRETCH which are center-only styles.  Likewise, if
     * we are centered, then normalize out the TILE bit.
     */
    FastGetProfileIntsW(pProfileUserName, apsi, 0);

    gwWPStyle &= DTF_TILE;
    if (!(gwWPStyle & DTF_TILE)) {
        gwWPStyle = WallpaperStyle2 & DTF_STRETCH;
    }

    /*
     * Load the wallpaper.  This makes a callback to the client to
     * perform the bitmap-creation.
     */

LoadWallpaper:

    if (xxxLoadDesktopWallpaper(gpszWall) == FALSE) {
        gwWPStyle = 0;
        goto SDW_Exit;
    }

    /*
     * If we have a palette, then we need to do the correct realization and
     * notification.
     */
    if (ghpalWallpaper != NULL) {
        PWND pwndSend;

        /*
         * Get the shell window. This could be NULL on system
         * initialization. We will use this to do palette realization.
         */
        pwndShell = _GetShellWindow(pdesk);
        if (pwndShell) {
            pwndSend = pwndShell;
        } else {
            pwndSend = (pdesk ? pdesk->pDeskInfo->spwnd : NULL);
        }

        /*
         * Update the desktop with the new bitmap.  This cleans
         * out the system-palette so colors can be realized.
         */
        GreRealizeDefaultPalette(gpDispInfo->hdcScreen, TRUE);

        /*
         * Don't broadcast if system initialization is occuring. Otherwise
         * this gives the shell first crack at realizing its colors
         * correctly.
         */
        if (pwndSend) {
            HWND hwnd = HW(pwndSend);

            ThreadLockAlways(pwndSend, &tl);
            xxxSendNotifyMessage(pwndSend, WM_PALETTECHANGED, (WPARAM)hwnd, 0);
            ThreadUnlock(&tl);
        }
    }

Metric_Change:
    if (fRet = GreExtGetObjectW(ghbmWallpaper, sizeof(bm), (PBITMAP)&bm)) {
        gsrcWallpaper.cx = bm.bmWidth;
        gsrcWallpaper.cy = bm.bmHeight;
    }

SDW_Exit:

    /*
     * Notify the shell-window that the wallpaper changed. We need to refresh
     * our local pwndShell here because we might have called-back above.
     */
    pwndShell = _GetShellWindow(pdesk);
    if ((pwndShell != NULL) &&
        ((hbmOld && !ghbmWallpaper) || (!hbmOld && ghbmWallpaper))) {

        ThreadLockAlways(pwndShell, &tl);
        xxxSendNotifyMessage(pwndShell,
                             WM_SHELLNOTIFY,
                             SHELLNOTIFY_WALLPAPERCHANGED,
                             (LPARAM)ghbmWallpaper);
        ThreadUnlock(&tl);
    }

    return fRet;
}


/***************************************************************************\
* DesktopBuildPaint
*
* Draw the build information onto the desktop
*
* History:
* 2/4/99    joejo - Bug 280256
\***************************************************************************/
void DesktopBuildPaint(
    HDC         hdc,
    PMONITOR    pMonitor)
{
    int         imode;
    COLORREF    oldColor;
    RECT        rcText = {0,0,0,0};
    RECT        rcBuildInfo = {0,0,0,0};
    HFONT       oldFont = GreGetHFONT(hdc);
    SIZE sizeText;
    SIZE sizeProductName;
    SIZE sizeProductBuild;
    SIZE sizeSystemRoot;
    BOOL fDrawSolidBackground = FALSE;
    int cBorder = 5;
    int cMargin = fDrawSolidBackground ? 5 : 0;

    /*
     * Set up DC
     */
    imode = GreSetBkMode(hdc, TRANSPARENT);

    if (fDrawSolidBackground) {
        /*
         * Since we are going to draw our own background, we can always set
         * the pen color to black.
         */
        oldColor = GreSetTextColor( hdc, RGB(0,0,0) );
    } else {
        /*
         * Since we are not going to draw our own background, we have to work
         * with whatever is already there.  This is an ugly hack to try and
         * cover the case where our white text won't show up on a white
         * background.  Of course, this doesn't catch the bitmap case.  Or
         * the almost white case.
         */
        if (GreGetBrushColor(SYSHBR(BACKGROUND)) != 0x00ffffff) {
            oldColor = GreSetTextColor( hdc, RGB(255,255,255) );
        } else {
            oldColor = GreSetTextColor( hdc, RGB(0,0,0) );
        }
    }

    /*
     * Get the width in pixels of the longest string we are going to print out.
     */
    if (gpsi && gpsi->hCaptionFont) {
        GreSelectFont(hdc, gpsi->hCaptionFont);
    }

    GreGetTextExtentW(
        hdc,
        wszProductName,
        wcslen(wszProductName),
        &sizeProductName,
        GGTE_WIN3_EXTENT);

    if (ghMenuFont != NULL ) {
        GreSelectFont(hdc, ghMenuFont);
    }

    GreGetTextExtentW(
        hdc,
        wszProductBuild,
        wcslen(wszProductBuild),
        &sizeProductBuild,
        GGTE_WIN3_EXTENT);

    if (gDrawVersionAlways) {
        GreGetTextExtentW(
            hdc,
            USER_SHARED_DATA->NtSystemRoot,
            wcslen(USER_SHARED_DATA->NtSystemRoot),
            &sizeSystemRoot,
            GGTE_WIN3_EXTENT);
    } else {
        sizeSystemRoot.cx = 0;
        sizeSystemRoot.cy = 0;
    }

    sizeText.cx = sizeProductName.cx >= sizeProductBuild.cx ? sizeProductName.cx : sizeProductBuild.cx;
    sizeText.cy = sizeProductName.cy + sizeProductBuild.cy;
    if (gDrawVersionAlways) {
        sizeText.cx = (sizeText.cx >= sizeSystemRoot.cx) ? sizeText.cx : sizeSystemRoot.cx;
        sizeText.cy += sizeSystemRoot.cy;
    }

    /*
     * Calculate the position for all of the build info on the desktop.
     * We will draw either 2 or 3 lines of text.
     */
    rcBuildInfo.left = pMonitor->rcWork.right - cBorder - cMargin - sizeText.cx - cMargin;
    rcBuildInfo.top = pMonitor->rcWork.bottom - cBorder - cMargin - sizeText.cy - cMargin;
    rcBuildInfo.right = pMonitor->rcWork.right - cBorder;
    rcBuildInfo.bottom = pMonitor->rcWork.bottom - cBorder;

    /*
     * Draw the background if we want it.
     *
     */
    if (fDrawSolidBackground) {
        NtGdiRoundRect(hdc,  rcBuildInfo.left, rcBuildInfo.top, rcBuildInfo.right, rcBuildInfo.bottom, 10, 10);
    }

    /*
     * Print Windows 2000 name
     */
    if (gpsi && gpsi->hCaptionFont) {
        GreSelectFont(hdc, gpsi->hCaptionFont);
    }

    rcText.left = rcBuildInfo.left + cMargin;
    rcText.top = rcBuildInfo.top + cMargin;
    rcText.right = rcText.left + sizeText.cx;
    rcText.bottom = rcText.top + sizeProductName.cy;

    GreSetTextAlign(hdc, TA_RIGHT | TA_BOTTOM);

    GreExtTextOutW(
        hdc,
        rcText.right,
        rcText.bottom,
        0,
        &rcText,
        wszProductName,
        wcslen(wszProductName),
        (LPINT)NULL
        );

    /*
     * Print Build Number
     */
    if (ghMenuFont != NULL ) {
        GreSelectFont(hdc, ghMenuFont);
    }

    rcText.top = rcText.bottom + 1;
    rcText.bottom = rcText.top + sizeProductBuild.cy;

    GreExtTextOutW(
        hdc,
        rcText.right,
        rcText.bottom,
        0,
        &rcText,
        wszProductBuild,
        wcslen(wszProductBuild),
        (LPINT)NULL
        );

    /*
     * If we are in CHK mode, draw the System Dir Path
     */
    if (gDrawVersionAlways) {
        rcText.top = rcText.bottom + 1;
        rcText.bottom = rcText.top + sizeSystemRoot.cy;

        GreExtTextOutW(
            hdc,
            rcText.right,
            rcText.bottom,
            0,
            &rcText,
            USER_SHARED_DATA->NtSystemRoot,
            wcslen(USER_SHARED_DATA->NtSystemRoot),
            (LPINT)NULL
            );
    }

    if (oldFont) {
        GreSelectFont(hdc, oldFont);
    }

    GreSetBkMode(hdc, imode);
    GreSetTextColor(hdc, oldColor);
}

/***************************************************************************\
* xxxDesktopPaintCallback
*
* Draw the wallpaper or fill with the background brush. In debug,
* also draw the build number on the top of each monitor.
*
* History:
* 20-Sep-1996 adams     Created.
\***************************************************************************/
BOOL
xxxDesktopPaintCallback(
    PMONITOR        pMonitor,
    HDC             hdc,
    LPRECT          lprcMonitorClip,
    LPARAM          dwData)
{
    BOOL            f;
    PWND            pwnd;

    CheckLock(pMonitor);

    pwnd = (PWND)dwData;


    if (SYSMET(CLEANBOOT)) {
        FillRect(hdc, lprcMonitorClip, ghbrBlack );
        f = TRUE;
    } else {
        /*
         * if this is the disconnected desktop, skip the bitmap paint
         */
        if (gbDesktopLocked) {
            f = FALSE;
        } else {

            /*
             * Paint the desktop with a color or the wallpaper.
             */
            if (ghbmWallpaper) {
                f = xxxDrawWallpaper(
                        pwnd,
                        hdc,
                        pMonitor,
                        lprcMonitorClip);
            } else {
                FillRect(hdc, lprcMonitorClip, SYSHBR(DESKTOP));
                f = TRUE;
            }
        }
    }

    if (SYSMET(CLEANBOOT)
            || gDrawVersionAlways
            || gdwCanPaintDesktop) {
        static BOOL fInit = TRUE;
        SIZE        size;
        int         imode;
        COLORREF    oldColor;
        HFONT       oldFont = NULL;


        /*
         * Grab the stuff from the registry
         */
        if (fInit) {
            if (SYSMET(CLEANBOOT)) {
                ServerLoadString( hModuleWin, STR_SAFEMODE, SafeModeStr, ARRAY_SIZE(SafeModeStr) );
                SafeModeStrLen = wcslen(SafeModeStr);
            }
            GetVersionInfo(SYSMET(CLEANBOOT) == 0);
            fInit = FALSE;
        }

        if (SYSMET(CLEANBOOT)) {
            if (gpsi != NULL && gpsi->hCaptionFont != NULL) {
                oldFont = GreSelectFont(hdc, gpsi->hCaptionFont);
            }

            GreGetTextExtentW(hdc, wszSafeMode, wcslen(wszSafeMode), &size, GGTE_WIN3_EXTENT);
            imode = GreSetBkMode(hdc, TRANSPARENT);

            oldColor = GreSetTextColor( hdc, RGB(255,255,255) );

            GreExtTextOutW(
                hdc,
                (pMonitor->rcWork.left + pMonitor->rcWork.right - size.cx) / 2,
                pMonitor->rcWork.top,
                0,
                (LPRECT)NULL,
                wszSafeMode,
                wcslen(wszSafeMode),
                (LPINT)NULL
                );

            GreGetTextExtentW(hdc, SafeModeStr, SafeModeStrLen, &size, GGTE_WIN3_EXTENT);

            GreExtTextOutW(
                hdc,
                pMonitor->rcWork.left,
                pMonitor->rcWork.top,
                0,
                (LPRECT)NULL,
                SafeModeStr,
                SafeModeStrLen,
                (LPINT)NULL
                );

            GreExtTextOutW(
                hdc,
                pMonitor->rcWork.right - size.cx,
                pMonitor->rcWork.top,
                0,
                (LPRECT)NULL,
                SafeModeStr,
                SafeModeStrLen,
                (LPINT)NULL
                );

            GreExtTextOutW(
                hdc,
                pMonitor->rcWork.right - size.cx,
                pMonitor->rcWork.bottom - gpsi->tmSysFont.tmHeight,
                0,
                (LPRECT)NULL,
                SafeModeStr,
                SafeModeStrLen,
                (LPINT)NULL
                );

            GreExtTextOutW(
                hdc,
                pMonitor->rcWork.left,
                pMonitor->rcWork.bottom - gpsi->tmSysFont.tmHeight,
                0,
                (LPRECT)NULL,
                SafeModeStr,
                SafeModeStrLen,
                (LPINT)NULL
                );

            GreSetBkMode(hdc, imode);
            GreSetTextColor(hdc, oldColor);

            if (oldFont) {
                GreSelectFont(hdc, oldFont);
            }
        } else {
            if (!gbRemoteSession || !gdwTSExcludeDesktopVersion) {
                DesktopBuildPaint(hdc, pMonitor);
            }
        }
    }

    return f;
}



/***************************************************************************\
* xxxInvalidateDesktopOnPaletteChange
*
* Invalidates the shell window and uncovered areas of the desktop
* when the palette changes.
*
* History:
* 28-Apr-1997 adams     Created.
\***************************************************************************/
VOID
xxxInvalidateDesktopOnPaletteChange(
    PWND pwnd)
{
    PDESKTOP    pdesk;
    PWND        pwndShell;
    TL          tlpwndShell;
    RECT        rc;
    BOOL        fRedrawDesktop;

    CheckLock(pwnd);

    /*
     * Invalidate the shell window.
     */
    pdesk = PtiCurrent()->rpdesk;
    pwndShell = _GetShellWindow(pdesk);
    if (!pwndShell) {
        fRedrawDesktop = TRUE;
        rc = gpsi->rcScreen;
    } else {
        ThreadLockAlways(pwndShell, &tlpwndShell);
        xxxRedrawWindow(
                pwndShell,
                NULL,
                NULL,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

        /*
         * The shell window may not cover all of the desktop.
         * Invalidate the part of the desktop wallpaper it
         * doesn't sit over.
         */
        fRedrawDesktop = SubtractRect(&rc, &pwnd->rcWindow, &pwndShell->rcWindow);
        ThreadUnlock(&tlpwndShell);
    }

    /*
     * Invalidate the desktop window.
     */
    if (fRedrawDesktop) {
        xxxRedrawWindow(
                pwnd,
                &rc,
                NULL,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}

/***************************************************************************\
* xxxInternalPaintDesktop
*
* If fPaint is TRUE, enumerate the monitors to paint the desktop.
* Otherwise, it selects the bitmap palette into the DC to select
* its colors into the hardware palette.
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/
BOOL xxxInternalPaintDesktop(
    PWND    pwnd,
    HDC     hdc,
    BOOL    fPaint)
{
    BOOL fRet = FALSE;

    CheckLock(pwnd);

    if (fPaint) {
        RECT rcOrg, rcT;
        POINT pt;

        /*
         * For compatiblity purposes the DC origin of desktop windows
         * is set to the primary monitor, i.e. (0,0). Since we may get
         * either desktop or non-desktop DCs here, temporarily reset
         * the hdc origin to (0,0).
         */
        GreGetDCOrgEx(hdc, &pt, &rcOrg);
        CopyRect(&rcT, &rcOrg);
        OffsetRect(&rcT, -rcT.left, -rcT.top);
        GreSetDCOrg(hdc, rcT.left, rcT.top, (PRECTL)&rcT);

        fRet = xxxEnumDisplayMonitors(
                hdc,
                NULL,
                (MONITORENUMPROC) xxxDesktopPaintCallback,
                (LPARAM)pwnd,
                TRUE);

        /*
         * Reset the DC origin back.
         */
        GreSetDCOrg(hdc, rcOrg.left, rcOrg.top, (PRECTL)&rcOrg);

    } else if (ghpalWallpaper &&
               GetPrimaryMonitor()->dwMONFlags & MONF_PALETTEDISPLAY) {
        /*
         * Select in the palette if one exists.
         */
        HPALETTE    hpalT;
        int         i;

        hpalT = _SelectPalette(hdc, ghpalWallpaper, FALSE);
        i = xxxRealizePalette(hdc);
        _SelectPalette(hdc, hpalT, FALSE);

        if (i > 0) {
            xxxInvalidateDesktopOnPaletteChange(pwnd);
        }
        fRet = TRUE;
    }

    return fRet;
}
