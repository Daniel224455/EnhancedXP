/*++

 Copyright (c) 2000-2002 Microsoft Corporation

 Module Name:
    
    IgnoreException.cpp

 Abstract:

    This shim is for handling exceptions that get thrown by bad apps.
    The primary causes of unhandled exceptions are:

        1. Priviliged mode instructions: cli, sti, out etc
        2. Access violations

    In most cases, ignoring an Access Violation will be fatal for the app,
    but it works in some cases, eg: 
    
        Deer Hunter 2 - their 3d algorithm reads too far back in a lookup 
        buffer. This is a game bug and doesn't crash win9x because that memory
        is usually allocated.

    Interstate 76 also requires a Divide by Zero exception to be ignored.

 Notes:

    This is a general purpose shim.

 History:

    02/10/2000  linstev     Created
    10/17/2000  maonis      Bug fix - now it ignores AVs correctly.
    02/27/2001  robkenny    Converted to use CString
    02/15/2002  robkenny    Shim was copying data into a temp buffer without verifying
                            that the buffer was large enough.
                            Cleaned up some signed/unsigned comparison mismatch.

--*/

#include "precomp.h"

IMPLEMENT_SHIM_BEGIN(IgnoreException)
#include "ShimHookMacro.h"

APIHOOK_ENUM_BEGIN
APIHOOK_ENUM_END

// Exception code for OutputDebugString
#define DBG_EXCEPTION  0x40010000L

// Determine how to manage second chance exceptions
BOOL g_bWin2000 = FALSE;
DWORD g_dwLastEip = 0;

extern DWORD GetInstructionLengthFromAddress(LPBYTE pEip);

typedef enum 
{
    eActive = 0, 
    eFirstChance, 
    eSecondChance,
    eExitProcess
} EMODE;

WCHAR * ToWchar(EMODE emode)
{
    switch (emode)
    {
    case eActive:
        return L"Active";

    case eFirstChance:
        return L"FirstChance";

    case eSecondChance:
        return L"SecondChance";

    case eExitProcess:
        return L"ExitProcess";
    };

    return L"ERROR";
}

// Convert a text version of EMODE to a EMODE value
EMODE ToEmode(const CString & csMode)
{
    if (csMode.Compare(L"0") == 0 || csMode.Compare(ToWchar(eActive)) == 0)
    {
        return eActive;
    }
    else if (csMode.Compare(L"1") == 0 || csMode.Compare(ToWchar(eFirstChance)) == 0)
    {
        return eFirstChance;
    }
    else if (csMode.Compare(L"2") == 0 || csMode.Compare(ToWchar(eSecondChance)) == 0)
    {
        return eSecondChance;
    }
    else if (csMode.Compare(L"3") == 0 || csMode.Compare(ToWchar(eExitProcess)) == 0)
    {
        return eExitProcess;
    }

    // Default value
    return eFirstChance;
}


static const DWORD DONT_CARE = 0xFFFFFFFF;

/*++

 This is the list of all the exceptions that this shim can handle. The fields are

    1. cName      - the name of the exception as accepted as a parameter and 
                    displayed in debug spew
    2. dwCode     - the exception code
    3. dwSubCode  - parameters specified by the exception: -1 = don't care
    4. dwIgnore   - ignore this exception: 
                    0 = don't ignore
                    1 = ignore 1st chance
                    2 = ignore 2nd chance
                    3 = exit process on 2nd chance.

--*/

struct EXCEPT
{
    WCHAR * cName;
    DWORD dwCode;
    DWORD dwSubCode;
    EMODE dwIgnore;
};

static EXCEPT g_eList[] =
{
    {L"ACCESS_VIOLATION_READ"    , (DWORD)EXCEPTION_ACCESS_VIOLATION        , 0 ,          eActive},
    {L"ACCESS_VIOLATION_WRITE"   , (DWORD)EXCEPTION_ACCESS_VIOLATION        , 1 ,          eActive},
    {L"ARRAY_BOUNDS_EXCEEDED"    , (DWORD)EXCEPTION_ARRAY_BOUNDS_EXCEEDED   , DONT_CARE,   eActive},
    {L"BREAKPOINT"               , (DWORD)EXCEPTION_BREAKPOINT              , DONT_CARE,   eActive},
    {L"DATATYPE_MISALIGNMENT"    , (DWORD)EXCEPTION_DATATYPE_MISALIGNMENT   , DONT_CARE,   eActive},
    {L"FLT_DENORMAL_OPERAND"     , (DWORD)EXCEPTION_FLT_DENORMAL_OPERAND    , DONT_CARE,   eActive},
    {L"FLT_DIVIDE_BY_ZERO"       , (DWORD)EXCEPTION_FLT_DIVIDE_BY_ZERO      , DONT_CARE,   eActive},
    {L"FLT_INEXACT_RESULT"       , (DWORD)EXCEPTION_FLT_INEXACT_RESULT      , DONT_CARE,   eActive},
    {L"FLT_INVALID_OPERATION"    , (DWORD)EXCEPTION_FLT_INVALID_OPERATION   , DONT_CARE,   eActive},
    {L"FLT_OVERFLOW"             , (DWORD)EXCEPTION_FLT_OVERFLOW            , DONT_CARE,   eActive},
    {L"FLT_STACK_CHECK"          , (DWORD)EXCEPTION_FLT_STACK_CHECK         , DONT_CARE,   eActive},
    {L"FLT_UNDERFLOW"            , (DWORD)EXCEPTION_FLT_UNDERFLOW           , DONT_CARE,   eActive},
    {L"ILLEGAL_INSTRUCTION"      , (DWORD)EXCEPTION_ILLEGAL_INSTRUCTION     , DONT_CARE,   eActive},
    {L"IN_PAGE_ERROR"            , (DWORD)EXCEPTION_IN_PAGE_ERROR           , DONT_CARE,   eActive},
    {L"INT_DIVIDE_BY_ZERO"       , (DWORD)EXCEPTION_INT_DIVIDE_BY_ZERO      , DONT_CARE,   eActive},
    {L"INT_OVERFLOW"             , (DWORD)EXCEPTION_INT_OVERFLOW            , DONT_CARE,   eActive},
    {L"INVALID_DISPOSITION"      , (DWORD)EXCEPTION_INVALID_DISPOSITION     , DONT_CARE,   eActive},
    {L"NONCONTINUABLE_EXCEPTION" , (DWORD)EXCEPTION_NONCONTINUABLE_EXCEPTION, DONT_CARE,   eActive},
    {L"PRIV_INSTRUCTION"         , (DWORD)EXCEPTION_PRIV_INSTRUCTION        , DONT_CARE,   eFirstChance},
    {L"SINGLE_STEP"              , (DWORD)EXCEPTION_SINGLE_STEP             , DONT_CARE,   eActive},
    {L"STACK_OVERFLOW"           , (DWORD)EXCEPTION_STACK_OVERFLOW          , DONT_CARE,   eActive},
    {L"INVALID_HANDLE"           , (DWORD)EXCEPTION_INVALID_HANDLE          , DONT_CARE,   eActive}
};

#define ELISTSIZE sizeof(g_eList) / sizeof(g_eList[0])

/*++

 Custom exception handler.

--*/

LONG 
ExceptionFilter(
    struct _EXCEPTION_POINTERS *ExceptionInfo
    )
{
    DWORD dwCode = ExceptionInfo->ExceptionRecord->ExceptionCode;

    if ((dwCode & DBG_EXCEPTION) == DBG_EXCEPTION) // for the DebugPrints
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    CONTEXT *lpContext = ExceptionInfo->ContextRecord;
    const WCHAR * szException = L"Unknown";
    BOOL bIgnore = FALSE;

    //
    // Run the list of exceptions to see if we're ignoring it
    //

    for (int i = 0; i < ELISTSIZE; i++)
    {
        const EXCEPT *pE = g_eList + i;

        // Matched the major exception code
        if (dwCode == pE->dwCode)
        {
            // See if we care about the subcode
            if ((pE->dwSubCode != DONT_CARE) && 
                (ExceptionInfo->ExceptionRecord->ExceptionInformation[0] != pE->dwSubCode))
            {
                continue;
            }

            szException = pE->cName;
            
            // Determine how to handle the exception
            switch (pE->dwIgnore)
            {
            case eActive: 
                bIgnore = FALSE;
                break;
            
            case eFirstChance:
                bIgnore = TRUE;
                break;
            
            case eSecondChance:
				#if defined(_WIN64)
                bIgnore = g_bWin2000 || (g_dwLastEip == lpContext->Rip);
                g_dwLastEip = lpContext->Rip;
				#else
                bIgnore = g_bWin2000 || (g_dwLastEip == lpContext->Eip);
                g_dwLastEip = lpContext->Eip;
				#endif
                break;

            case eExitProcess:
                // Try using unhandled exception filters to catch this
                bIgnore = TRUE;//g_bWin2000 || IsBadCodePtr((FARPROC)lpContext->Eip);
                if (bIgnore)
                {
                    ExitProcess(0);
                }
				#if defined(_WIN64)
                g_dwLastEip = lpContext->Rip;
				#else
                g_dwLastEip = lpContext->Eip;
				#endif
                break;
            }
            
            if (bIgnore) break;
        }
    }
    
    //
    //  Dump out the exception
    //

    DPFN( eDbgLevelWarning, "Exception %S (%08lx)\n", 
        szException,
        dwCode);

    #ifdef DBG
        DPFN( eDbgLevelWarning, "eip=%08lx\n", 
            lpContext->Eip);

        DPFN( eDbgLevelWarning, "eax=%08lx, ebx=%08lx, ecx=%08lx, edx=%08lx\n", 
            lpContext->Eax, 
            lpContext->Ebx, 
            lpContext->Ecx, 
            lpContext->Edx);

        DPFN( eDbgLevelWarning, "esi=%08lx, edi=%08lx, esp=%08lx, ebp=%08lx\n", 
            lpContext->Esi, 
            lpContext->Edi, 
            lpContext->Esp, 
            lpContext->Ebp);

        DPFN( eDbgLevelWarning, "cs=%04lx, ss=%04lx, ds=%04lx, es=%04lx, fs=%04lx, gs=%04lx\n", 
            lpContext->SegCs, 
            lpContext->SegSs, 
            lpContext->SegDs, 
            lpContext->SegEs,
            lpContext->SegFs,
            lpContext->SegGs);
    #endif

    LONG lRet;

    if (bIgnore) 
    {
		#if defined(_WIN64)
        if ((DWORD)lpContext->Rip <= (DWORD)0xFFFF)
		#else
        if ((DWORD)lpContext->Eip <= (DWORD)0xFFFF)
		#endif
        {
            LOGN( eDbgLevelError, "[ExceptionFilter] Exception %S (%08X), stuck at bad address, killing current thread.", szException, dwCode);    
            lRet = EXCEPTION_CONTINUE_SEARCH;
            return lRet;
        }

        LOGN( eDbgLevelWarning, "[ExceptionFilter] Exception %S (%08X) ignored.", szException, dwCode);

		#if defined(_WIN64)
        lpContext->Rip += GetInstructionLengthFromAddress((LPBYTE)lpContext->Rip);
		#else
        lpContext->Eip += GetInstructionLengthFromAddress((LPBYTE)lpContext->Eip);
		#endif
        g_dwLastEip = 0;
        lRet = EXCEPTION_CONTINUE_EXECUTION;
    }
    else
    {
        DPFN( eDbgLevelWarning, "Exception NOT handled\n\n");
        lRet = EXCEPTION_CONTINUE_SEARCH;
    }

    return lRet;
}

/*++

 Parse the command line for particular exceptions. The format of the command
 line is:

    [EXCEPTION_NAME[:0|1|2]];[EXCEPTION_NAME[:0|1|2]]...

 or "*" which ignores all first chance exceptions.

 Eg:
    ACCESS_VIOLATION:2;PRIV_INSTRUCTION:0;BREAKPOINT

 Will ignore:
    1. Access violations            - second chance
    2. Priviliged mode instructions - do not ignore
    3. Breakpoints                  - ignore

--*/

BOOL 
ParseCommandLine(
    LPCSTR lpCommandLine
    )
{
    CSTRING_TRY
    {
        CStringToken csTok(lpCommandLine, L" ;");
        //
        // Run the string, looking for exception names
        //
        
        CString token;
    
        // Each cl token may be followed by a : and an exception type
        // Forms can be:
        // *
        // *:SecondChance
        // INVALID_DISPOSITION
        // INVALID_DISPOSITION:Active
        // INVALID_DISPOSITION:0
        //
 
        while (csTok.GetToken(token))
        {
            CStringToken csSingleTok(token, L":");

            CString csExcept;
            CString csType;

            // grab the exception name and the exception type
            csSingleTok.GetToken(csExcept);
            csSingleTok.GetToken(csType);
            
            // Convert ignore value to emode (defaults to eFirstChance)
            EMODE emode = ToEmode(csType);

            if (token.Compare(L"*") == 0)
            {
                for (int i = 0; i < ELISTSIZE; i++)
                {
                    g_eList[i].dwIgnore = emode;
                }
            }
            else
            {
                // Find the exception specified
                for (int i = 0; i < ELISTSIZE; i++)
                {
                    if (csExcept.CompareNoCase(g_eList[i].cName) == 0)
                    {
                        g_eList[i].dwIgnore = emode;
                        break;
                    }
                }
            }
        }
    }
    CSTRING_CATCH
    {
        return FALSE;
    }

    //
    // Dump results of command line parse
    //

    DPFN( eDbgLevelInfo, "===================================\n");
    DPFN( eDbgLevelInfo, "          Ignore Exception         \n");
    DPFN( eDbgLevelInfo, "===================================\n");
    DPFN( eDbgLevelInfo, "  1 = First chance                 \n");
    DPFN( eDbgLevelInfo, "  2 = Second chance                \n");
    DPFN( eDbgLevelInfo, "  3 = ExitProcess on second chance \n");
    DPFN( eDbgLevelInfo, "-----------------------------------\n");
    for (int i = 0; i < ELISTSIZE; i++)
    {
        if (g_eList[i].dwIgnore != eActive)
        {
            DPFN( eDbgLevelInfo, "%S %S\n", ToWchar(g_eList[i].dwIgnore), g_eList[i].cName);
        }
    }

    DPFN( eDbgLevelInfo, "-----------------------------------\n");

    return TRUE;
}

/*++

 Register hooked functions

--*/

BOOL
NOTIFY_FUNCTION(
    DWORD fdwReason)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        // Run the command line to check for adjustments to defaults
        if (!ParseCommandLine(COMMAND_LINE))
        {
            return FALSE;
        }
    
        // Try to find new exception handler
        _pfn_RtlAddVectoredExceptionHandler pfnExcept;
        pfnExcept = (_pfn_RtlAddVectoredExceptionHandler)
            GetProcAddress(
                GetModuleHandle(L"NTDLL.DLL"), 
                "RtlAddVectoredExceptionHandler");

        if (pfnExcept)
        {
            (_pfn_RtlAddVectoredExceptionHandler) pfnExcept(
                0, 
                (PVOID)ExceptionFilter);
            g_bWin2000 = FALSE;
        }
        else
        {
            // Windows 2000 reverts back to the old method which unluckily 
            // doesn't get called for C++ exceptions

            SetUnhandledExceptionFilter(ExceptionFilter);
            g_bWin2000 = TRUE;
        }
    }

    return TRUE;
}

HOOK_BEGIN

    CALL_NOTIFY_FUNCTION

HOOK_END

IMPLEMENT_SHIM_END

