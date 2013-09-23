import zmq

class JRPC:
	def __init__(self):
		self.id = 0

	def make_noti(self, method, params=None):
		noti = {"jsonrpc":"2.0", "method":method}
		if params is not None:
			noti["params"] = params
		return noti

	def make_req(self, method, params=None):
		req = self.make_noti(method, params)
		req["id"] = self.id
		self.id += 1
		return req

zctx = zmq.Context.instance()
zsock = zctx.socket(zmq.REQ)
zsock.connect("tcp://127.0.0.1:10000")

jrpc = JRPC()

# the following tests are from http://www.jsonrpc.org/specification



# rpc call with positional parameters
req = jrpc.make_req("subtract", [42, 23])
zsock.send_json(req)
rep = zsock.recv_json()
assert(rep['id']==req['id'])
assert(rep['result']==19)

# rpc call with named parameters
req = jrpc.make_req("subtract", {"subtrahend":23, "minuend":42})
zsock.send_json(req)
rep = zsock.recv_json()
assert(rep['id']==req['id'])
assert(rep['result']==19)

# a Notification
req = jrpc.make_noti("update", [1,2,3,4,5])
zsock.send_json(req)
rep = zsock.recv()
assert not rep

# rpc call of non-existent method
req = jrpc.make_req("foobar")
zsock.send_json(req)
rep = zsock.recv_json()
assert(rep['id']==req['id'])
assert(rep['error']['code']==-32601)

# rpc call with invalid JSON
zsock.send('{"jsonrpc": "2.0", "method": "foobar, "params": "bar", "baz]')
rep = zsock.recv_json()
assert(rep['id']==None)
assert(rep['error']['code']==-32700)

# rpc call with invalid Request object
zsock.send_json({"jsonrpc": "2.0", "method": 1, "params": "bar"})
rep = zsock.recv_json()
assert(rep['id']==None)
assert(rep['error']['code']==-32600)

# rpc call Batch, invalid JSON
req = """[
	{"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
	{"jsonrpc": "2.0", "method"
]
"""
zsock.send(req)
rep = zsock.recv_json()
assert(rep['id']==None)
assert(rep['error']['code']==-32700)

# rpc call with an empty Array
zsock.send_json([])
rep = zsock.recv_json()
assert(rep['id']==None)
assert(rep['error']['code']==-32600)

# rpc call with an invalid Batch (but not empty)
zsock.send_json([1])
rep = zsock.recv_json()
assert(len(rep)==1)
assert(rep[0]['id']==None)
assert(rep[0]['error']['code']==-32600)

# rpc call with an invalid Batch
zsock.send_json([1,2,3])
batchrep = zsock.recv_json()
assert(len(batchrep)==3)
for rep in batchrep:
	assert(rep['id']==None)
	assert(rep['error']['code']==-32600)

# rpc call Batch
batchreq = []
batchreq.append(jrpc.make_req("sum", [1,2,4]))
batchreq.append(jrpc.make_noti("notify_hello", [7]))
batchreq.append(jrpc.make_req("subtract", [42, 23]))
batchreq.append({"foo": "boo"})
batchreq.append(jrpc.make_req("foo.get", {"name": "myself"}))
# we didn't implement method "get_data"
zsock.send_json(batchreq)
batchrep = zsock.recv_json()
assert(batchrep[0]['result']==7)
assert(batchrep[1]['result']==19)
assert(batchrep[2]['error']['code']==-32600)
assert(batchrep[3]['error']['code']==-32601)

# rpc call Batch (all notifications):
batchreq = []
batchreq.append(jrpc.make_noti("notify_sum", [1,2,4]))
batchreq.append(jrpc.make_noti("notify_hello", [7]))
zsock.send_json(batchreq)
rep = zsock.recv()
assert not rep

