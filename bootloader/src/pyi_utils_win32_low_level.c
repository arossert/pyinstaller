/*
 * ****************************************************************************
 * Copyright (c) 2013-2023, PyInstaller Development Team.
 *
 * Distributed under the terms of the GNU General Public License (version 2
 * or later) with exception for distributing the bootloader.
 *
 * The full license is in the file COPYING.txt, distributed with this software.
 *
 * SPDX-License-Identifier: (GPL-2.0-or-later WITH Bootloader-exception)
 * ****************************************************************************
 */

/*
 * Utility functions. This file contains low-level helpers that are specific
 * to Windows.
 */

#ifdef _WIN32


/* PyInstaller headers. */
#include "pyi_global.h"  /* PYI_PATH_MAX */
#include "pyi_utils.h"

#ifndef IO_REPARSE_TAG_SYMLINK
    #define IO_REPARSE_TAG_SYMLINK 0xA000000CL
#endif


/**********************************************************************\
 *                 WinError code to string conversion                 *
\**********************************************************************/
/* Return a pointer to a null-terminated string containing a textual
 * description of the given error code. If the error code is zero, the
 * result of GetLastError() is used. The text is localized and
 * UTF8-encoded. The caller is *NOT* responsible for freeing this
 * pointer.
 *
 * Returns a pointer to statically-allocated storage. Not thread safe.
 */
const char *
pyi_win32_get_winerror_string(DWORD error_code)
{
    #define ERROR_STRING_MAX 4096
    static wchar_t local_buffer_w[ERROR_STRING_MAX];
    static char local_buffer[ERROR_STRING_MAX];

    DWORD result;

    if (error_code == 0) {
        error_code = GetLastError();
    }

    /* Note: Giving 0 to dwLanguageID means MAKELANGID(LANG_NEUTRAL,
     * SUBLANG_NEUTRAL), but we should use SUBLANG_DEFAULT instead of
     * SUBLANG_NEUTRAL. Please see the note written in "Language
     * Identifier Constants and Strings" on MSDN.
     * https://docs.microsoft.com/en-us/windows/desktop/intl/language-identifier-constants-and-strings
     */
    result = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM, /* dwFlags */
        NULL, /* lpSource */
        error_code, /* dwMessageId */
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
        local_buffer_w, /* lpBuffer */
        ERROR_STRING_MAX, /* nSize */
        NULL /* Arguments */
    );

    if (!result) {
        return "PyInstaller: FormatMessageW failed.";
    }

    if (pyi_win32_wcs_to_utf8(local_buffer_w, local_buffer, ERROR_STRING_MAX) == NULL) {
        return "PyInstaller: pyi_win32_wcs_to_utf8 failed.";
    }

    return local_buffer;
}


/**********************************************************************\
 *                    Character encoding conversion                   *
\**********************************************************************/
/*
 * Convert wchar_t (UTF16) string into char (UTF8) string.
 *
 * `src` must be NULL-terminated.
 *
 * If `dest` is not NULL, copies the result into the given buffer, which
 * must hold at least `buflen` bytes (including terminating NULL). Returns
 * the given buffer if successful. Returns NULL on encoding failure, or
 * if the UTF-8 encoding requires more than `buflen` bytes.
 *
 * If `dest` is NULL, allocates and returns a new buffer to store the
 * result. The `buflen` argument is ignored. Returns NULL on encoding
 * failure. The caller is responsible for freeing the returned buffer
 * using free().
 */
char *
pyi_win32_wcs_to_utf8(const wchar_t *src, char *dest, size_t buflen)
{
    char *output = NULL;

    if (dest == NULL) {
        /* Query buffer size by passing NULL and 0 for output arguments
         * -1 for cchWideChar means string is null-terminated. */
        buflen = WideCharToMultiByte(
            CP_UTF8, /* CodePage */
            0, /* dwFlags */
            src, /* lpWideCharStr */
            -1, /* cchWideChar */
            NULL, /* lpMultiByteStr */
            0, /* cbMultiByte */
            NULL, /* lpDefaultChar */
            NULL /* lpUsedDefaultChar */
        );

        if (buflen == 0) {
            return NULL;
        }

        output = (char *)calloc(buflen + 1, sizeof(char));
        if (output == NULL) {
            return NULL;
        }
    } else {
        output = dest;
    }

    buflen = WideCharToMultiByte(
        CP_UTF8, /* CodePage */
        0, /* dwFlags */
        src, /* lpWideCharStr */
        -1, /* cchWideChar */
        output, /* lpMultiByteStr */
        (DWORD)buflen, /* cbMultiByte */
        NULL, /* lpDefaultChar */
        NULL /* lpUsedDefaultChar */
    );

    if (buflen == 0) {
        if (dest == NULL) {
            free(output); /* Free allocated buffer */
        }
        output = NULL;
    }

    return output;
}

/*
 * Convert char (UTF8) string into wchar_t (UTF16) string.
 *
 * `src` must be NULL-terminated.
 *
 * If `dest` is not NULL, copies the result into the given buffer, which
 * must hold at least `buflen` characters (including terminating NULL).
 * Returns the given buffer if successful. Returns NULL on encoding
 * failure, or if the UTF-16 encoding requires more than `buflen`
 * characters.
 *
 * If `dest` is NULL, allocates and returns a new buffer to store the
 * result. The `buflen` argument is ignored. Returns NULL on encoding
 * failure. The caller is responsible for freeing the returned buffer
 * using free().
 */
wchar_t *
pyi_win32_utf8_to_wcs(const char *src, wchar_t *dest, size_t buflen)
{
    wchar_t *output = NULL;

    if (dest == NULL) {
        /* Query buffer size by passing NULL and 0 for output arguments
         * -1 for cbMultiByte means string is null-terminated. */
        buflen = MultiByteToWideChar(
            CP_UTF8, /* CodePage */
            0, /* dwFlags */
            src, /* lpMultiByteStr */
            -1, /* cbMultiByte*/
            NULL, /* lpWideCharStr */
            0 /* cchWideChar */
        );

        if (buflen == 0) {
            return NULL;
        }

        output = (wchar_t *)calloc(buflen + 1, sizeof(wchar_t));
        if (output == NULL) {
            return NULL;
        }
    } else {
        output = dest;
    }

    buflen = MultiByteToWideChar(
        CP_UTF8, /* CodePage */
        0, /* dwFlags */
        src, /* lpMultiByteStr */
        -1, /* cbMultiByte */
        output, /* lpWideCharStr */
        (DWORD)buflen /* cchWideChar */
    );

    if (buflen == 0) {
        if (dest == NULL) {
            free(output); /* Free allocated buffer */
        }
        output = NULL;
    }

    return output;
}

/*
 * Convert a wide-char (UTF16) string into an ANSI string.
 *
 *  Returns a newly allocated buffer containing the ANSI characters
 *  terminated by a null character. The caller is responsible for freeing
 *  this buffer with free().
 *
 *  Returns NULL and logs error reason if encoding fails.
 */
static char *
_pyi_win32_wcs_to_mbs(const wchar_t *src)
{
    DWORD dest_size;
    DWORD ret;
    char *dest;

    /* NOTE: CP_ACP means "current ANSI codepage" which is set in the
     * "Language for Non-Unicode Programs" control panel setting. */

    /* Query buffer size by passing NULL and 0 for output arguments */
    dest_size = WideCharToMultiByte(
        CP_ACP, /* CodePage */
        0, /* dwFlags */
        src, /* lpWideCharStr */
        -1, /* cchWideChar*/
        NULL, /* lpMultiByteStr */
        0, /* cbMultiByte */
        NULL, /* lpDefaultChar */
        NULL /* lpUsedDefaultChar */
    );

    if (dest_size == 0) {
        return NULL;
    }

    dest = (char *)calloc(dest_size + 1, sizeof(char));
    if (dest == NULL) {
        return NULL;
    }

    ret = WideCharToMultiByte(
        CP_ACP, /* CodePage */
        0, /* dwFlags */
        src, /* lpWideCharStr */
        -1, /* cchWideChar - length in chars */
        dest, /* lpMultiByteStr */
        dest_size, /* cbMultiByte*/
        NULL, /* lpDefaultChar */
        NULL /* lpUsedDefaultChar */
    );

    if (ret == 0) {
        free(dest);
        dest = NULL;
    }

    return dest;
}

/* Convert an UTF-8 string to an ANSI string.
 *
 * Returns NULL if encoding fails.
 */
char *
pyi_win32_utf8_to_mbs(const char *src, char *dest, size_t buflen)
{
    wchar_t *src_w;
    char *mbs;

    /* UTF8 -> UTF16 */
    src_w = pyi_win32_utf8_to_wcs(src, NULL, 0);
    if (src_w == NULL) {
        return NULL;
    }

    /* UTF16 -> MBS */
    mbs = _pyi_win32_wcs_to_mbs(src_w);

    free(src_w);

    if (mbs == NULL) {
        return NULL;
    }

    if (dest) {
        snprintf(dest, buflen, "%s", mbs);
        free(mbs);
        return dest;
    } else {
        return mbs;
    }
}


/**********************************************************************\
 *                    Misc. file and path helpers                     *
\**********************************************************************/
/* Check if the given path is a symbolic link. */
int pyi_win32_is_symlink(const wchar_t *path)
{
    WIN32_FIND_DATAW info;
    HANDLE ret;

    ret = FindFirstFileExW(path, FindExInfoBasic, &info, FindExSearchNameMatch, NULL, 0);
    if (ret == INVALID_HANDLE_VALUE) {
        /* Failed to look up path; assume it is not symbolic link */
        return 0;
    }
    FindClose(ret);

    if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        if (info.dwReserved0 == IO_REPARSE_TAG_SYMLINK) {
            return 1;
        }
    }

    return 0;
}

/* Equivalent of `realpath()` function on POSIX systems; canonicalize
 * the given path and resolve symbolic links */
int pyi_win32_realpath(const wchar_t *path, wchar_t *resolved_path)
{
    HANDLE handle;
    DWORD ret;

    /* Open file/directory handle */
    handle = CreateFileW(
        path, /* lpFileName */
        0, /* dwDesiredAccess */
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, /* dwShareMode */
        NULL, /* lpSecurityAttributes */
        OPEN_EXISTING, /* dwCreationDisposition */
        FILE_ATTRIBUTE_NORMAL, /* dwFlagsAndAttributes*/
        NULL /* hTemplateFile */
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    /* Fully resolve the path */
    ret = GetFinalPathNameByHandleW(
        handle,  /* hFile */
        resolved_path, /* lpszFilePath */
        PYI_PATH_MAX, /* cchFilePath */
        FILE_NAME_NORMALIZED /* dwFlags */
    );

    CloseHandle(handle);

    if (ret == 0 || ret >= PYI_PATH_MAX) {
        /* Failure or insufficient buffer size */
        return  -1;
    }

    return 0;
}


/* Check if the given path is just a drive letter */
int pyi_win32_is_drive_root(const wchar_t *path)
{
    /* For now, handle just drive letter, optionally followed by the path separator.
       E.g., "C:" or "Z:\".
     */
    size_t len;

    len = wcslen(path);
    if (len == 2 || len == 3) {
        /* First character must be a letter */
        if (!iswalpha(path[0])) {
            return 0;
        }
        /* Second character must be the colon */
        if (path[1] != L':') {
            return 0;
        }
        /* Third character, if present, must be the Windows directory separator */
        if (len > 2 && (path[2] != L'\\')) {
            return 0;
        }

        return 1;
    }

    return 0;
}


#endif /* _WIN32 */