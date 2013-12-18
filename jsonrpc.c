#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <jansson.h>
#include "jsonrpc.h"

json_t *jsonrpc_error_object(int code, const char *message, json_t *data)
{
	/* reference to data is stolen */

	json_t *json;

	if (!message)
		message = "";

	json = json_pack("{s:i,s:s}", "code", code, "message", message);
	if (data) {
		json_object_set_new(json, "data", data);
	}
	return json;
}

json_t *jsonrpc_error_object_predefined(int code, json_t *data)
{
	/* reference to data is stolen */

	const char *message = "";

	assert(-32768 <= code && code <= -32000);	// reserved for pre-defined errors

	switch (code) {
		case JSONRPC_PARSE_ERROR:
			message = "Parse Error";
			break;
		case JSONRPC_INVALID_REQUEST:
			message = "Invalid Request";
			break;
		case JSONRPC_METHOD_NOT_FOUND:
			message = "Method not found";
			break;
		case JSONRPC_INVALID_PARAMS:
			message = "Invalid params";
			break;
		case JSONRPC_INTERNAL_ERROR:
			message = "Internal error";
			break;
	}

	return jsonrpc_error_object(code, message, data);
}

json_t *jsonrpc_error_response(json_t *json_id, json_t *json_error)
{
	/* json_error reference is stolen */

	json_t * response;

	/* json_id could be NULL */
	if (json_id) {
		json_incref(json_id);
	} else {
		json_id = json_null();
	}

	json_error = json_error ? json_error : json_null();

	response = json_pack("{s:s,s:o,s:o}",
		"jsonrpc", "2.0",
		"id", json_id,
		"error", json_error);
	return response;
}

json_t *jsonrpc_result_response(json_t *json_id, json_t *json_result)
{
	/*  json_result reference is stolen */

	json_t * response;

	/*  json_id shouldn't be NULL */
	if (json_id) {
		json_incref(json_id);
	} else {
		json_id = json_null();
	}

	json_result = json_result ? json_result : json_null();

	response = json_pack("{s:s,s:o,s:o}",
		"jsonrpc", "2.0",
		"id", json_id,
		"result", json_result);
	return response;
}

json_t *jsonrpc_validate_request(json_t *json_request, const char **str_method, json_t **json_params, json_t **json_id)
{
	size_t flags = 0;
	json_error_t error;
	const char *str_version = NULL;
	int rc;
	json_t *data = NULL;
	int valid_id = 0;

	*str_method = NULL;
	*json_params = NULL;
	*json_id = NULL;

	rc = json_unpack_ex(json_request, &error, flags, "{s:s,s:s,s?o,s?o}",
		"jsonrpc", &str_version,
		"method", str_method,
		"params", json_params,
		"id", json_id
	);
	if (rc==-1) {
		data = json_string(error.text);
		goto invalid;
	}

	if (0!=strcmp(str_version, "2.0")) {
		data = json_string("\"jsonrpc\" MUST be exactly \"2.0\"");
		goto invalid;
	}

	if (*json_id) {
		if (!json_is_string(*json_id) && !json_is_number(*json_id) && !json_is_null(*json_id)) {
			data = json_string("\"id\" MUST contain a String, Number, or NULL value if included");
			goto invalid;
		}
	}

	/*  Note that we only return json_id in the error response after we have established that it is jsonrpc/2.0 compliant */
	/*  otherwise we would be returning a non-compliant response ourselves! */
	valid_id = 1;

	if (*json_params) {
		if (!json_is_array(*json_params) && !json_is_object(*json_params)) {
			data = json_string("\"params\" MUST be Array or Object if included");
			goto invalid;
		}
	}

	return NULL;

invalid:
	if (!valid_id)
		*json_id = NULL;
	return jsonrpc_error_response(*json_id,
		jsonrpc_error_object_predefined(JSONRPC_INVALID_REQUEST, data));
}

json_t *jsonrpc_validate_params(json_t *json_params, const char *params_spec)
{
	json_t *data = NULL;

	if (strlen(params_spec)==0) {	/*  empty string means no arguments */
		if (!json_params) {
			/*  no params field: OK */
		} else if (json_is_array(json_params) && json_array_size(json_params)==0) {
			/*  an empty Array: OK */
		} else {
			data = json_string("method takes no arguments");
		}
	} else if (!json_params) {		/*  non-empty string but no params field */
		data = json_string("method takes arguments but params field missing");
	} else {					/*  non-empty string and have params field */
		size_t flags = JSON_VALIDATE_ONLY;
		json_error_t error;
		int rc = json_unpack_ex(json_params, &error, flags, params_spec);
		if (rc==-1) {
			data = json_string(error.text);
		}
	}

	return data ? jsonrpc_error_object_predefined(JSONRPC_INVALID_PARAMS, data) : NULL;
}

json_t *jsonrpc_handle_request_single(json_t *json_request, struct jsonrpc_method_entry_t method_table[],
	void *userdata)
{
	int rc;
	json_t *json_response;
	const char *str_method;
	json_t *json_params, *json_id;
	json_t *json_result;
	int is_notification;
	struct jsonrpc_method_entry_t *entry;

	json_response = jsonrpc_validate_request(json_request, &str_method, &json_params, &json_id);
	if (json_response)
		return json_response;

	is_notification = json_id==NULL;


	for (entry=method_table; entry->name!=NULL; entry++) {
		if (0==strcmp(entry->name, str_method))
			break;
	}
	if (entry->name==NULL) {
		json_response = jsonrpc_error_response(json_id,
				jsonrpc_error_object_predefined(JSONRPC_METHOD_NOT_FOUND, NULL));
		goto done;
	}

	if (entry->params_spec) {
		json_t *error_obj = jsonrpc_validate_params(json_params, entry->params_spec);
		if (error_obj) {
			json_response = jsonrpc_error_response(json_id, error_obj);
			goto done;
		}
	}

	json_response = NULL;
	json_result = NULL;
	rc = entry->funcptr(json_params, &json_result, userdata);
	if (is_notification) {
		json_decref(json_result);
		json_result = NULL;
	} else {
		if (rc==0) {
			json_response = jsonrpc_result_response(json_id, json_result);
		} else {
			if (!json_result) {
				/* method did not set a jsonrpc_error_object, create a generic error */
				json_result = jsonrpc_error_object_predefined(JSONRPC_INTERNAL_ERROR, NULL);
			}
			json_response = jsonrpc_error_response(json_id, json_result);
		}
	}

done:
	if (is_notification && json_response) {
		json_decref(json_response);
		json_response = NULL;
	}
	return json_response;
}

char *jsonrpc_handler(const char *input, size_t input_len, struct jsonrpc_method_entry_t method_table[],
	void *userdata)
{
	json_t *json_request, *json_response;
	json_error_t error;
	char *output = NULL;

	json_request = json_loadb(input, input_len, 0, &error);
	if (!json_request) {
		json_response = jsonrpc_error_response(NULL,
				jsonrpc_error_object_predefined(JSONRPC_PARSE_ERROR, NULL));
	} else if json_is_array(json_request) {
		size_t len = json_array_size(json_request);
		if (len==0) {
			json_response = jsonrpc_error_response(NULL,
					jsonrpc_error_object_predefined(JSONRPC_INVALID_REQUEST, NULL));
		} else {
			size_t k;
			json_response = NULL;
			for (k=0; k < len; k++) {
				json_t *req = json_array_get(json_request, k);
				json_t *rep = jsonrpc_handle_request_single(req, method_table, userdata);
				if (rep) {
					if (!json_response)
						json_response = json_array();
					json_array_append_new(json_response, rep);
				}
			}
		}
	} else {
		json_response = jsonrpc_handle_request_single(json_request, method_table, userdata);
	}

	if (json_response)
		output = json_dumps(json_response, JSON_INDENT(2));

	json_decref(json_request);
	json_decref(json_response);

	return output;
}

