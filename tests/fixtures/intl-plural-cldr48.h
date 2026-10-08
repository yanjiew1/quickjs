/*
 * UNICODE LICENSE V3
 *
 * COPYRIGHT AND PERMISSION NOTICE
 *
 * Copyright © 2016-2025 Unicode, Inc.
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
 */
/* Rule data: Copyright (c) 2016 and later Unicode, Inc. and others.
 * Generated from ICU 78.3 data/misc/plurals.txt (CLDR 48); samples omitted.
 * Source SHA256 b818b009597506b8c7eee32a77007f8e55154c26f35d42d87ea75b5cf48d7d6c.
 * Unicode data license: https://www.unicode.org/copyright.html */
static const char *const cldr48_plural_rules[] = {
    "", /* other */
    "i = 0 or n = 1", /* one */
    "", /* other */
    "v = 0 and i % 10 = 1 and i % 100 != 11 or f % 10 = 1 and f % 100 != 11", /* one */
    "", /* other */
    "v = 0 and i = 1,2,3 or v = 0 and i % 10 != 4,6,9 or v != 0 and f % 10 != 4,6,9", /* one */
    "", /* other */
    "n % 10 = 1 and n % 100 != 11 or v = 2 and f % 10 = 1 and f % 100 != 11 or v != 2 and f % 10 = 1", /* one */
    "", /* other */
    "n % 10 = 0 or n % 100 = 11..19 or v = 2 and f % 100 = 11..19", /* zero */
    "i = 0,1 and n != 0", /* one */
    "", /* other */
    "n = 0", /* zero */
    "n = 1", /* one */
    "", /* other */
    "n = 0", /* zero */
    "i = 1 and v = 0 or i = 0 and v != 0", /* one */
    "", /* other */
    "i = 2 and v = 0", /* two */
    "n = 1", /* one */
    "", /* other */
    "n = 2", /* two */
    "n = 2..10", /* few */
    "i = 0 or n = 1", /* one */
    "", /* other */
    "v != 0 or n = 0 or n != 1 and n % 100 = 1..19", /* few */
    "i = 1 and v = 0", /* one */
    "", /* other */
    "v = 0 and i % 10 = 2..4 and i % 100 != 12..14 or f % 10 = 2..4 and f % 100 != 12..14", /* few */
    "v = 0 and i % 10 = 1 and i % 100 != 11 or f % 10 = 1 and f % 100 != 11", /* one */
    "", /* other */
    "i = 0,1", /* one */
    "", /* other */
    "e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5", /* many */
    "i = 0,1", /* one */
    "", /* other */
    "e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5", /* many */
    "i = 0..1", /* one */
    "", /* other */
    "e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5", /* many */
    "i = 1 and v = 0", /* one */
    "", /* other */
    "e = 0 and i != 0 and i % 1000000 = 0 and v = 0 or e != 0..5", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 3..10,13..19", /* few */
    "n = 1,11", /* one */
    "", /* other */
    "n = 2,12", /* two */
    "v = 0 and i % 100 = 3..4 or v != 0", /* few */
    "v = 0 and i % 100 = 1", /* one */
    "", /* other */
    "v = 0 and i % 100 = 2", /* two */
    "v = 0 and i % 100 = 3..4 or f % 100 = 3..4", /* few */
    "v = 0 and i % 100 = 1 or f % 100 = 1", /* one */
    "", /* other */
    "v = 0 and i % 100 = 2 or f % 100 = 2", /* two */
    "i = 2..4 and v = 0", /* few */
    "v != 0", /* many */
    "i = 1 and v = 0", /* one */
    "", /* other */
    "v = 0 and i % 10 = 2..4 and i % 100 != 12..14", /* few */
    "v = 0 and i != 1 and i % 10 = 0..1 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 12..14", /* many */
    "i = 1 and v = 0", /* one */
    "", /* other */
    "n % 10 = 2..4 and n % 100 != 12..14", /* few */
    "n % 10 = 0 or n % 10 = 5..9 or n % 100 = 11..14", /* many */
    "n % 10 = 1 and n % 100 != 11", /* one */
    "", /* other */
    "i = 1 and v = 0", /* one */
    "", /* other */
    "n % 10 = 2..9 and n % 100 != 11..19", /* few */
    "f != 0", /* many */
    "n % 10 = 1 and n % 100 != 11..19", /* one */
    "", /* other */
    "v = 0 and i % 10 = 2..4 and i % 100 != 12..14", /* few */
    "v = 0 and i % 10 = 0 or v = 0 and i % 10 = 5..9 or v = 0 and i % 100 = 11..14", /* many */
    "v = 0 and i % 10 = 1 and i % 100 != 11", /* one */
    "", /* other */
    "n != 2 and n % 10 = 2..9 and n % 100 != 11..19", /* few */
    "f != 0", /* many */
    "n % 10 = 1 and n % 100 != 11", /* one */
    "", /* other */
    "n = 2", /* two */
    "n % 10 = 3..4,9 and n % 100 != 10..19,70..79,90..99", /* few */
    "n != 0 and n % 1000000 = 0", /* many */
    "n % 10 = 1 and n % 100 != 11,71,91", /* one */
    "", /* other */
    "n % 10 = 2 and n % 100 != 12,72,92", /* two */
    "n = 0 or n % 100 = 3..10", /* few */
    "n % 100 = 11..19", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 2", /* two */
    "n = 3..6", /* few */
    "n = 7..10", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 2", /* two */
    "v = 0 and i % 100 = 0,20,40,60,80", /* few */
    "v != 0", /* many */
    "v = 0 and i % 10 = 1", /* one */
    "", /* other */
    "v = 0 and i % 10 = 2", /* two */
    "n % 100 = 3,23,43,63,83", /* few */
    "n != 1 and n % 100 = 1,21,41,61,81", /* many */
    "n = 1", /* one */
    "", /* other */
    "n % 100 = 2,22,42,62,82 or n % 1000 = 0 and n % 100000 = 1000..20000,40000,60000,80000 or n != 0 and n % 1000000 = 100000", /* two */
    "n = 0", /* zero */
    "n % 100 = 3..10", /* few */
    "n % 100 = 11..99", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 2", /* two */
    "n = 0", /* zero */
    "n = 3", /* few */
    "n = 6", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 2", /* two */
    "n = 0", /* zero */
    "n = 0,1 or i = 0 and f = 1", /* one */
    "", /* other */
    "", /* other */
    "n % 10 = 1,2 and n % 100 != 11,12", /* one */
    "", /* other */
    "n = 1", /* one */
    "", /* other */
    "n = 1,5", /* one */
    "", /* other */
    "n = 1..4", /* one */
    "", /* other */
    "n % 10 = 2,3 and n % 100 != 12,13", /* few */
    "", /* other */
    "n % 10 = 3 and n % 100 != 13", /* few */
    "", /* other */
    "n % 10 = 6,9 or n = 10", /* few */
    "", /* other */
    "n % 10 = 6 or n % 10 = 9 or n % 10 = 0 and n != 0", /* many */
    "", /* other */
    "n = 11,8,80,800", /* many */
    "", /* other */
    "n = 0..1", /* one */
    "", /* other */
    "n = 11,8,80..89,800..899", /* many */
    "", /* other */
    "i = 0 or i % 100 = 2..20,40,60,80", /* many */
    "i = 1", /* one */
    "", /* other */
    "n % 10 = 4 and n % 100 != 14", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 5 or n % 100 = 5", /* many */
    "n = 1..4 or n % 100 = 1..4,21..24,41..44,61..64,81..84", /* one */
    "", /* other */
    "i = 2,3,4,5,6", /* few */
    "i = 1", /* one */
    "", /* other */
    "i = 0", /* zero */
    "n % 10 = 3 and n % 100 != 13", /* few */
    "n % 10 = 1 and n % 100 != 11", /* one */
    "", /* other */
    "n % 10 = 2 and n % 100 != 12", /* two */
    "n = 4", /* few */
    "n = 1", /* one */
    "", /* other */
    "n = 2,3", /* two */
    "n = 3,13", /* few */
    "n = 1,11", /* one */
    "", /* other */
    "n = 2,12", /* two */
    "n = 4", /* few */
    "n = 1,3", /* one */
    "", /* other */
    "n = 2", /* two */
    "i % 10 = 7,8 and i % 100 != 17,18", /* many */
    "i % 10 = 1 and i % 100 != 11", /* one */
    "", /* other */
    "i % 10 = 2 and i % 100 != 12", /* two */
    "n = 0..1 or n = 11..99", /* one */
    "", /* other */
    "i % 10 = 3,4 or i % 1000 = 100,200,300,400,500,600,700,800,900", /* few */
    "i = 0 or i % 10 = 6 or i % 100 = 40,60,90", /* many */
    "i % 10 = 1,2,5,7,8 or i % 100 = 20,50,70,80", /* one */
    "", /* other */
    "n = 4", /* few */
    "n = 6", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 2,3", /* two */
    "n = 4", /* few */
    "n = 6", /* many */
    "n = 1,5,7,8,9,10", /* one */
    "", /* other */
    "n = 2,3", /* two */
    "n = 4", /* few */
    "n = 6", /* many */
    "n = 1,5,7..9", /* one */
    "", /* other */
    "n = 2,3", /* two */
    "n = 3,4", /* few */
    "n = 5,6", /* many */
    "n = 1", /* one */
    "", /* other */
    "n = 2", /* two */
    "n = 0,7,8,9", /* zero */
    "n = 1", /* one */
    "", /* other */
    "n = 1 or t != 0 and i = 0,1", /* one */
    "", /* other */
    "t = 0 and i % 10 = 1 and i % 100 != 11 or t % 10 = 1 and t % 100 != 11", /* one */
    "", /* other */
};
