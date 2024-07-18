#ifndef _CJSON_DIRECT_H_
#define _CJSON_DIRECT_H_


typedef enum {
	KEY_TYPE_NULL,
	KEY_TYPE_S8,        // 有符号8位整型
	KEY_TYPE_S16,       // 有符号16位整型
	KEY_TYPE_S32,       // 有符号32位整型
	KEY_TYPE_S64,       // 有符号64位整型
	KEY_TYPE_U8,        // 无符号8位整型
	KEY_TYPE_U16,       // 无符号16位整型
	KEY_TYPE_U32,       // 无符号32位整型
	KEY_TYPE_U64,       // 无符号64位整型
	KEY_TYPE_FLOAT,
	KEY_TYPE_DOUBLE,
	KEY_TYPE_STRING,
	KEY_TYPE_OBJECT,
	KEY_TYPE_ARRAY,
	KEY_TYPE_MAX
}key_type_e;


typedef struct key_info_s{
	int csize;					///< 本结构体大小
	key_type_e type;				///< 成员类型
	char *key;					///< 成员名称
	int offset;					///< 成员偏移地址
	int ksize;						///< 成员大小
	struct key_info_s *sub_key;	///< 对于#KEY_TYPE_OBJECT类型，其具体类型定义
	int arraycnt;					///< 对于#KEY_TYPE_ARRAY类型，其个数
	key_type_e arraytype;		///< 对于#KEY_TYPE_ARRAY类型，其成员的类型
}key_info_t;


/*成员在结构体中的偏移地址*/
#define NAME_OFFSET(type, name) (offsetof(type, name))
#define NAME_SIZE(type,name) (sizeof((((type *)0)->name)))


/**
 *@brief 形成结构体
 *@param ctype 结构体类型
 *@param ktype 成员类型
 *@param kname 成员名
 *@param subkey 如果keytype为#KEY_TYPE_OBJECT，则为其对应结构体的#key_info_t 指针
 *@param arraycnt 对于#KEY_TYPE_ARRAY类型，其个数
 *@param arraytype 对于#KEY_TYPE_ARRAY类型，其成员的类型
 *
 */
#define MAKE_ARRAY_INFO(ctype, ktype, kname, subkey, arraycnt, arraytype) {sizeof(ctype), ktype, #kname, NAME_OFFSET(ctype,kname), NAME_SIZE(ctype,kname), subkey, arraycnt, arraytype}

#define MAKE_KEY_INFO(ctype, ktype, kname, subkey) MAKE_ARRAY_INFO(ctype, ktype, kname, subkey, 0, KEY_TYPE_NULL)
#define MAKE_END_INFO()	{0, KEY_TYPE_NULL, NULL, 0, 0, NULL}


/**
 *@brief
 *@param kinfo 输入 结构体信息，用于识别结构体各成员
 *@param string 输入 要转化的字符串
 *@param obj 输出 结构体指针。如果为NULL，会自动分配内存，需要释放
 *
 *@return obj。失败时返回NULL
 *
 */
void *cjson_string2object(key_info_t *kinfo, char *string, void *obj);

/**
 *@brief
 *@param kinfo 输入 结构体信息，用于识别结构体各成员
 *@param obj 输入 要转化的结构体地址
 *
 *@return json格式的字符串。NULL if failed
 *
 *@note 其返回的字符串，需要用free释放
 *
 */
char *cjson_object2string(key_info_t *kinfo, void *obj);

#endif

