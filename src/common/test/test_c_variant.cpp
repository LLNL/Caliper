#include "caliper/common/cali_variant.h"

#include "gtest/gtest.h"

#include <cstring>

//
// --- test variant creation
//

TEST(C_Variant_Test, CreateEmptyVariant) {
    cali_variant_t v = cali_make_empty_variant();
    
    EXPECT_TRUE(cali_variant_is_empty(v));
    
    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_INV);
    EXPECT_EQ(cali_variant_get_size(v), 0);
    EXPECT_EQ(cali_variant_get_data(&v), nullptr);
}

TEST(C_Variant_Test, CreateIntVariant) {
    int val = -42;

    cali_variant_t v = cali_make_variant_from_int(val);

    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_INT);
    EXPECT_EQ(cali_variant_to_int(v, NULL), val);
    EXPECT_EQ(cali_variant_get_size(v), sizeof(int64_t));

    cali_variant_t vz = cali_make_variant_from_uint(0);

    bool ok = false;
    EXPECT_EQ(cali_variant_to_bool(v, &ok), true);
    EXPECT_TRUE(ok);
    ok = false;
    EXPECT_EQ(cali_variant_to_bool(vz, &ok), false);
    EXPECT_TRUE(ok);
    ok = true;
    EXPECT_EQ(cali_variant_to_type(v, &ok), CALI_TYPE_INV);
    EXPECT_FALSE(ok);

    int64_t v64 = val;

    cali_variant_t v2 = cali_make_variant(CALI_TYPE_INT, &v64, sizeof(int64_t));
    EXPECT_EQ(cali_variant_compare(v, v2), 0);
}

TEST(C_Variant_Test, CreateUintVariant) {
    uint64_t val = 0xFFFFFFFFAA;

    cali_variant_t v = cali_make_variant_from_uint(val);

    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_UINT);
    EXPECT_EQ(cali_variant_get_size(v), sizeof(uint64_t));
    EXPECT_EQ(cali_variant_to_uint(v, NULL), val);

    cali_variant_t v2 = cali_make_variant(CALI_TYPE_UINT, &val, sizeof(uint64_t));
    EXPECT_EQ(cali_variant_compare(v, v2), 0);

    cali_variant_t vz = cali_make_variant_from_uint(0);

    bool ok = false;
    EXPECT_EQ(cali_variant_to_bool(v, &ok), true);
    EXPECT_TRUE(ok);
    ok = false;
    EXPECT_EQ(cali_variant_to_bool(vz, &ok), false);
    EXPECT_TRUE(ok);
    ok = true;
    EXPECT_EQ(cali_variant_to_type(v, &ok), CALI_TYPE_INV);
    EXPECT_FALSE(ok);

    // also test ADDR type here, basically the same as UINT

    void* ptr = &val;
    cali_variant_t v3 = cali_make_variant(CALI_TYPE_ADDR, &ptr, sizeof(void*));
    EXPECT_EQ(cali_variant_get_type(v3), CALI_TYPE_ADDR);
    EXPECT_EQ(cali_variant_get_size(v3), sizeof(uint64_t)); // yes, Caliper ADDR type is always 64bit
    EXPECT_EQ(cali_variant_to_uint(v3, NULL), reinterpret_cast<uint64_t>(ptr));
}

TEST(C_Variant_Test, CreateDoubleVariant) {
    double val = 42.42;

    cali_variant_t v = cali_make_variant_from_double(val);
    bool ok = false;
    
    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_DOUBLE);
    EXPECT_EQ(cali_variant_to_double(v, &ok), val);
    EXPECT_TRUE(ok);
    EXPECT_EQ(cali_variant_get_size(v), sizeof(double));

    ok = false;
    EXPECT_EQ(cali_variant_to_int(v, &ok), 42);
    EXPECT_TRUE(ok);
    ok = false;
    EXPECT_EQ(cali_variant_to_uint(v, &ok), 42);
    EXPECT_TRUE(ok);

    cali_variant_t v2 = cali_make_variant(CALI_TYPE_DOUBLE, &val, sizeof(double));

    EXPECT_TRUE(cali_variant_eq(v, v2));
    EXPECT_EQ(*((double*) cali_variant_get_data(&v2)), val);
}

TEST(C_Variant_Test, CreateStringVariant) {
    const char* mystring = "My test string";

    cali_variant_t v = cali_make_variant_from_string(mystring);

    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_STRING);
    EXPECT_EQ(cali_variant_get_size(v), strlen(mystring));
    EXPECT_EQ(cali_variant_get_data(&v), mystring);
    EXPECT_STREQ(static_cast<const char*>(cali_variant_get_data(&v)), mystring);

    bool ok = true;
    cali_variant_to_int(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    cali_variant_to_uint(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    cali_variant_to_double(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    cali_variant_to_bool(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    EXPECT_EQ(cali_variant_to_type(v, &ok), CALI_TYPE_INV);
    EXPECT_FALSE(ok);
}

TEST(C_Variant_Test, CreateExplicitStringVariant) {
    const char* mystring = "My test string";

    cali_variant_t v = cali_make_variant(CALI_TYPE_STRING, mystring, strlen(mystring)+1);

    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_STRING);
    EXPECT_EQ(cali_variant_get_size(v), strlen(mystring)+1);
    EXPECT_EQ(cali_variant_get_data(&v), mystring);
    EXPECT_STREQ(static_cast<const char*>(cali_variant_get_data(&v)), mystring);

    bool ok = true;
    cali_variant_to_int(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    cali_variant_to_uint(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    cali_variant_to_double(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    cali_variant_to_bool(v, &ok);
    EXPECT_FALSE(ok);
    ok = true;
    EXPECT_EQ(cali_variant_to_type(v, &ok), CALI_TYPE_INV);
    EXPECT_FALSE(ok);
}

TEST(C_Variant_Test, CreateBoolVariant) {
    bool val = true;

    cali_variant_t v = cali_make_variant_from_bool(val);
    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_BOOL);
    EXPECT_EQ(cali_variant_get_size(v), sizeof(bool));
    EXPECT_TRUE(cali_variant_to_bool(v, NULL));
    EXPECT_NE(cali_variant_to_int(v, NULL), 0);

    cali_variant_t v2 = cali_make_variant(CALI_TYPE_BOOL, &val, sizeof(bool));
    EXPECT_EQ(cali_variant_compare(v, v2), 0);
}

TEST(C_Variant_Test, CreateTypeVariant) {
    cali_attr_type val = CALI_TYPE_INT;

    cali_variant_t v = cali_make_variant_from_type(val);
    EXPECT_EQ(cali_variant_get_type(v), CALI_TYPE_TYPE);
    EXPECT_EQ(cali_variant_get_size(v), sizeof(cali_attr_type));
    EXPECT_NE(cali_variant_to_type(v, NULL), 0);

    cali_variant_t v2 = cali_make_variant(CALI_TYPE_TYPE, &val, sizeof(cali_attr_type));
    EXPECT_EQ(cali_variant_compare(v, v2), 0);
}

//
// --- comparison
//

TEST(C_Variant_Test, Compare) {
    cali_variant_t v_int_s = cali_make_variant_from_int(-42);
    cali_variant_t v_int_l = cali_make_variant_from_int(4000);

    EXPECT_LT(cali_variant_compare(v_int_s, v_int_l), 0);
    EXPECT_GT(cali_variant_compare(v_int_l, v_int_s), 0);
    EXPECT_EQ(cali_variant_compare(v_int_s, v_int_s), 0);

    cali_variant_t v_dbl_s = cali_make_variant_from_double(-42);
    cali_variant_t v_dbl_l = cali_make_variant_from_double(4000);

    EXPECT_LT(cali_variant_compare(v_dbl_s, v_dbl_l), 0);
    EXPECT_GT(cali_variant_compare(v_dbl_l, v_dbl_s), 0);
    EXPECT_EQ(cali_variant_compare(v_dbl_s, v_dbl_s), 0);

    const char* str_s = "abcdef";
    const char* str_S = "abcdefg";
    const char* str_l = "bcdefg";

    cali_variant_t v_str_s = cali_make_variant(CALI_TYPE_STRING, str_s, strlen(str_s));
    cali_variant_t v_str_S = cali_make_variant(CALI_TYPE_STRING, str_S, strlen(str_S));
    cali_variant_t v_str_l = cali_make_variant(CALI_TYPE_STRING, str_l, strlen(str_l));

    EXPECT_LT(cali_variant_compare(v_str_s, v_str_S), 0);
    EXPECT_LT(cali_variant_compare(v_str_S, v_str_l), 0);
    EXPECT_GT(cali_variant_compare(v_str_l, v_str_S), 0);
    EXPECT_GT(cali_variant_compare(v_str_S, v_str_s), 0);
    EXPECT_EQ(cali_variant_compare(v_str_s, v_str_s), 0);

    cali_variant_t v_uint_s = cali_make_variant_from_uint(0x01);
    cali_variant_t v_uint_l = cali_make_variant_from_uint(0xFFFFFFFFAA);

    EXPECT_LT(cali_variant_compare(v_uint_s, v_uint_l), 0);
    EXPECT_GT(cali_variant_compare(v_uint_l, v_uint_s), 0);
    EXPECT_EQ(cali_variant_compare(v_uint_l, v_uint_l), 0);

    cali_variant_t v_bool_s = cali_make_variant_from_bool(false);
    cali_variant_t v_bool_l = cali_make_variant_from_bool(true);

    EXPECT_LT(cali_variant_compare(v_bool_s, v_bool_l), 0);
    EXPECT_GT(cali_variant_compare(v_bool_l, v_bool_s), 0);
    EXPECT_EQ(cali_variant_compare(v_bool_l, v_bool_l), 0);

    EXPECT_NE(cali_variant_compare(v_bool_s, v_str_l), 0);
    EXPECT_NE(cali_variant_compare(v_int_s, v_dbl_s), 0);

    cali_variant_t v_inv = cali_make_variant(CALI_TYPE_INV, NULL, 0);
    
    EXPECT_EQ(cali_variant_compare(v_inv, v_inv), 0);
    EXPECT_NE(cali_variant_compare(v_inv, v_uint_l), 0);

    const char* str_ul = "abcd";
    const char* str_us = "abc";

    cali_variant_t v_usr_s = 
        cali_make_variant(CALI_TYPE_USR, (void*) str_us, strlen(str_us));
    cali_variant_t v_usr_l = 
        cali_make_variant(CALI_TYPE_USR, (void*) str_ul, strlen(str_ul));

    EXPECT_EQ(cali_variant_compare(v_usr_l, v_usr_l), 0);
    EXPECT_NE(cali_variant_compare(v_usr_s, v_usr_l), 0);
}

//
// --- Pack/Unpack
//

TEST(C_Variant_Test, PackUnpack) {
    int            val_1_int  = -27;
    uint64_t       val_2_uint = 0xFFFFFFFFAA;
    const char*    val_3_str  = "My wonderful test string";
    double         val_4_dbl  = 42.42;
    const void*    val_5_inv  = NULL;
    cali_attr_type val_6_type = CALI_TYPE_ADDR;
    bool           val_7_bool = true;

    cali_variant_t v_1_int_in  = cali_make_variant_from_int(val_1_int);
    cali_variant_t v_2_uint_in = cali_make_variant_from_uint(val_2_uint);
    cali_variant_t v_3_str_in  = cali_make_variant(CALI_TYPE_STRING, val_3_str, strlen(val_3_str)+1);
    cali_variant_t v_4_dbl_in  = cali_make_variant_from_double(val_4_dbl);
    cali_variant_t v_5_inv_in  = cali_make_variant(CALI_TYPE_INV, val_5_inv, 0);
    cali_variant_t v_6_type_in = cali_make_variant_from_type(val_6_type);
    cali_variant_t v_7_bool_in = cali_make_variant_from_bool(val_7_bool);

    unsigned char buf[144]; // must be >= 7*20 = 140 bytes
    size_t pos = 0;

    memset(buf, 0xFA, 144);

    pos += cali_variant_pack(v_1_int_in,  buf+pos);
    pos += cali_variant_pack(v_2_uint_in, buf+pos);
    pos += cali_variant_pack(v_3_str_in,  buf+pos);
    pos += cali_variant_pack(v_4_dbl_in,  buf+pos);
    pos += cali_variant_pack(v_5_inv_in,  buf+pos);
    pos += cali_variant_pack(v_6_type_in, buf+pos);
    pos += cali_variant_pack(v_7_bool_in, buf+pos);

    EXPECT_LE(pos, 140);

    bool ok = false;
    pos = 0;

    cali_variant_t v_1_int_out  = cali_variant_unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_1 unpack (int)");
    cali_variant_t v_2_uint_out = cali_variant_unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_2 unpack (uint)");
    cali_variant_t v_3_str_out  = cali_variant_unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_3 unpack (str)");
    cali_variant_t v_4_dbl_out  = cali_variant_unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_4 unpack (dbl)");
    cali_variant_t v_5_inv_out  = cali_variant_unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_5 unpack (inv)");
    cali_variant_t v_6_type_out = cali_variant_unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_6 unpack (type)");
    cali_variant_t v_7_bool_out = cali_variant_unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_7 unpack (bool)");

    EXPECT_FALSE(cali_variant_is_empty(v_1_int_out));
    EXPECT_EQ(cali_variant_get_type(v_1_int_out), CALI_TYPE_INT);
    EXPECT_EQ(cali_variant_to_int(v_1_int_out, NULL), val_1_int);
    EXPECT_TRUE(cali_variant_eq(v_1_int_in, v_1_int_out));

    EXPECT_FALSE(cali_variant_is_empty(v_2_uint_out));
    EXPECT_EQ(cali_variant_get_type(v_2_uint_out), CALI_TYPE_UINT);
    EXPECT_EQ(cali_variant_to_uint(v_2_uint_out, NULL), val_2_uint);
    EXPECT_TRUE(cali_variant_eq(v_2_uint_in, v_2_uint_out));

    EXPECT_FALSE(cali_variant_is_empty(v_3_str_out));
    EXPECT_EQ(cali_variant_get_type(v_3_str_out), CALI_TYPE_STRING);
    EXPECT_EQ(cali_variant_get_size(v_3_str_out), strlen(val_3_str)+1);
    EXPECT_STREQ(static_cast<const char*>(cali_variant_get_data(&v_3_str_out)), val_3_str);
    EXPECT_TRUE(cali_variant_eq(v_3_str_in, v_3_str_out));

    EXPECT_FALSE(cali_variant_is_empty(v_4_dbl_out));
    EXPECT_EQ(cali_variant_get_type(v_4_dbl_out), CALI_TYPE_DOUBLE);
    EXPECT_EQ(cali_variant_to_double(v_4_dbl_out, NULL), val_4_dbl);
    EXPECT_TRUE(cali_variant_eq(v_4_dbl_in, v_4_dbl_out));

    EXPECT_TRUE(cali_variant_is_empty(v_5_inv_out));
    EXPECT_EQ(cali_variant_get_type(v_5_inv_out), CALI_TYPE_INV);
    EXPECT_TRUE(cali_variant_eq(v_5_inv_in, v_5_inv_out));
    
    EXPECT_FALSE(cali_variant_is_empty(v_6_type_out));
    EXPECT_EQ(cali_variant_get_type(v_6_type_out), CALI_TYPE_TYPE);
    EXPECT_EQ(cali_variant_to_type(v_6_type_out, NULL), val_6_type);
    EXPECT_TRUE(cali_variant_eq(v_6_type_in, v_6_type_out));

    EXPECT_FALSE(cali_variant_is_empty(v_7_bool_out));
    EXPECT_EQ(cali_variant_get_type(v_7_bool_out), CALI_TYPE_BOOL);
    EXPECT_TRUE(cali_variant_to_bool(v_7_bool_out, NULL));
    EXPECT_EQ(cali_variant_to_uint(v_7_bool_in, NULL), cali_variant_to_uint(v_7_bool_out, NULL));
    EXPECT_TRUE(cali_variant_eq(v_7_bool_in, v_7_bool_out));
}
