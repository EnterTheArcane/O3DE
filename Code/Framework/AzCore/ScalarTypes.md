# Scalar type migration

Include `AzCore/base.h` for the scalar aliases. Use `i8`, `i16`, `i32`, and `i64` for new signed exact-width declarations. The corresponding unsigned names are `u8`, `u16`, `u32`, and `u64`. The old AZ aliases remain available during the transition.

On targets where the standard 64-bit aliases use `long` and `unsigned long`, the cutover changes the C++ ABI of interfaces using `AZ::s64` or `AZ::u64`. Rebuild the engine and all native projects and Gems together. Update native overrides and external API boundaries to match the declared types.

On these targets, change persisted fields declared as native `long long` or `unsigned long long` to the matching fixed-width alias so the fields keep their existing serialized UUIDs.

The reflected and serialized `i64` and `u64` types continue to use the old AZ 64-bit UUIDs. Existing data labeled `AZ::s64` or `AZ::u64` remains readable.

Before upgrading a Linux or Android project that persisted fields with native `long` or `unsigned long` UUIDs, use the previous engine version to read the data and write those fields as `AZ::s64` or `AZ::u64`. For reflected classes, a version converter in the previous engine can read the original native fields and write the fixed-width fields. Verify the converted values by loading the resaved assets before upgrading.

[RFC #19996](https://github.com/o3de/o3de/issues/19996)
