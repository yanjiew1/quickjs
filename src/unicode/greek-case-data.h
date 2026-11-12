/* Greek casing data from ICU4X commit
 * 56e2150c77decde2bfaf34b8265441714549e005, components/casemap/src/greek_to_me/data.rs.
 *
 * UNICODE LICENSE V3
 *
 * COPYRIGHT AND PERMISSION NOTICE
 *
 * Copyright © 2020-2024 Unicode, Inc.
 *
 * NOTICE TO USER: Carefully read the following legal agreement. BY
 * DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING DATA FILES, AND/OR
 * SOFTWARE, YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
 * TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO NOT
 * DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of data files and any associated documentation (the "Data Files") or
 * software and any associated documentation (the "Software") to deal in the
 * Data Files or Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Data Files or Software, and to permit persons to whom the
 * Data Files or Software are furnished to do so, provided that either (a)
 * this copyright and permission notice appear with all copies of the Data
 * Files or Software, or (b) this copyright and permission notice appear in
 * associated Documentation.
 *
 * THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
 * BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
 * FILES OR SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in these Data Files or Software without prior written
 * authorization of the copyright holder.
 *
 * SPDX-License-Identifier: Unicode-3.0
 *
 * —
 *
 * Portions of ICU4X may have been adapted from ICU4C and/or ICU4J.
 * ICU 1.8.1 to ICU 57.1 © 1995-2016 International Business Machines Corporation and others.
 */

static const uint8_t greek_case_370[] = {
    128, 128, 128, 128, 0, 0, 128, 128, 0, 0, 128, 128, 128, 128, 0, 128,
    0, 0, 0, 0, 0, 0, 65, 0, 66, 67, 68, 0, 69, 0, 70, 71,
    100, 1, 128, 128, 128, 2, 128, 3, 128, 4, 128, 128, 128, 128, 128, 5,
    128, 129, 0, 128, 128, 6, 128, 128, 128, 7, 36, 38, 65, 66, 67, 68,
    102, 1, 128, 128, 128, 2, 128, 3, 128, 4, 128, 128, 128, 128, 128, 5,
    128, 129, 128, 128, 128, 6, 128, 128, 128, 7, 36, 38, 69, 70, 71, 128,
    128, 128, 8, 72, 40, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    128, 129, 128, 128, 128, 2, 0, 128, 128, 128, 128, 128, 128, 128, 128, 128,
};
static const uint8_t greek_case_1f00[] = {
    1, 1, 65, 65, 65, 65, 65, 65, 1, 1, 65, 65, 65, 65, 65, 65,
    2, 2, 66, 66, 66, 66, 0, 0, 2, 2, 66, 66, 66, 66, 0, 0,
    3, 3, 67, 67, 67, 67, 67, 67, 3, 3, 67, 67, 67, 67, 67, 67,
    4, 4, 68, 68, 68, 68, 68, 68, 4, 4, 68, 68, 68, 68, 68, 68,
    5, 5, 69, 69, 69, 69, 0, 0, 5, 5, 69, 69, 69, 69, 0, 0,
    6, 6, 70, 70, 70, 70, 70, 70, 0, 6, 0, 70, 0, 70, 0, 70,
    7, 7, 71, 71, 71, 71, 71, 71, 7, 7, 71, 71, 71, 71, 71, 71,
    65, 65, 66, 66, 67, 67, 68, 68, 69, 69, 70, 70, 71, 71, 0, 0,
    17, 17, 81, 81, 81, 81, 81, 81, 17, 17, 81, 81, 81, 81, 81, 81,
    19, 19, 83, 83, 83, 83, 83, 83, 19, 19, 83, 83, 83, 83, 83, 83,
    23, 23, 87, 87, 87, 87, 87, 87, 23, 23, 87, 87, 87, 87, 87, 87,
    1, 1, 81, 17, 81, 0, 65, 81, 1, 1, 65, 65, 17, 0, 4, 0,
    0, 0, 83, 19, 83, 0, 67, 83, 66, 66, 67, 67, 19, 0, 0, 0,
    4, 4, 100, 100, 0, 0, 68, 100, 4, 4, 68, 68, 0, 0, 0, 0,
    6, 6, 102, 102, 129, 129, 70, 102, 6, 6, 70, 70, 129, 0, 0, 0,
    0, 0, 87, 23, 87, 0, 71, 87, 69, 69, 71, 71, 23,
};
