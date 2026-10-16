/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * RIL wrapper gpsd links to. Without a modem it is a stub, like Samsung's
 * Wi-Fi library: open calls return an 8-byte handle, the rest succeed.
 */
#include <stdlib.h>

void *wrapperOpenClient_RILD(void)
{
	return malloc(8);
}

void *wrapperOpenClient_RILD_second(void)
{
	return malloc(8);
}

int wrapperCloseClient_RILD(void *client)
{
	(void)client;
	return 0;
}

int wrapperConnect_RILD(void *client)
{
	(void)client;
	return 0;
}

int wrapperConnect_RILD_second(void *client)
{
	(void)client;
	return 0;
}

int wrapperisConnected_RILD(void *client)
{
	(void)client;
	return 0;
}

int wrapperDisconnect_RILD(void *client)
{
	(void)client;
	return 0;
}

int wrapperRegisterUnsolicitedHandler(void *client, unsigned int id, void *handler)
{
	(void)client;
	(void)id;
	(void)handler;
	return 0;
}

int wrapperRegisterRequestCompleteHandler(void *client, unsigned int id, void *handler)
{
	(void)client;
	(void)id;
	(void)handler;
	return 0;
}

int wrapperRegisterErrorCallback(void *client, void *callback, void *data)
{
	(void)client;
	(void)callback;
	(void)data;
	return 0;
}

int wrapperInvokeOemRequestHookRaw(void *client, char *data, size_t len)
{
	(void)client;
	(void)data;
	(void)len;
	return 0;
}

int OemRequestHookRawLog(void *client, char *data)
{
	(void)client;
	(void)data;
	return 0;
}
