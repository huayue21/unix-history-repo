#define YYEMPTY (-1)
#define L_EOS 257
#define L_IPADDR 258
#define L_NUMBER 259
#define L_STRING 260
#define L_QSTRING 261
#define L_END_INCLUDE 262
#define T_INCLUDE 263
#define T_OPTIONS 264
#define T_DIRECTORY 265
#define T_PIDFILE 266
#define T_NAMED_XFER 267
#define T_DUMP_FILE 268
#define T_STATS_FILE 269
#define T_MEMSTATS_FILE 270
#define T_FAKE_IQUERY 271
#define T_RECURSION 272
#define T_FETCH_GLUE 273
#define T_QUERY_SOURCE 274
#define T_LISTEN_ON 275
#define T_PORT 276
#define T_ADDRESS 277
#define T_DATASIZE 278
#define T_STACKSIZE 279
#define T_CORESIZE 280
#define T_DEFAULT 281
#define T_UNLIMITED 282
#define T_FILES 283
#define T_HOSTSTATS 284
#define T_DEALLOC_ON_EXIT 285
#define T_TRANSFERS_IN 286
#define T_TRANSFERS_OUT 287
#define T_TRANSFERS_PER_NS 288
#define T_TRANSFER_FORMAT 289
#define T_MAX_TRANSFER_TIME_IN 290
#define T_ONE_ANSWER 291
#define T_MANY_ANSWERS 292
#define T_NOTIFY 293
#define T_AUTH_NXDOMAIN 294
#define T_MULTIPLE_CNAMES 295
#define T_CLEAN_INTERVAL 296
#define T_INTERFACE_INTERVAL 297
#define T_STATS_INTERVAL 298
#define T_LOGGING 299
#define T_CATEGORY 300
#define T_CHANNEL 301
#define T_SEVERITY 302
#define T_DYNAMIC 303
#define T_FILE 304
#define T_VERSIONS 305
#define T_SIZE 306
#define T_SYSLOG 307
#define T_DEBUG 308
#define T_NULL_OUTPUT 309
#define T_PRINT_TIME 310
#define T_PRINT_CATEGORY 311
#define T_PRINT_SEVERITY 312
#define T_TOPOLOGY 313
#define T_SERVER 314
#define T_LONG_AXFR 315
#define T_BOGUS 316
#define T_TRANSFERS 317
#define T_KEYS 318
#define T_ZONE 319
#define T_IN 320
#define T_CHAOS 321
#define T_HESIOD 322
#define T_TYPE 323
#define T_MASTER 324
#define T_SLAVE 325
#define T_STUB 326
#define T_RESPONSE 327
#define T_HINT 328
#define T_MASTERS 329
#define T_TRANSFER_SOURCE 330
#define T_ALSO_NOTIFY 331
#define T_ACL 332
#define T_ALLOW_UPDATE 333
#define T_ALLOW_QUERY 334
#define T_ALLOW_TRANSFER 335
#define T_SEC_KEY 336
#define T_ALGID 337
#define T_SECRET 338
#define T_CHECK_NAMES 339
#define T_WARN 340
#define T_FAIL 341
#define T_IGNORE 342
#define T_FORWARD 343
#define T_FORWARDERS 344
#define T_ONLY 345
#define T_FIRST 346
#define T_IF_NO_ANSWER 347
#define T_IF_NO_DOMAIN 348
#define T_YES 349
#define T_TRUE 350
#define T_NO 351
#define T_FALSE 352
typedef union {
	char *			cp;
	int			s_int;
	long			num;
	u_long			ul_int;
	u_int16_t		us_int;
	struct in_addr		ip_addr;
	ip_match_element	ime;
	ip_match_list		iml;
	key_info		keyi;
	enum axfr_format	axfr_fmt;
} YYSTYPE;
extern YYSTYPE yylval;
