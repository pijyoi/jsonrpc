#include <czmq.h>
#include "jsonrpc.h"

static int method_echo(json_t *json_params, json_t **result)
{
	json_incref(json_params);
	*result = json_params;
	return 0;
}

static int method_subtract(json_t *json_params, json_t **result)
{
	size_t flags = 0;
	json_error_t error;
	double x, y;
	int rc;
	
	if (json_is_array(json_params)) {
		rc = json_unpack_ex(json_params, &error, flags, "[FF!]", &x, &y);
	} else if (json_is_object(json_params)) {
		rc = json_unpack_ex(json_params, &error, flags, "{s:F,s:F}",
			"minuend", &x, "subtrahend", &y
		);
	} else {
		assert(0);
	}

	if (rc==-1) {
		*result = jsonrpc_error_object(JSONRPC_INVALID_PARAMS, json_string(error.text));
		return JSONRPC_INVALID_PARAMS;
	}
	
	*result = json_real(x - y);
	return 0;
}

static int method_sum(json_t *json_params, json_t **result)
{
	double total = 0;
	size_t len = json_array_size(json_params);
	int k;
	for (k=0; k < len; k++) {
		double value = json_number_value(json_array_get(json_params, k));
		total += value;
	}
	*result = json_real(total);
	return 0;
}

static struct jsonrpc_method_entry_t method_table[] = {
	{ "echo", method_echo, "o" }, 
	{ "subtract", method_subtract, "o" }, 
	{ "sum", method_sum, "[]" }, 
	{ NULL },
};

int main()
{
	zctx_t *ctx = zctx_new();
	void *sock = zsocket_new(ctx, ZMQ_REP);
	int port = zsocket_bind(sock, "tcp://127.0.0.1:*");
	printf("bound to port %d\n", port);
	
	while (1) {
		zmsg_t *msg = zmsg_recv(sock);
		zframe_t *frame = zmsg_first(msg);
		
		char *output = jsonrpc_handler((char *)zframe_data(frame), zframe_size(frame), method_table);
		if (output) {
			zstr_send(sock, output);
			free(output);
		} else {
			zstr_send(sock, "");
		}
		
		zmsg_destroy(&msg);
	}
	
	zctx_destroy(&ctx);
	
	return 0;
}
