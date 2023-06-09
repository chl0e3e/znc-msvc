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

#include <gtest/gtest.h>
#include <znc/FileUtils.h>
#include <znc/Config.h>
/* mkstemp extracted from libc/sysdeps/posix/tempname.c.  Copyright
   (C) 1991-1999, 2000, 2001, 2006 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.  */

static const char letters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Generate a temporary file name based on TMPL.  TMPL must match the
   rules for mk[s]temp (i.e. end in "XXXXXX").  The name constructed
   does not exist at the time of the call to mkstemp.  TMPL is
   overwritten with the result.  */
int mkstemp(char* tmpl) {
    int len;
    char* XXXXXX;
    static unsigned long long value;
    unsigned long long random_time_bits;
    unsigned int count;
    int fd = -1;
    int save_errno = errno;

    /* A lower bound on the number of temporary files to attempt to
       generate.  The maximum total number of temporary file names that
       can exist for a given template is 62**6.  It should never be
       necessary to try all these combinations.  Instead if a reasonable
       number of names is tried (we define reasonable as 62**3) fail to
       give the system administrator the chance to remove the problems.  */
#define ATTEMPTS_MIN (62 * 62 * 62)

    /* The number of times to attempt to generate a temporary file.  To
       conform to POSIX, this must be no smaller than TMP_MAX.  */
#if ATTEMPTS_MIN < TMP_MAX
    unsigned int attempts = TMP_MAX;
#else
    unsigned int attempts = ATTEMPTS_MIN;
#endif

    len = strlen(tmpl);
    if (len < 6 || strcmp(&tmpl[len - 6], "XXXXXX")) {
        errno = EINVAL;
        return -1;
    }

    /* This is where the Xs start.  */
    XXXXXX = &tmpl[len - 6];

    /* Get some more or less random data.  */
    {
        SYSTEMTIME stNow;
        FILETIME ftNow;

        // get system time
        GetSystemTime(&stNow);
        stNow.wMilliseconds = 500;
        if (!SystemTimeToFileTime(&stNow, &ftNow)) {
            errno = -1;
            return -1;
        }

        random_time_bits = (((unsigned long long)ftNow.dwHighDateTime << 32) |
                            (unsigned long long)ftNow.dwLowDateTime);
    }
    value += random_time_bits ^ (unsigned long long)GetCurrentThreadId();

    for (count = 0; count < attempts; value += 7777, ++count) {
        unsigned long long v = value;

        /* Fill in the random bits.  */
        XXXXXX[0] = letters[v % 62];
        v /= 62;
        XXXXXX[1] = letters[v % 62];
        v /= 62;
        XXXXXX[2] = letters[v % 62];
        v /= 62;
        XXXXXX[3] = letters[v % 62];
        v /= 62;
        XXXXXX[4] = letters[v % 62];
        v /= 62;
        XXXXXX[5] = letters[v % 62];

        fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL, _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            errno = save_errno;
            return fd;
        } else if (errno != EEXIST)
            return -1;
    }

    /* We got out of the loop because we ran out of combinations to try.  */
    errno = EEXIST;
    return -1;
}

class CConfigTest : public ::testing::Test {
  public:
    ~CConfigTest() override { m_File.Delete(); }

  protected:
    CFile& WriteFile(const CString& sConfig) {
        char sName[] = "./temp-XXXXXX";
        int fd = mkstemp(sName);
        m_File.Open(sName, _O_RDWR);
        close(fd);

        m_File.Write(sConfig);

        return m_File;
    }

  private:
    CFile m_File;
};

class CConfigErrorTest : public CConfigTest {
  public:
    void TEST_ERROR(const CString& sConfig, const CString& sExpectError) {
        CFile& File = WriteFile(sConfig);

        CConfig conf;
        CString sError;
        EXPECT_FALSE(conf.Parse(File, sError));

        EXPECT_EQ(sError, sExpectError);
    }
};

class CConfigSuccessTest : public CConfigTest {
  public:
    void TEST_SUCCESS(const CString& sConfig, const CString& sExpectedOutput) {
        CFile& File = WriteFile(sConfig);
        // Verify that Parse() rewinds the file
        File.Seek(12);

        CConfig conf;
        CString sError;
        EXPECT_TRUE(conf.Parse(File, sError)) << sError;
        EXPECT_TRUE(sError.empty()) << "Non-empty error string!";

        CString sOutput;
        ToString(sOutput, conf);

        EXPECT_EQ(sOutput, sExpectedOutput);
    }

    void ToString(CString& sRes, CConfig& conf) {
        CConfig::EntryMapIterator it = conf.BeginEntries();
        while (it != conf.EndEntries()) {
            const CString& sKey = it->first;
            const VCString& vsEntries = it->second;
            VCString::const_iterator i = vsEntries.begin();
            if (i == vsEntries.end())
                sRes += sKey + " <- Error, empty list!\n";
            else
                while (i != vsEntries.end()) {
                    sRes += sKey + "=" + *i + "\n";
                    ++i;
                }
            ++it;
        }

        CConfig::SubConfigMapIterator it2 = conf.BeginSubConfigs();
        while (it2 != conf.EndSubConfigs()) {
            auto it3 = it2->second.begin();

            while (it3 != it2->second.end()) {
                sRes += "->" + it2->first + "/" + it3->first + "\n";
                ToString(sRes, *it3->second.m_pSubConfig);
                sRes += "<-\n";
                ++it3;
            }

            ++it2;
        }
    }

  private:
};

TEST_F(CConfigSuccessTest, Empty) { TEST_SUCCESS("", ""); }

/* duplicate entries */
TEST_F(CConfigSuccessTest, Duble1) {
    TEST_SUCCESS("Foo = bar\nFoo = baz\n", "foo=bar\nfoo=baz\n");
}
TEST_F(CConfigSuccessTest, Duble2) {
    TEST_SUCCESS("Foo = baz\nFoo = bar\n", "foo=baz\nfoo=bar\n");
}

/* sub configs */
TEST_F(CConfigErrorTest, SubConf1) {
    TEST_ERROR("</foo>",
               "Error on line 1: Closing tag \"foo\" which is not open.");
}
TEST_F(CConfigErrorTest, SubConf2) {
    TEST_ERROR("<foo a>\n</bar>\n",
               "Error on line 2: Closing tag \"bar\" which is not open.");
}
TEST_F(CConfigErrorTest, SubConf3) {
    TEST_ERROR("<foo bar>",
               "Error on line 1: Not all tags are closed at the end of the "
               "file. Inner-most open tag is \"foo\".");
}
TEST_F(CConfigErrorTest, SubConf4) {
    TEST_ERROR("<foo>\n</foo>",
               "Error on line 1: Empty block name at begin of block.");
}
TEST_F(CConfigErrorTest, SubConf5) {
    TEST_ERROR("<foo 1>\n</foo>\n<foo 1>\n</foo>",
               "Error on line 4: Duplicate entry for tag \"foo\" name \"1\".");
}
TEST_F(CConfigSuccessTest, SubConf6) {
    TEST_SUCCESS("<foo a>\n</foo>", "->foo/a\n<-\n");
}
TEST_F(CConfigSuccessTest, SubConf7) {
    TEST_SUCCESS("<a b>\n  <c d>\n </c>\n</a>", "->a/b\n->c/d\n<-\n<-\n");
}
TEST_F(CConfigSuccessTest, SubConf8) {
    TEST_SUCCESS(" \t <A B>\nfoo = bar\n\tFooO = bar\n</a>",
                 "->a/B\nfoo=bar\nfooo=bar\n<-\n");
}
// ensure order is preserved i.e. subconfigs should not be sorted by name
TEST_F(CConfigSuccessTest, SubConf9) {
    TEST_SUCCESS("<foo b>\n</foo>\n<foo a>\n</foo>",
                 "->foo/b\n<-\n->foo/a\n<-\n");
}

/* comments */
TEST_F(CConfigSuccessTest, Comment1) {
    TEST_SUCCESS("Foo = bar // baz\n// Bar = baz", "foo=bar // baz\n");
}
TEST_F(CConfigSuccessTest, Comment2) {
    TEST_SUCCESS(
        "Foo = bar /* baz */\n/*** Foo = baz ***/\n   /**** asdsdfdf \n Some "
        "quite invalid stuff ***/\n",
        "foo=bar /* baz */\n");
}
TEST_F(CConfigErrorTest, Comment3) {
    TEST_ERROR("<foo foo>\n/* Just a comment\n</foo>",
               "Error on line 3: Comment not closed at end of file.");
}
TEST_F(CConfigSuccessTest, Comment4) { TEST_SUCCESS("/* Foo\n/* Bar */", ""); }
TEST_F(CConfigSuccessTest, Comment5) { TEST_SUCCESS("/* Foo\n// */", ""); }
