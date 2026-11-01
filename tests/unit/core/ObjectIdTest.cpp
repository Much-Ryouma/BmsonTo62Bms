#include <gtest/gtest.h>

#include "b2b/core/bms/ObjectId.hpp"

using namespace b2b::core;

TEST(ObjectId, OID001_Base36WritesUppercaseDigits) {
    EXPECT_EQ(formatObjectId(1, IdBase::Base36), "01");
    EXPECT_EQ(formatObjectId(35, IdBase::Base36), "0Z");
    EXPECT_EQ(formatObjectId(36, IdBase::Base36), "10");
    EXPECT_EQ(formatObjectId(1295, IdBase::Base36), "ZZ");
}

TEST(ObjectId, OID002_Base62WritesLowercaseAfterUppercase) {
    EXPECT_EQ(formatObjectId(36, IdBase::Base62), "0a");
    EXPECT_EQ(formatObjectId(61, IdBase::Base62), "0z");
    EXPECT_EQ(formatObjectId(62, IdBase::Base62), "10");
    EXPECT_EQ(formatObjectId(3843, IdBase::Base62), "zz");
}

TEST(ObjectId, OID003_OutOfRangeValuesAreRejected) {
    EXPECT_THROW(static_cast<void>(formatObjectId(0, IdBase::Base36)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(formatObjectId(1296, IdBase::Base36)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(formatObjectId(3844, IdBase::Base62)), std::out_of_range);
}

TEST(ObjectId, OID004_Base62DistinguishesCase) {
    EXPECT_EQ(parseObjectId("Aa", IdBase::Base62), 656);
    EXPECT_EQ(parseObjectId("aA", IdBase::Base62), 2242);
}

TEST(ObjectId, OID005_Base36ReadsLowercaseAsUppercase) {
    EXPECT_EQ(parseObjectId("zz", IdBase::Base36), 1295);
    EXPECT_EQ(parseObjectId("aA", IdBase::Base36), 370);
}

TEST(ObjectId, OID006_MalformedTextIsNotAnId) {
    EXPECT_FALSE(parseObjectId("0", IdBase::Base36).has_value());
    EXPECT_FALSE(parseObjectId("001", IdBase::Base36).has_value());
    EXPECT_FALSE(parseObjectId("0!", IdBase::Base62).has_value());
}

TEST(ObjectId, OID007_EveryValueRoundTrips) {
    for (const IdBase base : {IdBase::Base36, IdBase::Base62}) {
        for (int value = 1; value <= maxObjectId(base); ++value) {
            ASSERT_EQ(parseObjectId(formatObjectId(value, base), base), value);
        }
    }
}
