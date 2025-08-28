/*
 * Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* The function IDs implemented in this TA */
#define TA_SAMPLE_CMD_INC_VALUE		0
#define TA_SAMPLE_CMD_DEC_VALUE		1
#define TA_SAMPLE_CMD_ECHO_STR		2

static void print_help() {
	printf("Usage: sample_ca [OPTIONS] [value]\n");
	printf("Options:\n");
	printf("  -i <value>      Increase value From TA\n");
	printf("  -d <value>      Decrease value From TA\n");
	printf("  -c <string>     Echo string From TA\n");
	printf("  -h              Show this help message\n");
}

int main(int argc, char **argv)
{
	void *handle;
	TEEC_Result (*TEEC_InitializeContext)(const char *name, TEEC_Context *context);
	TEEC_Result (*TEEC_OpenSession)(TEEC_Context *context,
                             TEEC_Session *session,
                             const TEEC_UUID *destination,
                             uint32_t connectionMethod,
                             const void *connectionData,
                             TEEC_Operation *operation,
                             uint32_t *returnOrigin);
	TEEC_Result (*TEEC_InvokeCommand)(TEEC_Session *session,
                               uint32_t commandID,
                               TEEC_Operation *operation,
                               uint32_t *returnOrigin);
	void (*TEEC_CloseSession)(TEEC_Session *session);
	void (*TEEC_FinalizeContext)(TEEC_Context *context);

	int value = 0;
	char *str = NULL;
	int cmd = 0;
	uint32_t commandID;

	if (argc != 3) {
		print_help();
		return 0;
	}

	if (0 == strcmp(argv[1], "-i")) {
		value = atoi(argv[2]);
		cmd = TA_SAMPLE_CMD_INC_VALUE;
	}
	else if (0 == strcmp(argv[1], "-d")) {
		value = atoi(argv[2]);
		cmd = TA_SAMPLE_CMD_DEC_VALUE;
	}
	else if (0 == strcmp(argv[1], "-c")) {
		str = argv[2];
		cmd = TA_SAMPLE_CMD_ECHO_STR;
	}
	else {
		print_help();
		return 0;
	}

	handle = dlopen("libteec.so", RTLD_LAZY);

	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid =  { 0xed53d67d, 0x4e58, 0x4b9a, \
		{ 0x85, 0x14, 0x0a, 0x61, 0xc3, 0xc9, 0x44, 0x01} };

	uint32_t err_origin;

	/* Initialize a context connecting us to the TEE */
	printf("Establish Context with OP-TEE!\n");
	TEEC_InitializeContext = dlsym(handle, "TEEC_InitializeContext");
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		printf("TEEC_InitializeContext failed with code 0x%x", res);

	/*
	 * Open a session to the Sample TA.
	 */
	printf("Establish Session with TA!\n");
	TEEC_OpenSession = dlsym(handle, "TEEC_OpenSession");
	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		printf("TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);

	/*
	 * Execute a function in the TA by invoking it
	 */

	/* Clear the TEEC_Operation struct */
	memset(&op, 0, sizeof(op));

	/*
	 * Prepare the argument. Pass a value in the first parameter,
	 * the remaining three parameters are unused.
	 */
	if (cmd == TA_SAMPLE_CMD_INC_VALUE || cmd == TA_SAMPLE_CMD_DEC_VALUE) {
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE,
			TEEC_NONE, TEEC_NONE);
		op.params[0].value.a = value;
	}
	else if (cmd == TA_SAMPLE_CMD_ECHO_STR) {
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_NONE,
			TEEC_NONE, TEEC_NONE);
		op.params[0].tmpref.buffer = str;
		op.params[0].tmpref.size = strlen(str);
	}

	commandID = (uint32_t)cmd;
	TEEC_InvokeCommand = dlsym(handle, "TEEC_InvokeCommand");
	res = TEEC_InvokeCommand(&sess, commandID, &op, &err_origin);
	if (res != TEEC_SUCCESS)
		printf("TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
			res, err_origin);

	if (cmd == TA_SAMPLE_CMD_INC_VALUE || cmd == TA_SAMPLE_CMD_DEC_VALUE) {
		printf("TA incremented value to %d\n", op.params[0].value.a);
	}
	else if (cmd == TA_SAMPLE_CMD_ECHO_STR) {
		printf("TA Echo: %s\n", (char *)op.params[0].tmpref.buffer);
	}

	/*
	 * We're done with the TA, close the session and
	 * destroy the context.
	 *
	 */

	printf("Disconnect Session with TA!\n");
	TEEC_CloseSession = dlsym(handle, "TEEC_CloseSession");
	TEEC_CloseSession(&sess);

	printf("Disconnect Context with OP-TEE!\n");
	TEEC_FinalizeContext = dlsym(handle, "TEEC_FinalizeContext");
	TEEC_FinalizeContext(&ctx);

	dlclose(handle);

	return 0;
}
