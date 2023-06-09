/*
 * Copyright (C) 2004-2023 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ZNC_FILEUTILS_H
#define ZNC_FILEUTILS_H

#ifndef WINDOWSH
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define WINDOWSH
#endif

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <direct.h>
#include <fileapi.h>
#include <stdio.h>
class CFile {
  public:
    CFile();
    CFile(const CString& sLongName);
    ~CFile();

    enum EFileTypes {
        FT_REGULAR,
        FT_DIRECTORY,
        FT_CHARACTER,
        FT_BLOCK,
        FT_FIFO,
        FT_LINK,
        FT_SOCK
    };

    void SetFileName(const CString& sLongName);
    static bool IsReg(const CString& sLongName, bool bUseLstat = false);
    static bool IsDir(const CString& sLongName, bool bUseLstat = false);
    static bool IsChr(const CString& sLongName, bool bUseLstat = false);
    static bool IsBlk(const CString& sLongName, bool bUseLstat = false);
    static bool IsFifo(const CString& sLongName, bool bUseLstat = false);
    static bool IsLnk(const CString& sLongName, bool bUseLstat = true);
    static bool IsSock(const CString& sLongName, bool bUseLstat = false);

    bool IsReg(bool bUseLstat = false) const;
    bool IsDir(bool bUseLstat = false) const;
    bool IsChr(bool bUseLstat = false) const;
    bool IsBlk(bool bUseLstat = false) const;
    bool IsFifo(bool bUseLstat = false) const;
    bool IsLnk(bool bUseLstat = true) const;
    bool IsSock(bool bUseLstat = false) const;

    // for gettin file types, using fstat instead
    static bool FType(const CString& sFileName, EFileTypes eType,
                      bool bUseLstat = false);

    enum EFileAttr { FA_Name, FA_Size, FA_ATime, FA_MTime, FA_CTime, FA_UID };

    //
    // Functions to retrieve file information
    //
    bool Exists() const;
    off_t GetSize() const;
    time_t GetATime() const;
    time_t GetMTime() const;
    time_t GetCTime() const;
    static bool Exists(const CString& sFile);

    static off_t GetSize(const CString& sFile);
    static time_t GetATime(const CString& sFile);
    static time_t GetMTime(const CString& sFile);
    static time_t GetCTime(const CString& sFile);
    static int GetInfo(const CString& sFile, struct stat& st);

    //
    // Functions to manipulate the file on the filesystem
    //
    bool Delete();
    bool Move(const CString& sNewFileName, bool bOverwrite = false);
    bool Copy(const CString& sNewFileName, bool bOverwrite = false);

    static bool Delete(const CString& sFileName);
    static bool Move(const CString& sOldFileName, const CString& sNewFileName,
                     bool bOverwrite = false);
    static bool Copy(const CString& sOldFileName, const CString& sNewFileName,
                     bool bOverwrite = false);
    bool Seek(off_t uPos);
    bool Truncate();
    bool Sync();
    bool Open(const CString& sFileName, int iFlags = _O_RDONLY);
    bool Open(int iFlags = _O_RDONLY);
    SSIZE_T Read(char* pszBuffer, int iBytes);
    bool ReadLine(CString& sData, const CString& sDelimiter = "\n");
    bool XReadFile(CString& sData, size_t iMaxSize = 512 * 1024);
    SSIZE_T Write(const char* pszBuffer, size_t iBytes);
    SSIZE_T Write(const CString& sData);
    void Close();
    void ClearBuffer();

    bool TryExLock(const CString& sLockFile, int iFlags = _O_RDWR | _O_CREAT);
    /*bool TryExLock();
    bool ExLock();
    bool UnLock();*/

    bool IsOpen() const;
    CString GetLongName() const;
    CString GetShortName() const;
    CString GetDir() const;

    bool HadError() const { return m_bHadError; }
    void ResetError() { m_bHadError = false; }

    static void InitHomePath(const CString& sFallback);
    static const CString& GetHomePath() { return m_sHomePath; }

  private:
    // fcntl() locking wrapper
    bool Lock(short iType, bool bBlocking);

    CString m_sBuffer;
    int m_iFD;
    bool m_bHadError;

    static CString m_sHomePath;

  protected:
    CString m_sLongName;   //!< Absolute filename (m_sPath + "/" + m_sShortName)
    CString m_sShortName;  //!< Filename alone, without path
};

class CDir : public std::vector<CFile*> {
  public:
    CDir(const CString& sDir) : m_eSortAttr(CFile::FA_Name), m_bDesc(false) {
        Fill(sDir);
    }

    CDir() : m_eSortAttr(CFile::FA_Name), m_bDesc(false) {}

    ~CDir() { CleanUp(); }

    void CleanUp() {
        for (unsigned int a = 0; a < size(); a++) {
            delete (*this)[a];
        }

        clear();
    }

    size_t Fill(const CString& sDir) { return FillByWildcard(sDir, "*"); }

    size_t FillByWildcard(const CString& sDir, const CString& sWildcard) {
        CleanUp();
        HANDLE hFind;
        WIN32_FIND_DATA FindFileData;
        const char* sDirC = sDir.c_str();
        int sDirL = strlen(sDirC);
        const char* sWildcardC = sWildcard.c_str();
        int sWildcardL = strlen(sWildcardC);
        char* fullString = (char*)malloc(sDirL + 1 + sWildcardL + 1);
        snprintf(fullString, sDirL + 1 + sWildcardL + 1, "%s\\%s", sDirC,
                  +sWildcardC);
        if ((hFind = FindFirstFile(fullString, &FindFileData)) != (HANDLE)-1) {
            do {
                std::string sf =
                    sDir.TrimSuffix_n("\\") + "\\" + FindFileData.cFileName;
                CFile* file = new CFile(sf);
                push_back(file);
                
            }
            while (FindNextFile(hFind, &FindFileData));
            FindClose(hFind);
        }
        return size();
    }
    
    static unsigned int Delete(const CString& sWildcard,
                               const CString& sDir = ".") {
        CDir cDir;
        cDir.FillByWildcard(sDir, sWildcard);
        return cDir.Delete();
    }

    unsigned int Delete() {
        unsigned int uRet = 0;
        for (unsigned int a = 0; a < size(); a++) {
            if ((*this)[a]->Delete()) {
                uRet++;
            }
        }

        return uRet;
    }

    CFile::EFileAttr GetSortAttr() const { return m_eSortAttr; }
    bool IsDescending() const { return m_bDesc; }

    // Check if sPath + "/" + sAdd (~/ is handled) is an absolute path which
    // resides under sPath. Returns absolute path on success, else "".
    static CString CheckPathPrefix(const CString& sPath, const CString& sAdd,
                                   const CString& sHomeDir = "");
    static CString ChangeDir(const CString& sPath, const CString& sAdd,
                             const CString& sHomeDir = "");
    static bool MakeDir(const CString& sPath);

    static CString GetCWD() {
        CString sRet;
        char* pszCurDir = _getcwd(nullptr, 0);
        if (pszCurDir) {
            sRet = pszCurDir;
            free(pszCurDir);
        }

        return sRet;
    }

  private:
  protected:
    CFile::EFileAttr m_eSortAttr;
    bool m_bDesc;
};
#endif  // !ZNC_FILEUTILS_H
