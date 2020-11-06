#include "caliper/common/Variant.h"

#include "gtest/gtest.h"

#include <cstring>

using namespace cali;

// Test some type conversions

TEST(Variant_Test, FromString) {
    const char* teststr = "My wonderful test string";
    uint64_t uval = 0xef10;
    int64_t i64val = -9876543210;

    const struct testcase_t {
        cali_attr_type type;
        const char*    str;
        bool           ok;
        Variant        expected;
    } testcases[] = {
        { CALI_TYPE_INV,    "42",    false, Variant() },

        { CALI_TYPE_INT,    "42",    true,  Variant(static_cast<int>(42))  },
        { CALI_TYPE_INT,    " -10 ", true,  Variant(static_cast<int>(-10)) },
        { CALI_TYPE_INT,    "bla",   false, Variant() },

        { CALI_TYPE_INT,    "-9876543210", true, Variant(cali_make_variant_from_int64(i64val)) },

        { CALI_TYPE_STRING, teststr, true,  Variant(CALI_TYPE_STRING, teststr, strlen(teststr)) },
        { CALI_TYPE_STRING, teststr, true,  Variant(teststr) },
        { CALI_TYPE_STRING, "",      true,  Variant(CALI_TYPE_STRING, "", 0)  },

        { CALI_TYPE_UINT,   "0",     true,  Variant(static_cast<uint64_t>(0)) },
        { CALI_TYPE_UINT,   "1337",  true,  Variant(static_cast<uint64_t>(1337)) },
        { CALI_TYPE_ADDR,   "ef10",  true,  Variant(CALI_TYPE_ADDR, &uval, sizeof(uval))   },

        { CALI_TYPE_DOUBLE, "-1.0",  true,  Variant(static_cast<double>(-1.0)) },

        { CALI_TYPE_BOOL,   "false", true,  Variant(false) },
        { CALI_TYPE_BOOL,   "1",     true,  Variant(true)  },
        { CALI_TYPE_BOOL,   "bla",   false, Variant()      },

        { CALI_TYPE_TYPE,   "int",   true,  Variant(CALI_TYPE_INT) },
        { CALI_TYPE_TYPE,   "bla",   false, Variant()      },

        { CALI_TYPE_PTR,    "0",     false, Variant()      },

        { CALI_TYPE_INV, 0, false, Variant() }
    };

    for (const testcase_t* t = testcases; t->str; ++t) {
        bool    ok = false;
        Variant v(Variant::from_string(t->type, t->str, &ok));

        EXPECT_EQ(ok, t->ok) << "for \"" << t->str << "\" (" << cali_type2string(t->type) << ")";
        EXPECT_EQ(v, t->expected) << "for \"" << t->str << "\" (" << cali_type2string(t->type) << ")";
    }
}

TEST(Variant_Test, UintOverloads) {
    EXPECT_EQ( Variant( static_cast<std::size_t   >(42)  ).type(), CALI_TYPE_UINT   );
    EXPECT_EQ( Variant( static_cast<unsigned      >(42)  ).type(), CALI_TYPE_UINT   );
    EXPECT_EQ( Variant( static_cast<unsigned char >(42)  ).type(), CALI_TYPE_UINT   );
    EXPECT_EQ( Variant( static_cast<uint64_t      >(42)  ).type(), CALI_TYPE_UINT   );
    EXPECT_EQ( Variant( static_cast<uint8_t       >(42)  ).type(), CALI_TYPE_UINT   );
    EXPECT_EQ( Variant( static_cast<int32_t       >(42)  ).type(), CALI_TYPE_INT    );
    EXPECT_EQ( Variant( static_cast<int8_t        >(42)  ).type(), CALI_TYPE_INT    );
    EXPECT_EQ( Variant( static_cast<float         >(4.2) ).type(), CALI_TYPE_DOUBLE );
    EXPECT_EQ( Variant( static_cast<bool          >(0)   ).type(), CALI_TYPE_BOOL   );
    EXPECT_EQ( Variant( CALI_TYPE_STRING                 ).type(), CALI_TYPE_TYPE   );
}

TEST(Variant_Test, Conversions) {
    int      val_1_int_neg  = -27;
    int64_t  val_2_i64_nlrg = -9876543210;
    uint64_t val_3_uint_sml = 42;
    uint64_t val_4_uint_lrg = 0xFFFFFFFFFFAA;
    int      val_5_zero = 0;

    cali::Variant v_1_int_neg(val_1_int_neg);
    cali::Variant v_2_i64_nlrg(cali_make_variant_from_int64(val_2_i64_nlrg));
    cali::Variant v_3_uint_sml(val_3_uint_sml);
    cali::Variant v_4_uint_lrg(val_4_uint_lrg);
    cali::Variant v_5_int_zero(val_5_zero);

    EXPECT_EQ(v_1_int_neg.type(),  CALI_TYPE_INT);
    EXPECT_EQ(v_2_i64_nlrg.type(), CALI_TYPE_INT);
    EXPECT_EQ(v_3_uint_sml.type(), CALI_TYPE_UINT);
    EXPECT_EQ(v_4_uint_lrg.type(), CALI_TYPE_UINT);
    EXPECT_EQ(v_5_int_zero.type(), CALI_TYPE_INT);

    {
        bool v_1_int_neg_to_int = false;
        int val_int = v_1_int_neg.to_int(&v_1_int_neg_to_int);
        EXPECT_TRUE(v_1_int_neg_to_int);
        EXPECT_EQ(val_int, val_1_int_neg);
    }
    {
        bool v_1_int_neg_to_i64 = false;        
        int64_t val_i64 = v_1_int_neg.to_int64(&v_1_int_neg_to_i64);
        EXPECT_TRUE(v_1_int_neg_to_i64);
        EXPECT_EQ(val_i64, static_cast<int64_t>(val_1_int_neg));
    }
    {
        bool v_1_int_neg_to_uint = true;
        v_1_int_neg.to_uint(&v_1_int_neg_to_uint);
        EXPECT_FALSE(v_1_int_neg_to_uint);
    }

    {
        bool v_2_i64_nlrg_to_int = true;
        v_2_i64_nlrg.to_int(&v_2_i64_nlrg_to_int);
        EXPECT_FALSE(v_2_i64_nlrg_to_int);
    }
    {
        bool v_2_i64_nlrg_to_i64 = false;
        int64_t val_i64 = v_2_i64_nlrg.to_int64(&v_2_i64_nlrg_to_i64);
        EXPECT_TRUE(v_2_i64_nlrg_to_i64);
        EXPECT_EQ(val_i64, val_2_i64_nlrg);
    }
    {
        bool v_2_i64_nlrg_to_uint = true;
        v_2_i64_nlrg.to_uint(&v_2_i64_nlrg_to_uint);
        EXPECT_FALSE(v_2_i64_nlrg_to_uint);
    }    

    {
        bool v_3_uint_sml_to_int = false;
        int val_int = v_3_uint_sml.to_int(&v_3_uint_sml_to_int);
        EXPECT_TRUE(v_3_uint_sml_to_int);
        EXPECT_EQ(val_int, static_cast<int>(val_3_uint_sml));
    }
    {
        bool v_3_uint_sml_to_i64 = false;
        int64_t val_i64 = v_3_uint_sml.to_int64(&v_3_uint_sml_to_i64);
        EXPECT_TRUE(v_3_uint_sml_to_i64);
        EXPECT_EQ(val_i64, static_cast<int64_t>(val_3_uint_sml));
    }
    {
        bool v_3_uint_sml_to_uint = false;
        uint64_t val_uint = v_3_uint_sml.to_uint(&v_3_uint_sml_to_uint);
        EXPECT_TRUE(v_3_uint_sml_to_uint);
        EXPECT_EQ(val_uint, val_3_uint_sml);
    }
    {
        bool v_3_uint_sml_to_bool = false;
        bool val_bool = v_3_uint_sml.to_bool(&v_3_uint_sml_to_bool);
        EXPECT_TRUE(v_3_uint_sml_to_bool);
        EXPECT_TRUE(val_bool);
    }

    {
        bool v_4_uint_lrg_to_int = true;
        v_4_uint_lrg.to_int(&v_4_uint_lrg_to_int);
        EXPECT_FALSE(v_4_uint_lrg_to_int);
    }
    {
        bool v_4_uint_lrg_to_uint = false;
        uint64_t val_uint = v_4_uint_lrg.to_uint(&v_4_uint_lrg_to_uint);
        EXPECT_TRUE(v_4_uint_lrg_to_uint);
        EXPECT_EQ(val_uint, val_4_uint_lrg);
    }

    {
        bool v_5_zero_to_int = false;
        int val_int = v_5_int_zero.to_int(&v_5_zero_to_int);
        EXPECT_TRUE(v_5_zero_to_int);
        EXPECT_EQ(val_int, val_5_zero);
    }
    {
        bool v_5_zero_to_i64 = false;
        int64_t val_i64 = v_5_int_zero.to_int64(&v_5_zero_to_i64);
        EXPECT_TRUE(v_5_zero_to_i64);
        EXPECT_EQ(val_i64, static_cast<int64_t>(val_5_zero));
    }
    {
        bool v_5_zero_to_uint = false;
        uint64_t val_uint = v_5_int_zero.to_uint(&v_5_zero_to_uint);
        EXPECT_TRUE(v_5_zero_to_uint);
        EXPECT_EQ(val_uint, static_cast<uint64_t>(val_5_zero));
    }
    {
        bool v_5_zero_to_bool = false;
        bool val_bool = v_5_int_zero.to_bool(&v_5_zero_to_bool);
        EXPECT_TRUE(v_5_zero_to_bool);
        EXPECT_FALSE(val_bool);
    }

}

// --- Test Variant pack/unpack

TEST(Variant_Test, PackUnpack) {
    int            val_1_int  = -27;
    uint64_t       val_2_uint = 0xFFFFFFFFAA;
    const char*    val_3_str  = "My wonderful test string";
    double         val_4_dbl  = 42.42;
    // const void*    val_5_inv  = NULL;
    cali_attr_type val_6_type = CALI_TYPE_ADDR;
    bool           val_7_bool = true;
    void*          val_8_ptr  = this;
    int64_t        val_9_i64  = -9876543210;

    cali::Variant  v_1_int_in(val_1_int);
    cali::Variant  v_2_uint_in(CALI_TYPE_UINT, &val_2_uint, sizeof(uint64_t));
    cali::Variant  v_3_str_in(CALI_TYPE_STRING, val_3_str, strlen(val_3_str)+1);
    cali::Variant  v_4_dbl_in(val_4_dbl);
    cali::Variant  v_5_inv_in;
    cali::Variant  v_6_type_in(val_6_type);
    cali::Variant  v_7_bool_in(val_7_bool);
    cali::Variant  v_8_ptr_in(cali_make_variant_from_ptr(val_8_ptr));
    cali::Variant  v_9_i64_in(CALI_TYPE_INT, &val_9_i64, sizeof(int64_t));

    unsigned char buf[256]; // must be >= 9*22 = 198 bytes
    size_t pos = 0;

    memset(buf, 0, 256);

    pos += v_1_int_in.pack(buf+pos);
    pos += v_2_uint_in.pack(buf+pos);
    pos += v_3_str_in.pack(buf+pos);
    pos += v_4_dbl_in.pack(buf+pos);
    pos += v_5_inv_in.pack(buf+pos);
    pos += v_6_type_in.pack(buf+pos);
    pos += v_7_bool_in.pack(buf+pos);
    pos += v_8_ptr_in.pack(buf+pos);
    pos += v_9_i64_in.pack(buf+pos);

    EXPECT_LE(pos, 198);

    bool ok = false;
    pos = 0;

    Variant v_1_int_out = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_1 unpack (int)");
    Variant v_2_uint_out = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_2 unpack (uint)");
    Variant v_3_str_out = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_3 unpack (string)");
    Variant v_4_dbl_out = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_4 unpack (double)");
    Variant v_5_inv_out = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_5 unpack (inv)");
    Variant v_6_type_out = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_6 unpack (type)");
    Variant v_7_bool_out = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_7 unpack (bool)");
    Variant v_8_ptr_out  = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_8 unpack (ptr)");
    Variant v_9_i64_out  = Variant::unpack(buf+pos, &pos, &ok);
    EXPECT_TRUE(ok && "v_9 unpack (int64)");

    EXPECT_EQ(v_1_int_out.type(), CALI_TYPE_INT);
    EXPECT_EQ(v_1_int_out.to_int(), val_1_int);
    EXPECT_EQ(v_1_int_in, v_1_int_out);

    EXPECT_EQ(v_2_uint_out.type(), CALI_TYPE_UINT);
    EXPECT_EQ(v_2_uint_out.to_uint(), val_2_uint);
    EXPECT_EQ(v_2_uint_in, v_2_uint_out);

    EXPECT_EQ(v_3_str_out.type(), CALI_TYPE_STRING);
    EXPECT_EQ(v_3_str_out.size(), strlen(val_3_str)+1);
    EXPECT_EQ(v_3_str_out.data(), static_cast<const void*>(val_3_str));
    EXPECT_EQ(v_3_str_out.to_string(), std::string(val_3_str));
    EXPECT_EQ(v_3_str_in, v_3_str_out);

    EXPECT_EQ(v_4_dbl_out.type(), CALI_TYPE_DOUBLE);
    EXPECT_EQ(v_4_dbl_out.to_double(), val_4_dbl);
    EXPECT_EQ(v_4_dbl_in, v_4_dbl_out);

    EXPECT_EQ(v_5_inv_out.type(), CALI_TYPE_INV);
    EXPECT_TRUE(v_5_inv_out.empty());
    EXPECT_EQ(v_5_inv_in, v_5_inv_out);

    EXPECT_EQ(v_6_type_out.type(), CALI_TYPE_TYPE);
    EXPECT_EQ(v_6_type_out.to_attr_type(), val_6_type);
    EXPECT_EQ(v_6_type_in, v_6_type_out);

    EXPECT_EQ(v_7_bool_out.type(), CALI_TYPE_BOOL);
    EXPECT_EQ(v_7_bool_out.to_bool(), val_7_bool);
    EXPECT_EQ(v_7_bool_in, v_7_bool_out);

    EXPECT_EQ(v_8_ptr_out.type(), CALI_TYPE_PTR);
    EXPECT_EQ(v_8_ptr_out.get_ptr(), val_8_ptr);
    EXPECT_EQ(v_8_ptr_in, v_8_ptr_out);

    EXPECT_EQ(v_9_i64_out.type(), CALI_TYPE_INT);
    EXPECT_EQ(v_9_i64_out.to_int64(), val_9_i64);
    EXPECT_EQ(v_9_i64_in, v_9_i64_out);
}
