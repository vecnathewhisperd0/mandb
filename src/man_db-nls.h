
#define manpSet	0x1
#define manp_PARSE_CONF	0x1
#define manp_STAT	0x2
#define manp_NOT_DIR	0x3
#define manp_MANPATH	0x4
#define manp_NO_PATH	0x5
#define manp_MT_PATH	0x6
#define manp_MKDIR_CAT1	0x7
#define manp_MKDIR_CAT2	0x8
#define manp_OPEN_CONF	0x9
#define manp_PARSE_DIRS	0xa
#define manp_MISS_DIR	0xb
#define manp_CWD	0xc
#define manp_NO_TREE	0xd
#define manp_PREFIX	0xe

#define whatisSet	0x2
#define whatis_APROPOS_USAGE	0x1
#define whatis_APROPOS_OPTIONS	0x2
#define whatis_WHATIS_USAGE	0x3
#define whatis_WHATIS_OPTIONS	0x4
#define whatis_WHATIS	0x5
#define whatis_UNKNOWN	0x6
#define whatis_NOTHING_APPROPRIATE	0x7
#define whatis_WHAT_WHAT	0x8
#define whatis_FAILED_REGEX	0x9

#define securitySet	0x3
#define security_EUID	0x1
#define security_REMOVE	0x2
#define security_FORK	0x3

#define u_sSet	0x4
#define u_s_OPENDIR	0x1
#define u_s_DANGLE	0x2
#define u_s_RESOLVE	0x3
#define u_s_SELF_REF	0x4
#define u_s_OPEN	0x5

#define catmanSet	0x5
#define catman_USAGE	0x1
#define catman_OPTIONS	0x2
#define catman_RD_DB	0x3
#define catman_FORK_FAILED	0x4
#define catman_WAIT4MAN	0x5
#define catman_MAN_FAILED	0x6
#define catman_RESET_CURSOR	0x7
#define catman_NULL_CONTENT	0x8
#define catman_UPDATE_DB_MSG	0x9
#define catman_NO_WRITE	0xa
#define catman_NO_DB	0xb

#define straySet	0x6
#define stray_OPENDIR	0x1
#define stray_BOGUS	0x2
#define strayANGLE	0x3
#define strayESOLVE	0x4
#define stray_NO_WHATIS	0x5
#define stray_CHECK	0x6
#define stray_UPDATE_DB	0x7

#define lexgrogSet	0x7
#define lexgrog_TOOBIG	0x1
#define lexgrog_OPEN	0x2

#define manpathSet	0x8
#define manpath_USAGE	0x1
#define manpath_OPT	0x2
#define manpath_NO_GLOBAL	0x3

#define compressionSet	0x9
#define compression_TEMPNAM	0x1

#define versionSet	0xa
#define version_STRING	0x1
#define version_STRING2	0x2

#define convnamSet	0xb
#define convnam_FAIL	0x1

#define c_mSet	0xc
#define c_m_BOGUS	0x1
#define c_m_MULT_EXT	0x2
#define c_m_BAD_INCL	0x3
#define c_m_MT_MAN	0x4
#define c_m_NO_WHATIS	0x5
#define c_m_OPENDIR	0x6
#define c_m_UPDATE_DB	0x7
#define c_m_UPDATE_DB_MSG	0x8
#define c_m_CREATE_DB	0x9
#define c_m_DONE	0xa

#define manSet	0xd
#define man_POPEN	0x1
#define man_SYSTEM	0x2
#define man_USAGE_TROFF	0x3
#define man_USAGE_NOTROFF	0x4
#define man_OPT1	0x5
#define man_OPT2	0x6
#define man_OPTDIT	0x7
#define man_OPT3	0x8
#define man_OPT4	0x9
#define man_NO_NAME_S	0xa
#define man_NO_NAME	0xb
#define man_NO_SRC_MAN	0xc
#define man_NO_MAN	0xd
#define man_SEC	0xe
#define man_LESS_PROMPT	0xf
#define man_INCOMP_OPT	0x10
#define man_BAD_PP	0x11
#define man_CHMOD	0x12
#define man_RENAME	0x13
#define man_UNLINK	0x14
#define man_PIPE	0x15
#define man_FORK	0x16
#define man_DUP2	0x17
#define man_CREATE_CAT	0x18
#define man_EXEC	0x19
#define man_WAITPID	0x1a
#define man_STILL_SAVING	0x1b
#define man_CD	0x1c
#define man_NO_CAT	0x1d
#define man_REFORMAT	0x1e
#define man_UPDATE_DB	0x1f
#define man_NEXT	0x20

#define mandbSet	0xe
#define mandb_USAGE	0x1
#define mandb_OPTIONS	0x2
#define mandb_REMOVE	0x3
#define mandb_RENAME	0x4
#define mandb_CHMOD	0x5
#define mandb_CHOWN	0x6
#define mandb_PROCESS	0x7
#define mandb_NO_USER	0x8
#define mandb_NO_DIRECTIVES	0x9
#define mandb_MANS	0xa
#define mandb_STRAYS	0xb
#define mandb_ADDED	0xc
