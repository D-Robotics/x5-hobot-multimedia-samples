#ifndef _UAC_COMMON_H_
#define _UAC_COMMON_H_

#define UAC_FILE_PATH_LEN		128

enum uac_test_type_t{
	E_UAC_TEST_FILE = 0,        /** UAC play or record wav file */
	E_UAC_TEST_CARD,            /** UAC play or record through sound card */
	E_UAC_TEST_TAIL,
};

#endif