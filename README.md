jsonrpc.{ch} implements the parsing and validation of the jsonrpc/2.0 request before handing it over to 
user-defined methods.

jsonrpc_server.c is an example of usage using ZeroMQ as a transport.
It has been tested against ZeroMQ 3.2.2 and jansson 2.3.1


Since significant functionality is provided by jansson library, the same licence is chosen to be used by this code.
