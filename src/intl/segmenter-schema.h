/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Additive private schema1.3 proposal; frozen metadata1.2 is untouched. */
#ifndef QJS_INTL_SEGMENTER_SCHEMA_H
#define QJS_INTL_SEGMENTER_SCHEMA_H
#define QJS_INTL_DATA_SEGMENT_GCB 60u
#define QJS_INTL_DATA_SEGMENT_WB 61u
#define QJS_INTL_DATA_SEGMENT_SB 62u
#define QJS_INTL_DATA_SEGMENT_INCB 63u
#define QJS_INTL_DATA_SEGMENT_EP 64u
#define QJS_INTL_DATA_SEGMENT_RULE 65u
#define QJS_INTL_SEGMENT_RANGE_WIDTH 12u
#define QJS_INTL_SEGMENT_RULE_WIDTH 24u
#define QJS_INTL_SEGMENT_UNICODE_VERSION (18u << 16)
#define QJS_INTL_SEGMENT_UAX29_REVISION 49u
#define QJS_INTL_SEGMENT_ALGORITHM_VERSION 1u
#define QJS_INTL_SEGMENT_EP_PACKED 1u
#define QJS_INTL_SEGMENT_EP_LIBUNICODE 2u
/* 60..64: first:u32@0,last:u32@4 inclusive,value:u32@8. Non-default
 * property values use the explicit enums in segmenter-native.h.64 value1.
 * Sorted disjoint scalar ranges; adjacent same-value rows are coalesced.
 * No surrogate intersection, zero/default rows, or enum outside domain.
 * 65 has exactly3 rows sorted granularity0,1,2:
 * granularity:u32@0, unicode_version:u32@4, uax29_revision:u32@8,
 * algorithm_version:u32@12, ep_source:u32@16, reserved_zero:u32@20.
 * All3 use18.0.0/revision49/algorithm1 and identical ep_source1 or2.
 * ep_source1 requires nonempty64; ep_source2 forbids64 and requires existing
 * Unicode18 libunicode EP lookup.60..63 nonempty and65 are an atomic group.
 * No locale tailoring, dictionary or service-coverage bits exist here.
 * Schema1.0..1.2 forbid60..65;1.3 allows these independently of other owners.
 * Header Unicode18.0.0 and CLDR49.0.0 required when group is present.
 * Provider digest/source checks establish complete property coverage; shape
 * validation alone cannot prove that a table matches the pinned UCD. */
#endif
