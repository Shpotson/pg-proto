--   name                = "Alice"
--   id                  = 42
--   email               = "alice@example.com"
--   phone.number        = "+71234567890"
--   balance             = -9876543210     (sint64, zigzag)
--   height_m            = 1.8288
--   weight_kg           = 72.5
--   is_active           = true
--   address.street      = "Tverskaya 1"
--   address.city        = "Moscow"
--   address.zip         = 125009          (uint32)
--   address.building_id = 12345678901
--   address.latitude    = 55.7558
--   address.longitude   = 37.6173
--   address.is_primary  = true
--   score               = -150            (sint32, zigzag)

-- 1) depth 1
SELECT get_double_by_scheme(
               $$
package tutorial;

message Person {
    string name = 1;
    int32 id = 2;
    string email = 3;
    PhoneNumber phone = 4;
    sint64 balance = 5;
    double height_m = 6;
    float weight_kg = 7;
    bool is_active = 8;
    Address address = 9;
    sint32 score = 10;
}

message PhoneNumber {
    string number = 1;
}

message Address {
    string street = 1;
    string city = 2;
    uint32 zip = 3;
    int64 building_id = 4;
    double latitude = 5;
    float longitude = 6;
    bool is_primary = 7;
}
$$,
    'Person',
    '%.height_m',
    '\x0a05416c696365102a1a11616c696365406578616d706c652e636f6d220e0a0c2b373132333435363738393028d3db80cb4931fbcbeec9c342fd3f3d0000914240014a2f0a0b54766572736b617961203112064d6f73636f7718d1d00720b5b8f0fe2d298d28ed0dbee04b40351d781642380150ab02'::bytea
);

-- 2) depth 2
SELECT get_double_by_scheme(
               $$
package tutorial;

message Person {
    string name = 1;
    int32 id = 2;
    string email = 3;
    PhoneNumber phone = 4;
    sint64 balance = 5;
    double height_m = 6;
    float weight_kg = 7;
    bool is_active = 8;
    Address address = 9;
    sint32 score = 10;
}

message PhoneNumber {
    string number = 1;
}

message Address {
    string street = 1;
    string city = 2;
    uint32 zip = 3;
    int64 building_id = 4;
    double latitude = 5;
    float longitude = 6;
    bool is_primary = 7;
}
$$,
    'Person',
    '%.address.latitude',
    '\x0a05416c696365102a1a11616c696365406578616d706c652e636f6d220e0a0c2b373132333435363738393028d3db80cb4931fbcbeec9c342fd3f3d0000914240014a2f0a0b54766572736b617961203112064d6f73636f7718d1d00720b5b8f0fe2d298d28ed0dbee04b40351d781642380150ab02'::bytea
);