
#define db_verSet	0x65
#define db_ver_WR_VERS	0x1
#define db_ver_NO_VERS	0x2
#define db_ver_BAD_VERS	0x3

#define db_storeSet	0x66
#define db_store_INS_UNUSED	0x1

#define db_deleteSet	0x67
#define db_delete_NO_MULTI	0x1

#define db_lookupSet	0x68
#define db_lookup_LOCK_DB	0x1
#define db_lookup_CORRUPT	0x2
#define db_lookup_REPLACE	0x3
#define db_lookup_FEW_FIELDS	0x4
#define db_lookup_BAD_MULTI	0x5
